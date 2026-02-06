#!/usr/bin/env python3
"""Generate lightweight markdown/HTML documentation from Fern source files."""

from __future__ import annotations

import argparse
import html
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

FUNC_RE = re.compile(
    r"^\s*(?:pub\s+)?fn\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*(?:->\s*([^:]+))?\s*:"
)
MODULE_RE = re.compile(r"^\s*module\s+([A-Za-z0-9_.]+)\s*$")
DOC_SINGLE_RE = re.compile(r'^\s*@doc\s+"""(.*)"""\s*$')
DOC_START_RE = re.compile(r'^\s*@doc\s+"""\s*$')
DOC_END_RE = re.compile(r'^\s*"""\s*$')


@dataclass
class DocFunction:
    """Collected function documentation."""

    name: str
    signature: str
    comment: str
    anchor: str


@dataclass
class ModuleDoc:
    """Collected per-module documentation."""

    module: str
    module_anchor: str
    source: Path
    functions: list[DocFunction]


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


def normalize_doc_text(lines: list[str]) -> str:
    """Normalize extracted doc/comment lines."""
    if not lines:
        return ""
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    if not lines:
        return ""

    min_indent = None
    for line in lines:
        stripped = line.lstrip()
        if not stripped:
            continue
        indent = len(line) - len(stripped)
        if min_indent is None or indent < min_indent:
            min_indent = indent
    if min_indent is None:
        min_indent = 0

    normalized = [line[min_indent:].rstrip() for line in lines]
    return "\n".join(normalized).strip()


def extract_doc_block(lines: list[str], index: int) -> str:
    """Extract nearby @doc text above a function declaration."""
    cursor = index - 1
    while cursor >= 0 and lines[cursor].strip() == "":
        cursor -= 1
    if cursor < 0:
        return ""

    single_match = DOC_SINGLE_RE.match(lines[cursor])
    if single_match:
        return single_match.group(1).strip()

    if not DOC_END_RE.match(lines[cursor]):
        return ""

    cursor -= 1
    collected: list[str] = []
    while cursor >= 0 and not DOC_START_RE.match(lines[cursor]):
        collected.append(lines[cursor].rstrip("\n"))
        cursor -= 1
    if cursor < 0:
        return ""

    collected.reverse()
    return normalize_doc_text(collected)


def parse_module_name(lines: list[str]) -> str | None:
    """Parse the first `module` declaration in a file, if present."""
    for line in lines:
        match = MODULE_RE.match(line)
        if match:
            return match.group(1)
    return None


def slug(text: str) -> str:
    """Generate a stable anchor slug."""
    lowered = text.lower()
    lowered = re.sub(r"[^a-z0-9_]+", "-", lowered)
    lowered = re.sub(r"-{2,}", "-", lowered).strip("-")
    return lowered or "item"


def parse_functions(lines: list[str], module_name: str) -> list[DocFunction]:
    """Extract function signatures and nearby docs/comments."""
    functions: list[DocFunction] = []
    module_anchor = slug(module_name)
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
        doc_text = extract_doc_block(lines, idx)
        if not doc_text:
            doc_text = extract_comment(lines, idx)
        functions.append(
            DocFunction(
                name=name,
                signature=signature,
                comment=doc_text,
                anchor=f"{module_anchor}-{slug(name)}",
            )
        )
    return functions


def collect_module_docs(source: Path, repo_root: Path) -> ModuleDoc:
    """Collect documentation metadata for a single source file."""
    text = source.read_text(encoding="utf-8")
    lines = text.splitlines()
    rel = source.relative_to(repo_root) if source.is_relative_to(repo_root) else source
    module_name = parse_module_name(lines)
    if not module_name:
        module_name = rel.with_suffix("").as_posix().replace("/", ".")

    return ModuleDoc(
        module=module_name,
        module_anchor=slug(module_name),
        source=rel,
        functions=parse_functions(lines, module_name),
    )


def render_markdown(modules: list[ModuleDoc], source_root: str) -> str:
    """Render collected docs as markdown."""
    out: list[str] = []
    out.append("# Fern Documentation")
    out.append("")
    out.append(f"Source root: `{Path(source_root)}`")
    out.append(f"Files documented: {len(modules)}")
    out.append("")
    out.append("## Modules")
    out.append("")
    for module in modules:
        out.append(f"- [`{module.module}`](#{module.module_anchor})")
        if not module.functions:
            continue
        for fn in module.functions:
            out.append(f"  - [`{fn.name}`](#{fn.anchor})")
    out.append("")

    for module in modules:
        out.append(f"<a id=\"{module.module_anchor}\"></a>")
        out.append(f"## `{module.module}`")
        out.append("")
        out.append(f"- Source: `{module.source}`")
        out.append(f"- Functions: {len(module.functions)}")
        out.append("")

        if not module.functions:
            out.append("_No function declarations found._")
            out.append("")
            continue

        for fn in module.functions:
            out.append(f"<a id=\"{fn.anchor}\"></a>")
            out.append(f"### `{fn.name}`")
            out.append("")
            out.append("```fern")
            out.append(fn.signature)
            out.append("```")
            out.append("")
            if fn.comment:
                out.append(fn.comment)
            else:
                out.append("_No description provided._")
            out.append("")

    return "\n".join(out).rstrip() + "\n"


