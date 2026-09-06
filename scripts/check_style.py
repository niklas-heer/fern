#!/usr/bin/env -S uv run --locked --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#     "rich>=13.0",
# ]
# ///
"""
Fern Quality Checker - Consolidated pre-commit and style checker.

This script combines ALL quality checks into one place:
1. Build verification (clean compile with no warnings)
2. Unit tests (mise run test)
3. Examples testing (type-check all examples/*.fn files)
4. FERN_STYLE code compliance

Default mode is STRICT - all warnings are treated as errors.
Use --lenient to allow warnings during development.

Usage:
    uv run scripts/check_style.py                   # Full check (strict)
    uv run scripts/check_style.py --lenient         # Allow warnings
    uv run scripts/check_style.py --style-only      # Only FERN_STYLE checks
    uv run scripts/check_style.py --pre-commit      # Pre-commit hook mode

FERN_STYLE Rules (all errors in strict mode):
  1. Minimum 2 assertions per function
  2. Maximum 70 lines per function
  3. No direct malloc/free usage (use arena)
  4. No unbounded loops (must have limit)
  5. Function doc comments required (@param, @return)
  6. No raw char* params (use sds or const char*)
  7. No manual tagged unions (use Datatype99)

Inline Exceptions:
    // FERN_STYLE: allow(assertion-density) reason here
    // FERN_STYLE: allow(function-length) complex but cohesive
    // FERN_STYLE: allow(no-malloc) this IS the allocator
"""

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator

from rich import box
from rich.console import Console
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

console = Console()


# ============================================================================
# Data Classes
# ============================================================================


@dataclass
class DocComment:
    """Parsed documentation comment."""
    exists: bool = False
    has_description: bool = False
    documented_params: set = field(default_factory=set)
    has_return: bool = False
    raw_text: str = ""


@dataclass
class Violation:
    """A FERN_STYLE violation."""
    file: Path
    line: int
    function: str
    rule: str
    message: str
    severity: str = "error"


@dataclass
class Function:
    """A parsed C function."""
    name: str
    start_line: int
    end_line: int
    assertion_count: int
    has_unbounded_loop: bool
    has_malloc: bool
    has_free: bool
    has_doc_comment: bool = False
    has_raw_char_params: bool = False
    doc_comment: DocComment = field(default_factory=DocComment)
    param_names: list = field(default_factory=list)
    return_type: str = ""
    allowed_rules: set = field(default_factory=set)


@dataclass
class CheckResult:
    """Result of a check operation."""
    success: bool
    message: str
    details: str = ""


# ============================================================================
# Helper Functions
# ============================================================================


def run_command(cmd: list[str], capture: bool = True) -> tuple[int, str, str]:
    """Run a command and return exit code, stdout, stderr."""
    try:
        result = subprocess.run(cmd, capture_output=capture, text=True, timeout=300)
        return result.returncode, result.stdout or "", result.stderr or ""
    except subprocess.TimeoutExpired:
        return 1, "", "Command timed out"
    except Exception as e:
        return 1, "", str(e)


def strip_string_literals(line: str) -> str:
    """Remove string literal contents to avoid counting braces inside strings."""
    result = []
    i = 0
    in_string = False
    string_char = None

    while i < len(line):
        c = line[i]
        if not in_string:
            if c == '"' or c == "'":
                in_string = True
                string_char = c
                result.append(c)
            else:
                result.append(c)
        else:
            if c == "\\" and i + 1 < len(line):
                result.append(" ")
                result.append(" ")
                i += 2
                continue
            elif c == string_char:
                in_string = False
                result.append(c)
            else:
                result.append(" ")
        i += 1

    return "".join(result)


def find_c_files(directories: list[str]) -> Iterator[Path]:
    """Find all .c files in the given directories."""
    for directory in directories:
        path = Path(directory)
        if path.is_file() and path.suffix == ".c":
            yield path
        elif path.is_dir():
            yield from path.rglob("*.c")


