#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = [
#     "rich>=13.0",
# ]
# ///
"""
FERN_STYLE Checker - Enforces TigerBeetle-inspired coding standards.

Checks:
1. Minimum 2 assertions per function         [error]
2. Maximum 70 lines per function             [error]
3. No direct malloc/free usage (use arena)   [error]
4. No unbounded loops (must have limit)      [warning]
5. Function doc comments required            [warning]
   - doc-comment: Must have /** ... */ before function
   - doc-params:  Must have @param for each parameter
   - doc-return:  Must have @return for non-void functions
6. No raw char* params (use sds)             [warning]
7. No manual tagged unions (use Datatype99)  [warning]

Inline Exceptions:
    Add a comment in your function to suppress specific rules:

    // FERN_STYLE: allow(assertion-density) reason here
    // FERN_STYLE: allow(function-length) complex but cohesive logic
    // FERN_STYLE: allow(no-malloc) this IS the allocator
    // FERN_STYLE: allow(bounded-loops) loop bounded by input length
    // FERN_STYLE: allow(doc-comment) trivial getter
    // FERN_STYLE: allow(doc-params) params self-explanatory
    // FERN_STYLE: allow(doc-return) return obvious
    // FERN_STYLE: allow(doc-style) skip all doc style checks
    // FERN_STYLE: allow(no-raw-char) interop with C library
    // FERN_STYLE: allow(no-tagged-union) legacy code

Usage:
    uv run scripts/check_style.py src/ lib/
    uv run scripts/check_style.py --strict src/ lib/  # Treat warnings as errors
"""

import argparse
import re
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
    severity: str = "error"  # error, warning


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
    has_raw_char_params: bool = False  # char* in parameters (should use sds)
    doc_comment: DocComment = field(default_factory=DocComment)
    param_names: list = field(default_factory=list)
    return_type: str = ""
    allowed_rules: set = field(default_factory=set)  # Rules to skip via inline comments


def strip_string_literals(line: str) -> str:
    """Remove string literal contents to avoid counting braces inside strings.

    Handles "string" and 'char' literals, including escaped quotes.
    """
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
            # Inside string literal
            if c == "\\" and i + 1 < len(line):
                # Skip escaped character
                result.append(" ")  # Replace with space
                result.append(" ")
                i += 2
                continue
            elif c == string_char:
                in_string = False
                result.append(c)
            else:
                result.append(" ")  # Replace string content with space

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


# Pattern to match FERN_STYLE inline exception comments
# Examples:
#   // FERN_STYLE: allow(assertion-density) reason here
#   /* FERN_STYLE: allow(no-malloc, no-free) this is the allocator */
ALLOW_PATTERN = re.compile(r"(?://|/\*)\s*FERN_STYLE:\s*allow\(([^)]+)\)")


def parse_doc_comment(lines: list[str], func_line_idx: int) -> DocComment:
    """Parse documentation comment before a function.

    Looks for:
    - /** ... */ style (Doxygen/Javadoc)
    - /* ... */ style multi-line comments
    - /// style single-line doc comments

    Returns DocComment with details about what's documented.
    """
    result = DocComment()

    # Look backwards from function definition
    search_start = max(0, func_line_idx - 30)  # Allow larger comments

    for i in range(func_line_idx - 1, search_start - 1, -1):
        line = lines[i].strip()

        # Skip empty lines
        if not line:
            continue

        # Found a doc comment ending
        if line.endswith("*/"):
            # Look for the start of this comment
            for j in range(i, search_start - 1, -1):
                check_line = lines[j].strip()
                if check_line.startswith("/**") or check_line.startswith("/*"):
                    # Found the start - parse the comment
                    comment_lines = lines[j : i + 1]
                    comment_text = "\n".join(comment_lines)
                    result.raw_text = comment_text
                    result.exists = True

                    # Check for description (text after /** on first line or second line)
                    # Strip comment markers and check for content
                    content = (
                        comment_text.replace("/**", "")
                        .replace("/*", "")
                        .replace("*/", "")
                    )
                    content_lines = [
                        l.strip().lstrip("*").strip() for l in content.split("\n")
                    ]
                    # First non-empty line that's not a tag is the description
                    for cl in content_lines:
                        if cl and not cl.startswith("@"):
                            result.has_description = True
                            break

                    # Check for @param tags
                    for match in re.finditer(r"@param\s+(\w+)", comment_text):
                        result.documented_params.add(match.group(1))

                    # Check for @return or @returns
                    if re.search(r"@returns?\b", comment_text):
                        result.has_return = True

                    return result
            return result

        # /// style doc comment - collect consecutive /// lines
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

            for match in re.finditer(r"@param\s+(\w+)", comment_text):
                result.documented_params.add(match.group(1))

            if re.search(r"@returns?\b", comment_text):
                result.has_return = True

            return result

        # If we hit actual code (not a comment), stop looking
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
    # Split by comma, but handle function pointers
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

    # Extract parameter names (last word before optional array brackets)
    names = []
    for param in params:
        # Remove array brackets
        param = re.sub(r"\[[^\]]*\]", "", param)
        # Get last word (the parameter name)
        words = param.split()
        if words:
            # Handle pointer: "int *name" or "int* name"
            last = words[-1].lstrip("*")
            if last and last != "void":
                names.append(last)

    return names


