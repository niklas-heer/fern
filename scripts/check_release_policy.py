#!/usr/bin/env python3
"""Validate that the compatibility/deprecation policy document is release-ready."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REQUIRED_HEADINGS = [
    "# Compatibility and Deprecation Policy",
    "## Compatibility Guarantees",
    "## Deprecation Lifecycle",
    "## Versioning Policy",
    "## Release Checklist",
]

REQUIRED_TOKENS = [
    "include/version.h",
    "release workflow",
    "deprec",
    "semantic version",
]


def fail(message: str) -> None:
    """Print an error and exit.

    Args:
        message: Failure description.

    Returns:
        Never.
    """

    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def parse_args() -> argparse.Namespace:
    """Parse CLI arguments.

    Returns:
        Parsed args.
    """

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", default="docs/COMPATIBILITY_POLICY.md")
    return parser.parse_args()


def main() -> int:
    """Program entry point.

    Returns:
        Exit code.
    """

    args = parse_args()
    path = Path(args.path)

    if not path.exists():
        fail(f"missing policy document: {path}")

    text = path.read_text(encoding="utf-8")
    text_lower = text.lower()

    for heading in REQUIRED_HEADINGS:
        if heading not in text:
            fail(f"missing heading in {path}: {heading}")

    for token in REQUIRED_TOKENS:
        if token not in text_lower:
            fail(f"missing required content in {path}: '{token}'")

    match = re.search(r"minimum\s+support\s+window\s*:\s*(.+)", text_lower)
    if not match:
        fail("policy must declare a minimum support window")

    print(f"Release policy check passed: {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