ALLOW_PATTERN = re.compile(r"(?://|/\*)\s*FERN_STYLE:\s*allow\(([^)]+)\)")


# ============================================================================
# Build and Test Functions
# ============================================================================


def check_build() -> CheckResult:
    """Run clean build and check for errors/warnings."""
    code, _, _ = run_command(["mise", "run", "clean"])
    if code != 0:
        return CheckResult(False, "Clean failed", "")

    code, stdout, stderr = run_command(["mise", "run", "debug"])
    output = stdout + stderr

    if code != 0:
        return CheckResult(False, "Build failed", output)

    if "warning:" in output.lower() or "error:" in output.lower():
        return CheckResult(False, "Build has warnings/errors", output)

    return CheckResult(True, "Build clean", "")


def check_tests() -> CheckResult:
    """Run test suite."""
    code, stdout, stderr = run_command(["mise", "run", "test"])
    output = stdout + stderr

    if code != 0:
        return CheckResult(False, "Tests failed", output)

    match = re.search(r"Passed:\s*(\d+)", output)
    if match:
        passed = match.group(1)
        return CheckResult(True, f"All tests passing ({passed} tests)", "")

    return CheckResult(True, "Tests passed", "")


def check_examples() -> CheckResult:
    """Type-check all .fn example files."""
    examples_dir = Path("examples")
    if not examples_dir.exists():
        return CheckResult(True, "No examples directory", "")

    fn_files = list(examples_dir.glob("*.fn"))
    if not fn_files:
        return CheckResult(True, "No .fn files in examples/", "")

    fern_bin = Path("bin/fern")
    if not fern_bin.exists():
        return CheckResult(False, "Fern compiler not built", "Run mise run debug first")

    failed = []
    passed = 0

    for fn_file in sorted(fn_files):
        code, stdout, stderr = run_command([str(fern_bin), "check", str(fn_file)])
        if code != 0:
            failed.append((fn_file.name, stderr or stdout))
        else:
            passed += 1

    if failed:
        details = "\n".join(f"  {name}: {err[:100]}" for name, err in failed[:5])
        return CheckResult(
            False,
            f"{len(failed)}/{len(fn_files)} examples failed type check",
            details
        )

    return CheckResult(True, f"All {passed} examples type-check", "")


def check_git_hygiene() -> list[str]:
    """Check for git commit hygiene issues."""
    warnings = []

    code, _, _ = run_command(["git", "rev-parse", "--git-dir"])
    if code != 0:
        return warnings

    code, stdout, _ = run_command(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACM"]
    )
    if code != 0:
        return warnings

    staged_files = stdout.strip().split("\n") if stdout.strip() else []
    code_files = [f for f in staged_files if f.endswith((".c", ".h"))]
    if not code_files:
        return warnings

    if any("feat" in f.lower() or "feature" in f.lower() for f in staged_files):
        if "ROADMAP.md" not in staged_files:
            warnings.append("Reminder: Consider updating ROADMAP.md for feature changes")

    if any(f.startswith("include/") or f == "DESIGN.md" for f in staged_files):
        if "DECISIONS.md" not in staged_files:
            warnings.append("Reminder: Did you make a design decision? Consider /decision")

    return warnings


# ============================================================================
# FERN_STYLE Checking
# ============================================================================