def get_return_type(line: str) -> str:
    """Extract return type from function signature."""
    # Remove everything from ( onwards
    sig = re.sub(r"\(.*", "", line).strip()
    # Remove function name (last word)
    words = sig.split()
    if len(words) >= 2:
        return " ".join(words[:-1])
    return ""


def has_doc_comment_before(lines: list[str], func_line_idx: int) -> bool:
    """Check if there's a documentation comment before the function.

    Simple check - returns True if any doc comment exists.
    """
    doc = parse_doc_comment(lines, func_line_idx)
    return doc.exists and doc.has_description


def check_raw_char_params(line: str) -> bool:
    """Check if function has raw char* parameters (should use sds instead).

    Excludes:
    - const char* (acceptable for read-only strings from literals)
    - char (single char, not pointer)
    - argv patterns (main function)
    """
    # Extract the parameter list
    param_match = re.search(r"\(([^)]*)\)", line)
    if not param_match:
        return False

    params = param_match.group(1)

    # Skip main function
    if "argv" in params or "argc" in params:
        return False

    # Look for char* that isn't const char*
    # Pattern: char followed by optional spaces and *
    # But not preceded by "const "
    if re.search(r"(?<!const\s)char\s*\*", params):
        return True

    return False


def extract_functions(content: str, filepath: Path) -> list[Function]:
    """Extract function definitions from C source code."""
    functions = []
    lines = content.split("\n")

    # Simple heuristic: find function definitions
    # Pattern: type name(params) {
    func_pattern = re.compile(
        r"^(?:static\s+)?(?:inline\s+)?"  # Optional modifiers
        r"(?:[\w\*]+\s+)+"  # Return type
        r"(\w+)\s*"  # Function name (captured)
        r"\([^)]*\)\s*"  # Parameters
        r"\{"  # Opening brace
    )

    i = 0
    while i < len(lines):
        line = lines[i]

        # Skip preprocessor directives and comments
        if line.strip().startswith("#") or line.strip().startswith("//"):
            i += 1
            continue

        match = func_pattern.match(line)
        if match:
            func_name = match.group(1)
            start_line = i + 1  # 1-indexed

            # Parse doc comment before function
            doc_comment = parse_doc_comment(lines, i)
            has_doc = doc_comment.exists and doc_comment.has_description

            # Extract parameter names and return type
            param_names = extract_function_params(line)
            return_type = get_return_type(line)

            # Check for raw char* parameters
            has_raw_char = check_raw_char_params(line)

            # Find the matching closing brace
            brace_count = 1
            j = i + 1
            assertion_count = 0
            has_unbounded_loop = False
            has_malloc = False
            has_free = False
            allowed_rules = set()

            while j < len(lines) and brace_count > 0:
                current_line = lines[j]
                # Strip string literals to avoid counting braces inside strings
                stripped_line = strip_string_literals(current_line)
                brace_count += stripped_line.count("{") - stripped_line.count("}")

                # Check for FERN_STYLE allow comments
                allow_match = ALLOW_PATTERN.search(current_line)
                if allow_match:
                    rules = allow_match.group(1)
                    for rule in rules.split(","):
                        allowed_rules.add(rule.strip())

                # Count assertions
                if re.search(r"\bassert\s*\(", current_line):
                    assertion_count += 1

                # Check for unbounded loops
                # while(1), while(true), for(;;)
                if re.search(r"\bwhile\s*\(\s*1\s*\)", current_line):
                    has_unbounded_loop = True
                if re.search(r"\bwhile\s*\(\s*true\s*\)", current_line):
                    has_unbounded_loop = True
                if re.search(r"\bfor\s*\(\s*;\s*;\s*\)", current_line):
                    has_unbounded_loop = True
                # while without obvious bound check
                while_match = re.search(r"\bwhile\s*\(([^)]+)\)", current_line)
                if while_match:
                    condition = while_match.group(1)
                    # If no comparison operator, likely unbounded
                    if not re.search(
                        r"[<>=!]", condition
                    ) and condition.strip() not in ("0", "false"):
                        has_unbounded_loop = True

                # Check for malloc/free
                if re.search(r"\bmalloc\s*\(", current_line):
                    has_malloc = True
                if re.search(r"\bfree\s*\(", current_line):
                    has_free = True

                j += 1

            end_line = j  # 1-indexed

            # Also check the function signature line for allow comments
            allow_match = ALLOW_PATTERN.search(line)
            if allow_match:
                rules = allow_match.group(1)
                for rule in rules.split(","):
                    allowed_rules.add(rule.strip())

            functions.append(
                Function(
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
                )
            )

            i = j
        else:
            i += 1

    return functions


