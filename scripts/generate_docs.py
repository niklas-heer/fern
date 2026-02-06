#!/usr/bin/env python3
"""Generate lightweight documentation from Fern source files."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

FUNC_RE = re.compile(
    r"^\s*(?:pub\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*(?:->\s*([^:]+))?\s*:"
)
MODULE_RE = re.compile(r"^\s*module\s+([A-Za-z0-9_.]+)\s*$")


def collect_sources(path_arg: str) -> list[Path]:
    """Resolve a source path into a deterministic list of `.fn` files."""
    root = Path(path_arg)
    if root.is_file():
        return [root] if root.suffix == ".fn" else []
    if not root.exists():
        return []
    if not root.is_dir():
        return []
    return sorted(root.rglob("*.fn"))


def normalize_comment_line(raw: str) -> str:
    """Convert a leading `#` comment line into plain text."""
    text = raw.strip()
    if text.startswith("#"):
        return text[1:].strip()
    return text


def extract_comment(lines: list[str], index: int) -> str:
    """Extract nearby comment text above a function declaration."""
    comments: list[str] = []
    cursor = index - 1
    saw_comment = False

    while cursor >= 0:
        text = lines[cursor].rstrip()
        stripped = text.strip()
        if stripped.startswith("#"):
            saw_comment = True
            comments.append(normalize_comment_line(stripped))
            cursor -= 1
            continue
        if stripped == "":
            if saw_comment:
                cursor -= 1
                continue
            break
        break

    comments.reverse()
    while comments and comments[0] == "":
        comments.pop(0)
    while comments and comments[-1] == "":
        comments.pop()
    return "\n".join(comments).strip()


def parse_module_name(lines: list[str]) -> str | None:
    """Parse the first `module` declaration in a file, if present."""
    for line in lines:
        match = MODULE_RE.match(line)
        if match:
            return match.group(1)
    return None


def parse_functions(lines: list[str]) -> list[dict[str, str]]:
    """Extract function signatures and nearby comments."""
    functions: list[dict[str, str]] = []
    for idx, line in enumerate(lines):
        match = FUNC_RE.match(line)
        if not match:
            continue
        name = match.group(1)
        params = match.group(2).strip()
        ret = (match.group(3) or "").strip()
        signature = f"fn {name}({params})"
        if ret:
            signature += f" -> {ret}"
        functions.append(
            {
                "name": name,
                "signature": signature,
                "comment": extract_comment(lines, idx),
            }
        )
    return functions


def render_file_section(source: Path, repo_root: Path) -> str:
    """Render documentation for a single source file."""
    text = source.read_text(encoding="utf-8")
    lines = text.splitlines()
    module_name = parse_module_name(lines)
    functions = parse_functions(lines)
    rel = source.relative_to(repo_root) if source.is_relative_to(repo_root) else source

    out: list[str] = []
    out.append(f"## `{rel}`")
    out.append("")
    if module_name:
        out.append(f"- Module: `{module_name}`")
    out.append(f"- Functions: {len(functions)}")
    out.append("")

    if not functions:
        out.append("_No function declarations found._")
        out.append("")
        return "\n".join(out)

    for fn in functions:
        out.append(f"### `{fn['name']}`")
        out.append("")
        out.append("```fern")
        out.append(fn["signature"])
        out.append("```")
        out.append("")
        if fn["comment"]:
            out.append(fn["comment"])
        else:
            out.append("_No description provided._")
        out.append("")

    return "\n".join(out)


def open_output(path: Path) -> bool:
    """Best-effort open of generated docs in the default browser/app."""
    opener = None
    if sys.platform == "darwin":
        opener = shutil.which("open")
    elif sys.platform.startswith("linux"):
        opener = shutil.which("xdg-open")

    if not opener:
        print(f"note: no opener available for {path}", file=sys.stderr)
        return False

    try:
        subprocess.run([opener, str(path)], check=False)
    except OSError as exc:
        print(f"note: failed to open docs: {exc}", file=sys.stderr)
        return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate documentation for Fern sources.")
    parser.add_argument(
        "--path",
        default="examples",
        help="Fern source file or directory (default: examples)",
    )
    parser.add_argument(
        "--output",
        default="docs/generated/fern-docs.md",
        help="Output markdown file path",
    )
    parser.add_argument(
        "--open",
        action="store_true",
        dest="open_docs",
        help="Open generated documentation after writing",
    )
    args = parser.parse_args()

    repo_root = Path.cwd()
    sources = collect_sources(args.path)
    if not sources:
        print(f"error: no Fern source files found at {args.path}", file=sys.stderr)
        return 1

    sections = [
        "# Fern Documentation",
        "",
        f"Source root: `{Path(args.path)}`",
        f"Files documented: {len(sources)}",
        "",
    ]
    for source in sources:
        sections.append(render_file_section(source, repo_root))

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(sections).rstrip() + "\n", encoding="utf-8")
    print(f"Generated docs: {output}")

    if args.open_docs:
        open_output(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