def parse_doc_comment(lines: list[str], func_line_idx: int) -> DocComment:
    """Parse documentation comment before a function."""
    result = DocComment()
    search_start = max(0, func_line_idx - 30)

    for i in range(func_line_idx - 1, search_start - 1, -1):
        line = lines[i].strip()
        if not line:
            continue

        if line.endswith("*/"):
            for j in range(i, search_start - 1, -1):
                check_line = lines[j].strip()
                if check_line.startswith("/**") or check_line.startswith("/*"):
                    comment_lines = lines[j : i + 1]
                    comment_text = "\n".join(comment_lines)
                    result.raw_text = comment_text
                    result.exists = True

                    content = comment_text.replace("/**", "").replace("/*", "").replace("*/", "")
                    content_lines = [l.strip().lstrip("*").strip() for l in content.split("\n")]
                    for cl in content_lines:
                        if cl and not cl.startswith("@"):
                            result.has_description = True
                            break

                    for match in re.finditer(r"@param\s+(\w+|\.\.\.)", comment_text):
                        result.documented_params.add(match.group(1))

                    if re.search(r"@returns?\b", comment_text):
                        result.has_return = True

                    return result
            return result

        if line.startswith("///"):
            doc_lines = [line]
            for k in range(i - 1, search_start - 1, -1):
                prev_line = lines[k].strip()
                if prev_line.startswith("///"):
                    doc_lines.insert(0, prev_line)
                elif not prev_line:
                    continue
                else:
                    break

            comment_text = "\n".join(doc_lines)
            result.raw_text = comment_text
            result.exists = True

            content = comment_text.replace("///", "")
            if len(content.strip()) > 5:
                result.has_description = True

            for match in re.finditer(r"@param\s+(\w+|\.\.\.)", comment_text):
                result.documented_params.add(match.group(1))

            if re.search(r"@returns?\b", comment_text):
                result.has_return = True

            return result

        if not line.startswith("//") and not line.startswith("*"):
            return result

    return result


def extract_function_params(line: str) -> list[str]:
    """Extract parameter names from a function signature."""
    param_match = re.search(r"\(([^)]*)\)", line)
    if not param_match:
        return []

    params_str = param_match.group(1).strip()
    if not params_str or params_str == "void":
        return []

    params = []
    depth = 0
    current = ""
    for char in params_str:
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "," and depth == 0:
            params.append(current.strip())
            current = ""
            continue
        current += char
    if current.strip():
        params.append(current.strip())

    names = []
    for param in params:
        if param.strip() == "...":
            names.append("...")
            continue
        param = re.sub(r"\[[^\]]*\]", "", param)
        words = param.split()
        if words:
            last = words[-1].lstrip("*")
            if last and last != "void":
                names.append(last)

    return names


def get_return_type(line: str) -> str:
    """Extract return type from function signature."""
    sig = re.sub(r"\(.*", "", line).strip()
    words = sig.split()
    if len(words) >= 2:
        return " ".join(words[:-1])
    return ""


def check_raw_char_params(line: str) -> bool:
    """Check if function has raw char* parameters."""
    param_match = re.search(r"\(([^)]*)\)", line)
    if not param_match:
        return False

    params = param_match.group(1)
    if "argv" in params or "argc" in params:
        return False

    if re.search(r"(?<!const\s)char\s*\*", params):
        return True

    return False