def check_manual_tagged_unions(content: str, filepath: Path) -> list[Violation]:
    """Check for manual tagged union patterns (should use Datatype99).

    Detects patterns like:
        typedef struct {
            enum { KIND_A, KIND_B } kind;
            union { ... } data;
        } MyType;

    Or struct with 'tag' or 'kind' field followed by union.
    """
    violations = []
    lines = content.split("\n")

    # Look for suspicious patterns
    for i, line in enumerate(lines):
        # Check for file-level allow comment
        if "FERN_STYLE: allow(no-tagged-union)" in content:
            break

        # Pattern 1: enum { ... } kind/tag/type inside struct
        if re.search(r"\benum\s*\{[^}]+\}\s*(kind|tag|type)\s*;", line):
            violations.append(
                Violation(
                    file=filepath,
                    line=i + 1,
                    function="",
                    rule="no-tagged-union",
                    message="Manual tagged union detected - use Datatype99 instead",
                    severity="warning",
                )
            )

        # Pattern 2: struct with .kind = or ->kind = assignment with union
        # This is harder to detect reliably, skip for now

    return violations


def check_file(filepath: Path) -> list[Violation]:
    """Check a single file for FERN_STYLE violations."""
    violations = []

    try:
        content = filepath.read_text()
    except Exception as e:
        violations.append(
            Violation(
                file=filepath,
                line=0,
                function="",
                rule="read-error",
                message=f"Could not read file: {e}",
            )
        )
        return violations

    # File-level checks
    violations.extend(check_manual_tagged_unions(content, filepath))

    functions = extract_functions(content, filepath)

    for func in functions:
        line_count = func.end_line - func.start_line

        # Rule 1: Minimum 2 assertions per function
        if func.assertion_count < 2 and "assertion-density" not in func.allowed_rules:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="assertion-density",
                    message=f"{func.assertion_count} assertions (need 2+)",
                )
            )

        # Rule 2: Maximum 70 lines per function
        if line_count > 70 and "function-length" not in func.allowed_rules:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="function-length",
                    message=f"{line_count} lines (max 70)",
                )
            )

        # Rule 3: No malloc/free (use arena)
        if func.has_malloc and "no-malloc" not in func.allowed_rules:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="no-malloc",
                    message="Uses malloc() - use arena allocation instead",
                )
            )

        if func.has_free and "no-free" not in func.allowed_rules:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="no-free",
                    message="Uses free() - use arena allocation instead",
                )
            )

        # Rule 4: No unbounded loops
        if func.has_unbounded_loop and "bounded-loops" not in func.allowed_rules:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="bounded-loops",
                    message="Unbounded loop detected - add explicit limit",
                    severity="warning",
                )
            )

        # Rule 5: Function documentation required
        if not func.has_doc_comment and "doc-comment" not in func.allowed_rules:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="doc-comment",
                    message="Missing doc comment (add /** ... */ before function)",
                    severity="warning",
                )
            )
        elif func.has_doc_comment and "doc-style" not in func.allowed_rules:
            # Check doc comment style/completeness
            doc = func.doc_comment

            # Check for missing @param
            missing_params = set(func.param_names) - doc.documented_params
            if missing_params and "doc-params" not in func.allowed_rules:
                violations.append(
                    Violation(
                        file=filepath,
                        line=func.start_line,
                        function=func.name,
                        rule="doc-params",
                        message=f"Missing @param for: {', '.join(sorted(missing_params))}",
                        severity="warning",
                    )
                )

            # Check for missing @return (only for non-void functions)
            if (
                func.return_type
                and func.return_type != "void"
                and "doc-return" not in func.allowed_rules
            ):
                if not doc.has_return:
                    violations.append(
                        Violation(
                            file=filepath,
                            line=func.start_line,
                            function=func.name,
                            rule="doc-return",
                            message="Missing @return for non-void function",
                            severity="warning",
                        )
                    )

        # Rule 6: No raw char* parameters (use sds)
        if func.has_raw_char_params and "no-raw-char" not in func.allowed_rules:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="no-raw-char",
                    message="Raw char* parameter - use sds or const char* instead",
                    severity="warning",
                )
            )

    return violations