def render_html_paragraphs(text: str) -> str:
    """Render plain text into paragraph blocks."""
    if not text:
        return "<p><em>No description provided.</em></p>"
    chunks = [chunk.strip() for chunk in re.split(r"\n\s*\n", text) if chunk.strip()]
    if not chunks:
        return "<p><em>No description provided.</em></p>"
    return "\n".join(f"<p>{html.escape(chunk)}</p>" for chunk in chunks)


def render_html(modules: list[ModuleDoc], source_root: str) -> str:
    """Render collected docs as HTML."""
    lines: list[str] = [
        "<!doctype html>",
        "<html lang=\"en\">",
        "<head>",
        "<meta charset=\"utf-8\">",
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
        "<title>Fern Documentation</title>",
        "<style>",
        "body { font-family: ui-sans-serif, -apple-system, sans-serif; margin: 2rem auto; max-width: 900px; line-height: 1.55; padding: 0 1rem; }",
        "h1, h2, h3 { line-height: 1.2; }",
        "code { background: #f4f4f4; padding: 0.1rem 0.3rem; border-radius: 4px; }",
        "pre { background: #111827; color: #e5e7eb; padding: 0.75rem; border-radius: 8px; overflow-x: auto; }",
        "a { color: #0f5da8; text-decoration: none; }",
        "a:hover { text-decoration: underline; }",
        "</style>",
        "</head>",
        "<body>",
        "<h1>Fern Documentation</h1>",
        f"<p>Source root: <code>{html.escape(str(Path(source_root)))}</code></p>",
        f"<p>Files documented: {len(modules)}</p>",
        "<h2>Modules</h2>",
        "<ul>",
    ]

    for module in modules:
        lines.append(
            f"<li><a href=\"#{module.module_anchor}\"><code>{html.escape(module.module)}</code></a></li>"
        )
        if not module.functions:
            continue
        lines.append("<ul>")
        for fn in module.functions:
            lines.append(f"<li><a href=\"#{fn.anchor}\"><code>{html.escape(fn.name)}</code></a></li>")
        lines.append("</ul>")

    lines.append("</ul>")

    for module in modules:
        lines.append(f"<section id=\"{module.module_anchor}\">")
        lines.append(f"<h2><code>{html.escape(module.module)}</code></h2>")
        lines.append(f"<p>Source: <code>{html.escape(str(module.source))}</code></p>")
        lines.append(f"<p>Functions: {len(module.functions)}</p>")
        if not module.functions:
            lines.append("<p><em>No function declarations found.</em></p>")
            lines.append("</section>")
            continue

        for fn in module.functions:
            lines.append(f"<article id=\"{fn.anchor}\">")
            lines.append(f"<h3><code>{html.escape(fn.name)}</code></h3>")
            lines.append(
                f"<pre><code class=\"language-fern\">{html.escape(fn.signature)}</code></pre>"
            )
            lines.append(render_html_paragraphs(fn.comment))
            lines.append("</article>")

        lines.append("</section>")

    lines.append("</body>")
    lines.append("</html>")
    return "\n".join(lines) + "\n"


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
        help="Output documentation file path",
    )
    parser.add_argument(
        "--open",
        action="store_true",
        dest="open_docs",
        help="Open generated documentation after writing",
    )
    parser.add_argument(
        "--html",
        action="store_true",
        help="Generate HTML output instead of markdown",
    )
    args = parser.parse_args()

    repo_root = Path.cwd()
    sources = collect_sources(args.path)
    if not sources:
        print(f"error: no Fern source files found at {args.path}", file=sys.stderr)
        return 1

    modules = [collect_module_docs(source, repo_root) for source in sources]
    output = Path(args.output)
    if args.html and output == Path("docs/generated/fern-docs.md"):
        output = Path("docs/generated/fern-docs.html")

    if args.html:
        body = render_html(modules, args.path)
    else:
        body = render_markdown(modules, args.path)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(body, encoding="utf-8")
    print(f"Generated docs: {output}")

    if args.open_docs:
        open_output(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