def extract_functions(content: str, filepath: Path) -> list[Function]:
    """Extract function definitions from C source code."""
    functions = []
    lines = content.split("\n")

    func_pattern = re.compile(
        r"^(?:static\s+)?(?:inline\s+)?"
        r"(?:[\w\*]+\s+)+"
        r"(\w+)\s*"
        r"\([^)]*\)\s*"
        r"\{"
    )

    i = 0
    while i < len(lines):
        line = lines[i]

        if line.strip().startswith("#") or line.strip().startswith("//"):
            i += 1
            continue

        match = func_pattern.match(line)
        if match:
            func_name = match.group(1)
            start_line = i + 1

            doc_comment = parse_doc_comment(lines, i)
            has_doc = doc_comment.exists and doc_comment.has_description
            param_names = extract_function_params(line)
            return_type = get_return_type(line)
            has_raw_char = check_raw_char_params(line)

            brace_count = 1
            j = i + 1
            assertion_count = 0
            has_unbounded_loop = False
            has_malloc = False
            has_free = False
            allowed_rules = set()

            while j < len(lines) and brace_count > 0:
                current_line = lines[j]
                stripped_line = strip_string_literals(current_line)
                brace_count += stripped_line.count("{") - stripped_line.count("}")

                allow_match = ALLOW_PATTERN.search(current_line)
                if allow_match:
                    for rule in allow_match.group(1).split(","):
                        allowed_rules.add(rule.strip())

                if re.search(r"\bassert\s*\(", current_line):
                    assertion_count += 1

                if re.search(r"\bwhile\s*\(\s*1\s*\)", current_line):
                    has_unbounded_loop = True
                if re.search(r"\bwhile\s*\(\s*true\s*\)", current_line):
                    has_unbounded_loop = True
                if re.search(r"\bfor\s*\(\s*;\s*;\s*\)", current_line):
                    has_unbounded_loop = True
                while_match = re.search(r"\bwhile\s*\(([^)]+)\)", current_line)
                if while_match:
                    condition = while_match.group(1)
                    if not re.search(r"[<>=!]", condition) and condition.strip() not in ("0", "false"):
                        has_unbounded_loop = True

                if re.search(r"\bmalloc\s*\(", current_line):
                    has_malloc = True
                if re.search(r"\bfree\s*\(", current_line):
                    has_free = True

                j += 1

            end_line = j

            allow_match = ALLOW_PATTERN.search(line)
            if allow_match:
                for rule in allow_match.group(1).split(","):
                    allowed_rules.add(rule.strip())

            functions.append(Function(
                name=func_name,
                start_line=start_line,
                end_line=end_line,
                assertion_count=assertion_count,
                has_unbounded_loop=has_unbounded_loop,
                has_malloc=has_malloc,
                has_free=has_free,
                has_doc_comment=has_doc,
                has_raw_char_params=has_raw_char,
                doc_comment=doc_comment,
                param_names=param_names,
                return_type=return_type,
                allowed_rules=allowed_rules,
            ))

            i = j
        else:
            i += 1

    return functions


def check_manual_tagged_unions(content: str, filepath: Path) -> list[Violation]:
    """Check for manual tagged union patterns."""
    violations = []
    lines = content.split("\n")

    for i, line in enumerate(lines):
        if "FERN_STYLE: allow(no-tagged-union)" in content:
            break

        if re.search(r"\benum\s*\{[^}]+\}\s*(kind|tag|type)\s*;", line):
            violations.append(Violation(
                file=filepath,
                line=i + 1,
                function="",
                rule="no-tagged-union",
                message="Manual tagged union detected - use Datatype99 instead",
                severity="warning",
            ))

    return violations


