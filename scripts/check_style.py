#!/usr/bin/env python3
"""
FERN_STYLE Checker - Enforces TigerBeetle-inspired coding standards.

Checks:
1. Minimum 2 assertions per function
2. Maximum 70 lines per function
3. No direct malloc/free usage (use arena)
4. No unbounded loops (must have explicit limit)

Usage:
    python3 scripts/check_style.py src/ lib/
    python3 scripts/check_style.py --fix src/ lib/  # Add TODO comments
"""

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator


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

            # Find the matching closing brace
            brace_count = 1
            j = i + 1
            assertion_count = 0
            has_unbounded_loop = False
            has_malloc = False
            has_free = False

            while j < len(lines) and brace_count > 0:
                current_line = lines[j]
                # Strip string literals to avoid counting braces inside strings
                stripped_line = strip_string_literals(current_line)
                brace_count += stripped_line.count("{") - stripped_line.count("}")

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

            functions.append(
                Function(
                    name=func_name,
                    start_line=start_line,
                    end_line=end_line,
                    assertion_count=assertion_count,
                    has_unbounded_loop=has_unbounded_loop,
                    has_malloc=has_malloc,
                    has_free=has_free,
                )
            )

            i = j
        else:
            i += 1

    return functions


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

    functions = extract_functions(content, filepath)

    for func in functions:
        line_count = func.end_line - func.start_line

        # Rule 1: Minimum 2 assertions per function
        if func.assertion_count < 2:
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
        if line_count > 70:
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
        if func.has_malloc:
            violations.append(
                Violation(
                    file=filepath,
                    line=func.start_line,
                    function=func.name,
                    rule="no-malloc",
                    message="Uses malloc() - use arena allocation instead",
                )
            )

        if func.has_free:
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
        if func.has_unbounded_loop:
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

    return violations


def print_violations(violations: list[Violation], by_file: bool = True) -> None:
    """Print violations in a readable format."""
    if not violations:
        print("\033[32m✓ All files pass FERN_STYLE checks\033[0m")
        return

    if by_file:
        # Group by file
        files: dict[Path, list[Violation]] = {}
        for v in violations:
            files.setdefault(v.file, []).append(v)

        for filepath, file_violations in sorted(files.items()):
            print(f"\n\033[1m{filepath}:\033[0m")
            for v in file_violations:
                color = "\033[31m" if v.severity == "error" else "\033[33m"
                symbol = "✗" if v.severity == "error" else "⚠"
                func_str = f"{v.function}()" if v.function else "file"
                print(
                    f"  {color}{symbol}\033[0m {func_str} line {v.line} - {v.message}"
                )

    # Summary
    errors = sum(1 for v in violations if v.severity == "error")
    warnings = sum(1 for v in violations if v.severity == "warning")
    files_affected = len(set(v.file for v in violations))

    print(
        f"\n\033[1mSummary:\033[0m {errors} errors, {warnings} warnings in {files_affected} files"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check C code for FERN_STYLE compliance",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
FERN_STYLE Rules (TigerBeetle-inspired):
  1. Minimum 2 assertions per function
  2. Maximum 70 lines per function
  3. No malloc/free (use arena allocation)
  4. No unbounded loops (add explicit limits)

Examples:
  %(prog)s src/ lib/           Check all .c files
  %(prog)s src/lexer.c         Check specific file
  %(prog)s --summary src/      Show only summary
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

    print("Checking FERN_STYLE compliance...\n")

    for filepath in find_c_files(args.paths):
        files_checked += 1
        violations = check_file(filepath)
        all_violations.extend(violations)

    if files_checked == 0:
        print("\033[33mNo .c files found to check\033[0m")
        return 0

    print(f"Checked {files_checked} files")

    if not args.summary:
        print_violations(all_violations)
    else:
        errors = sum(1 for v in all_violations if v.severity == "error")
        warnings = sum(1 for v in all_violations if v.severity == "warning")
        files_affected = len(set(v.file for v in all_violations))

        if all_violations:
            print(
                f"\033[31m{errors} errors, {warnings} warnings in {files_affected} files\033[0m"
            )
        else:
            print("\033[32m✓ All files pass FERN_STYLE checks\033[0m")

    # Exit code
    errors = sum(1 for v in all_violations if v.severity == "error")
    warnings = sum(1 for v in all_violations if v.severity == "warning")

    if args.strict:
        return 1 if (errors + warnings) > 0 else 0
    return 1 if errors > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
