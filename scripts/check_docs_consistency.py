#!/usr/bin/env python3
"""Lightweight docs consistency checks for CI and local workflows."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterable
from urllib.parse import unquote

LINK_PATTERN = re.compile(r"(?<!\!)\[[^\]]+\]\(([^)]+)\)")
HEADING_PATTERN = re.compile(r"^\s{0,3}#{1,6}\s+(.+?)\s*$")

DOC_FILES = (
    "README.md",
    "BUILD.md",
    "ROADMAP.md",
    "docs/README.md",
)

REQUIRED_CROSS_LINKS = {
    "README.md": ("docs/README.md", "ROADMAP.md"),
    "BUILD.md": ("docs/README.md", "ROADMAP.md"),
}

REQUIRED_ROADMAP_MARKERS = (
    "Last updated:",
    "- Quality gate:",
    "- Perf gate:",
    "- Fuzz gate:",
    "- Docs gate:",
    "- Release readiness:",
    "## Next Session Start Here",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        default=".",
        help="Repository root to validate (default: current directory).",
    )
    return parser.parse_args()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def normalize_markdown_destination(raw: str) -> str:
    destination = raw.strip()
    if destination.startswith("<") and destination.endswith(">"):
        destination = destination[1:-1].strip()

    if " " in destination and not re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", destination):
        destination = destination.split(" ", 1)[0]
    return destination


def is_external_destination(destination: str) -> bool:
    lowered = destination.lower()
    return lowered.startswith(("http://", "https://", "mailto:", "tel:"))


def slugify_heading(title: str) -> str:
    text = re.sub(r"`([^`]*)`", r"\1", title)
    text = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", text)
    text = text.strip().rstrip("#").strip().lower()
    text = re.sub(r"[^\w\s-]", "", text)
    text = re.sub(r"[\s_]+", "-", text)
    text = re.sub(r"-{2,}", "-", text)
    return text.strip("-")


def collect_markdown_anchors(text: str) -> set[str]:
    anchors: set[str] = set()
    seen: dict[str, int] = {}

    for line in text.splitlines():
        match = HEADING_PATTERN.match(line)
        if not match:
            continue

        heading = re.sub(r"\s+#*$", "", match.group(1)).strip()
        base = slugify_heading(heading)
        if not base:
            continue

        count = seen.get(base, 0)
        anchor = base if count == 0 else f"{base}-{count}"
        seen[base] = count + 1
        anchors.add(anchor)

    return anchors


def format_path(root: Path, path: Path) -> str:
    try:
        return str(path.relative_to(root))
    except ValueError:
        return str(path)


def split_destination(destination: str) -> tuple[str, str | None]:
    if "#" not in destination:
        return destination, None
    path_part, anchor = destination.split("#", 1)
    return path_part, anchor


def validate_links(root: Path, doc_paths: Iterable[Path]) -> list[str]:
    errors: list[str] = []
    anchor_cache: dict[Path, set[str]] = {}

    for path in doc_paths:
        text = read_text(path)
        for raw_destination in LINK_PATTERN.findall(text):
            destination = normalize_markdown_destination(raw_destination)
            if not destination or is_external_destination(destination):
                continue

            link_path_raw, anchor_raw = split_destination(destination)
            link_path = unquote(link_path_raw)
            anchor = unquote(anchor_raw) if anchor_raw is not None else None

            if not link_path:
                target = path
            else:
                target = (path.parent / link_path).resolve()

            if not target.exists():
                errors.append(
                    f"{format_path(root, path)}: missing link target {destination}"
                )
                continue

            if not anchor:
                continue

            if target.suffix.lower() != ".md":
                continue

            anchors = anchor_cache.get(target)
            if anchors is None:
                anchors = collect_markdown_anchors(read_text(target))
                anchor_cache[target] = anchors

            if anchor not in anchors:
                errors.append(
                    f"{format_path(root, path)}: missing anchor '{anchor}' "
                    f"in {format_path(root, target)}"
                )

    return errors


def validate_required_cross_links(root: Path) -> list[str]:
    errors: list[str] = []

    for rel_path, required_links in REQUIRED_CROSS_LINKS.items():
        path = root / rel_path
        text = read_text(path)
        for link in required_links:
            if f"({link})" not in text:
                errors.append(
                    f"{rel_path}: missing required canonical link to {link}"
                )

    return errors


def validate_roadmap_markers(root: Path) -> list[str]:
    roadmap = read_text(root / "ROADMAP.md")
    errors: list[str] = []

    for marker in REQUIRED_ROADMAP_MARKERS:
        if marker not in roadmap:
            errors.append(f"ROADMAP.md: missing status marker '{marker}'")

    return errors


def main() -> int:
    args = parse_args()
    root = Path(args.root).resolve()

    doc_paths = [root / rel_path for rel_path in DOC_FILES]
    missing_docs = [path for path in doc_paths if not path.exists()]
    if missing_docs:
        for path in missing_docs:
            print(f"error: missing required doc {format_path(root, path)}", file=sys.stderr)
        return 1

    errors: list[str] = []
    errors.extend(validate_links(root, doc_paths))
    errors.extend(validate_required_cross_links(root))
    errors.extend(validate_roadmap_markers(root))

    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        print(f"error: docs consistency check failed ({len(errors)} issue(s))", file=sys.stderr)
        return 1

    print("✓ docs consistency checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