def check_file(filepath: Path, strict: bool = True) -> list[Violation]:
    """Check a single file for FERN_STYLE violations."""
    violations = []

    try:
        content = filepath.read_text()
    except Exception as e:
        violations.append(Violation(
            file=filepath, line=0, function="", rule="read-error",
            message=f"Could not read file: {e}",
        ))
        return violations

    violations.extend(check_manual_tagged_unions(content, filepath))
    functions = extract_functions(content, filepath)

    for func in functions:
        line_count = func.end_line - func.start_line

        if func.assertion_count < 2 and "assertion-density" not in func.allowed_rules:
            violations.append(Violation(
                file=filepath, line=func.start_line, function=func.name,
                rule="assertion-density",
                message=f"{func.assertion_count} assertions (need 2+)",
            ))

        if line_count > 70 and "function-length" not in func.allowed_rules:
            violations.append(Violation(
                file=filepath, line=func.start_line, function=func.name,
                rule="function-length",
                message=f"{line_count} lines (max 70)",
            ))

        if func.has_malloc and "no-malloc" not in func.allowed_rules:
            violations.append(Violation(
                file=filepath, line=func.start_line, function=func.name,
                rule="no-malloc",
                message="Uses malloc() - use arena allocation instead",
            ))

        if func.has_free and "no-free" not in func.allowed_rules:
            violations.append(Violation(
                file=filepath, line=func.start_line, function=func.name,
                rule="no-free",
                message="Uses free() - use arena allocation instead",
            ))

        if func.has_unbounded_loop and "bounded-loops" not in func.allowed_rules:
            violations.append(Violation(
                file=filepath, line=func.start_line, function=func.name,
                rule="bounded-loops",
                message="Unbounded loop detected - add explicit limit",
                severity="error" if strict else "warning",
            ))

        if not func.has_doc_comment and "doc-comment" not in func.allowed_rules:
            violations.append(Violation(
                file=filepath, line=func.start_line, function=func.name,
                rule="doc-comment",
                message="Missing doc comment (add /** ... */ before function)",
                severity="error" if strict else "warning",
            ))
        elif func.has_doc_comment and "doc-style" not in func.allowed_rules:
            doc = func.doc_comment

            missing_params = set(func.param_names) - doc.documented_params
            if missing_params and "doc-params" not in func.allowed_rules:
                violations.append(Violation(
                    file=filepath, line=func.start_line, function=func.name,
                    rule="doc-params",
                    message=f"Missing @param for: {', '.join(sorted(missing_params))}",
                    severity="error" if strict else "warning",
                ))

            if (func.return_type and not func.return_type.endswith("void")
                    and "doc-return" not in func.allowed_rules and not doc.has_return):
                violations.append(Violation(
                    file=filepath, line=func.start_line, function=func.name,
                    rule="doc-return",
                    message="Missing @return for non-void function",
                    severity="error" if strict else "warning",
                ))

        if func.has_raw_char_params and "no-raw-char" not in func.allowed_rules:
            violations.append(Violation(
                file=filepath, line=func.start_line, function=func.name,
                rule="no-raw-char",
                message="Raw char* parameter - use sds or const char* instead",
                severity="error" if strict else "warning",
            ))

    return violations


# ============================================================================
# Output Functions
# ============================================================================


def print_violations(violations: list[Violation]) -> None:
    """Print violations in a readable format."""
    if not violations:
        console.print(Panel(
            "[bold green]All files pass FERN_STYLE checks[/]",
            border_style="green", box=box.ROUNDED,
        ))
        return

    files: dict[Path, list[Violation]] = {}
    for v in violations:
        files.setdefault(v.file, []).append(v)

    for filepath, file_violations in sorted(files.items()):
        table = Table(
            title=str(filepath), title_style="bold white",
            box=box.SIMPLE, show_header=True, header_style="bold cyan", padding=(0, 1),
        )
        table.add_column("", width=2)
        table.add_column("Function", style="yellow")
        table.add_column("Line", justify="right", style="dim")
        table.add_column("Rule", style="magenta")
        table.add_column("Message")

        for v in file_violations:
            icon = "[bold red]X[/]" if v.severity == "error" else "[bold yellow]![/]"
            msg_style = "red" if v.severity == "error" else "yellow"
            func_name = f"{v.function}()" if v.function else "file"
            table.add_row(icon, func_name, str(v.line), v.rule, Text(v.message, style=msg_style))

        console.print(table)
        console.print()

    errors = sum(1 for v in violations if v.severity == "error")
    warnings = sum(1 for v in violations if v.severity == "warning")
    files_affected = len(set(v.file for v in violations))

    parts = []
    if errors > 0:
        parts.append(f"[bold red]{errors} error{'s' if errors != 1 else ''}[/]")
    if warnings > 0:
        parts.append(f"[bold yellow]{warnings} warning{'s' if warnings != 1 else ''}[/]")

    console.print(Panel(
        ", ".join(parts) + f" in [bold]{files_affected}[/] file{'s' if files_affected != 1 else ''}",
        title="Summary", border_style="red" if errors > 0 else "yellow", box=box.ROUNDED,
    ))


def print_check_result(name: str, result: CheckResult) -> None:
    """Print a single check result."""
    if result.success:
        console.print(f"[green]  [bold]OK[/bold][/green]   {name}: {result.message}")
    else:
        console.print(f"[red]  [bold]FAIL[/bold][/red] {name}: {result.message}")
        if result.details:
            console.print(f"[dim]{result.details}[/dim]")