def print_violations(violations: list[Violation], by_file: bool = True) -> None:
    """Print violations in a readable format using rich."""
    if not violations:
        console.print(
            Panel(
                "[bold green]All files pass FERN_STYLE checks[/]",
                border_style="green",
                box=box.ROUNDED,
            )
        )
        return

    if by_file:
        # Group by file
        files: dict[Path, list[Violation]] = {}
        for v in violations:
            files.setdefault(v.file, []).append(v)

        for filepath, file_violations in sorted(files.items()):
            table = Table(
                title=str(filepath),
                title_style="bold white",
                box=box.SIMPLE,
                show_header=True,
                header_style="bold cyan",
                padding=(0, 1),
            )
            table.add_column("", width=2)  # Status icon
            table.add_column("Function", style="yellow")
            table.add_column("Line", justify="right", style="dim")
            table.add_column("Rule", style="magenta")
            table.add_column("Message")

            for v in file_violations:
                if v.severity == "error":
                    icon = "[bold red]X[/]"
                    msg_style = "red"
                else:
                    icon = "[bold yellow]![/]"
                    msg_style = "yellow"

                func_name = f"{v.function}()" if v.function else "file"
                table.add_row(
                    icon,
                    func_name,
                    str(v.line),
                    v.rule,
                    Text(v.message, style=msg_style),
                )

            console.print(table)
            console.print()

    # Summary
    errors = sum(1 for v in violations if v.severity == "error")
    warnings = sum(1 for v in violations if v.severity == "warning")
    files_affected = len(set(v.file for v in violations))

    summary_parts = []
    if errors > 0:
        summary_parts.append(f"[bold red]{errors} error{'s' if errors != 1 else ''}[/]")
    if warnings > 0:
        summary_parts.append(
            f"[bold yellow]{warnings} warning{'s' if warnings != 1 else ''}[/]"
        )

    summary_text = (
        ", ".join(summary_parts)
        + f" in [bold]{files_affected}[/] file{'s' if files_affected != 1 else ''}"
    )

    console.print(
        Panel(
            summary_text,
            title="Summary",
            border_style="red" if errors > 0 else "yellow",
            box=box.ROUNDED,
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check C code for FERN_STYLE compliance",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
FERN_STYLE Rules (TigerBeetle-inspired):
  1. Minimum 2 assertions per function       [error]
  2. Maximum 70 lines per function           [error]
  3. No malloc/free (use arena allocation)   [error]
  4. No unbounded loops (add explicit limits)[warning]
  5. Function doc comments required          [warning]
     - doc-comment: must have /** ... */
     - doc-params:  must have @param for each param
     - doc-return:  must have @return if non-void
  6. No raw char* params (use sds)           [warning]
  7. No manual tagged unions (use Datatype99)[warning]

Inline Exceptions:
  Add a comment inside the function to suppress specific rules:
    // FERN_STYLE: allow(assertion-density) simple helper
    // FERN_STYLE: allow(no-malloc, no-free) this IS the allocator
    // FERN_STYLE: allow(doc-comment) trivial getter
    // FERN_STYLE: allow(doc-style) skip all doc checks
    // FERN_STYLE: allow(no-raw-char) C library interop
    // FERN_STYLE: allow(no-tagged-union) legacy code

Examples:
  uv run %(prog)s src/ lib/           Check all .c files
  uv run %(prog)s src/lexer.c         Check specific file
  uv run %(prog)s --summary src/      Show only summary
  uv run %(prog)s --strict src/       Treat warnings as errors
        """,
    )
    parser.add_argument(
        "paths",
        nargs="+",
        help="Directories or files to check",
    )
    parser.add_argument(
        "--summary",
        action="store_true",
        help="Show only summary, not individual violations",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Treat warnings as errors",
    )

    args = parser.parse_args()

    all_violations: list[Violation] = []
    files_checked = 0

    console.print("[bold]Checking FERN_STYLE compliance...[/]\n")

    for filepath in find_c_files(args.paths):
        files_checked += 1
        violations = check_file(filepath)
        all_violations.extend(violations)

    if files_checked == 0:
        console.print("[yellow]No .c files found to check[/]")
        return 0

    console.print(f"Checked [bold]{files_checked}[/] files\n")

    if not args.summary:
        print_violations(all_violations)
    else:
        errors = sum(1 for v in all_violations if v.severity == "error")
        warnings = sum(1 for v in all_violations if v.severity == "warning")
        files_affected = len(set(v.file for v in all_violations))

        if all_violations:
            console.print(
                f"[red]{errors} errors, {warnings} warnings in {files_affected} files[/]"
            )
        else:
            console.print("[green]All files pass FERN_STYLE checks[/]")

    # Exit code
    errors = sum(1 for v in all_violations if v.severity == "error")
    warnings = sum(1 for v in all_violations if v.severity == "warning")

    if args.strict:
        return 1 if (errors + warnings) > 0 else 0
    return 1 if errors > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