# ============================================================================
# Main
# ============================================================================


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Fern Quality Checker - Build, Test, Examples, and FERN_STYLE",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Modes:
  Default (strict)    Run all checks, warnings are errors
  --lenient           Run all checks, allow warnings
  --style-only        Only FERN_STYLE checks (skip build/test/examples)
  --pre-commit        Pre-commit hook mode (includes git hygiene)

All Checks Run:
  1. Build            mise run clean && mise run debug (no warnings)
  2. Tests            mise run test (all must pass)
  3. Examples         Type-check all examples/*.fn files
  4. FERN_STYLE       Code compliance checks
""",
    )
    parser.add_argument("paths", nargs="*", default=["src", "lib"],
                        help="Directories or files to check (default: src lib)")
    parser.add_argument("--lenient", action="store_true",
                        help="Allow warnings (default is strict mode)")
    parser.add_argument("--style-only", action="store_true",
                        help="Only run FERN_STYLE checks (skip build/test/examples)")
    parser.add_argument("--pre-commit", action="store_true",
                        help="Pre-commit hook mode (full strict check + git hygiene)")
    parser.add_argument("--summary", action="store_true",
                        help="Show only summary, not individual violations")

    args = parser.parse_args()
    strict = not args.lenient

    console.print(Panel(
        "[bold]Fern Quality Checker[/]",
        subtitle="[dim]strict mode[/]" if strict else "[dim]lenient mode[/]",
        border_style="blue",
    ))
    console.print()

    all_passed = True

    # Build, test, examples (unless style-only)
    if not args.style_only:
        console.print("[bold cyan]Build & Test[/]\n")

        build_result = check_build()
        print_check_result("Build", build_result)
        if not build_result.success:
            all_passed = False

        test_result = check_tests()
        print_check_result("Tests", test_result)
        if not test_result.success:
            all_passed = False

        examples_result = check_examples()
        print_check_result("Examples", examples_result)
        if not examples_result.success:
            all_passed = False

        console.print()

    # FERN_STYLE check
    console.print("[bold cyan]FERN_STYLE Compliance[/]\n")

    all_violations: list[Violation] = []
    files_checked = 0

    for filepath in find_c_files(args.paths):
        files_checked += 1
        violations = check_file(filepath, strict=strict)
        all_violations.extend(violations)

    if files_checked == 0:
        console.print("[yellow]No .c files found to check[/]")
    else:
        console.print(f"Checked [bold]{files_checked}[/] files\n")

        if not args.summary:
            print_violations(all_violations)
        else:
            errors = sum(1 for v in all_violations if v.severity == "error")
            warnings = sum(1 for v in all_violations if v.severity == "warning")
            if all_violations:
                console.print(f"[red]{errors} errors, {warnings} warnings[/]")
            else:
                console.print("[green]All files pass FERN_STYLE checks[/]")

    errors = sum(1 for v in all_violations if v.severity == "error")
    warnings = sum(1 for v in all_violations if v.severity == "warning")

    if strict:
        if errors + warnings > 0:
            all_passed = False
    else:
        if errors > 0:
            all_passed = False

    # Git hygiene (pre-commit mode only)
    if args.pre_commit:
        console.print("\n[bold cyan]Git Hygiene[/]\n")
        git_warnings = check_git_hygiene()
        if git_warnings:
            for w in git_warnings:
                console.print(f"[yellow]  [bold]![/bold][/yellow]  {w}")
        else:
            console.print("[green]  [bold]OK[/bold][/green]   No issues detected")

    # Final summary
    console.print()
    if all_passed:
        console.print(Panel(
            "[bold green]All checks passed![/]",
            border_style="green", box=box.ROUNDED,
        ))
        return 0
    else:
        console.print(Panel(
            "[bold red]Checks failed - fix issues before committing[/]",
            border_style="red", box=box.ROUNDED,
        ))
        return 1


if __name__ == "__main__":
    sys.exit(main())
