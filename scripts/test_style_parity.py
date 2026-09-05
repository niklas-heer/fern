#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["rich>=13.0"]
# ///
"""Compare diagnostic identity, message, severity, and exit status of both checkers.

Fixtures deliberately fail style checks: matching success on clean source cannot
prove parity. The Python implementation remains the compatibility reference.
Run from the repository root after building bin/fern. No clean build is performed.
"""
import argparse
from collections import Counter
import os
from pathlib import Path
import subprocess
import tempfile

import check_style


ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests/style_fixtures"


def native_diagnostics(binary, paths, lenient=False, summary=False):
    """Run an isolated checker process and parse its tab-separated diagnostics."""
    command = [str(binary), "--style-only", "--diagnostics", *map(str, paths)]
    if lenient:
        command.append("--lenient")
    if summary:
        command.append("--summary")
    result = subprocess.run(command, text=True, capture_output=True, timeout=120)
    records = []
    for line in result.stdout.splitlines():
        if line.startswith("DIAG\t"):
            fields = line.split("\t")
            assert len(fields) == 7, f"Malformed diagnostic: {line!r}"
            records.append(tuple(fields[1:]))
    return result, Counter(records)


def reference_diagnostics(paths, lenient):
    """Collect structured reference records without depending on Rich rendering."""
    records = []
    for file in check_style.find_c_files(list(map(str, paths))):
        for v in check_style.check_file(file, strict=not lenient):
            records.append((str(v.file), str(v.line), v.function, v.rule, v.message, v.severity))
    return Counter(records)


def compare(binary, paths, label):
    """Require exact multiset equality, including duplicate diagnostics and exit codes."""
    for lenient in (False, True):
        expected = reference_diagnostics(paths, lenient)
        result, actual = native_diagnostics(binary, paths, lenient)
        missing, extra = expected - actual, actual - expected
        assert not missing and not extra, (
            f"{label}, lenient={lenient}\nMissing: {missing}\nExtra: {extra}\n"
            f"Native stderr: {result.stderr}\nNative stdout: {result.stdout[:2000]}"
        )
        failed = any(v[-1] == "error" or not lenient for v in expected)
        assert result.returncode == int(failed), (
            f"{label}: expected exit {int(failed)}, got {result.returncode}"
        )
    print(f"  PASS {label}: exact strict/lenient diagnostic and exit parity")


def main():
    """Build the native checker once, then exercise fixtures and repository sources."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, help="Use an already compiled checker")
    parser.add_argument("--fixtures-only", action="store_true")
    args = parser.parse_args()
    os.chdir(ROOT)
    with tempfile.TemporaryDirectory(prefix="fern-style-parity-") as temp:
        binary = args.native.resolve() if args.native else Path(temp) / "checker"
        if not args.native:
            env = dict(os.environ)
            env.pop("LIBRARY_PATH", None)
            subprocess.run(["bin/fern", "build", "-o", str(binary), "scripts/check_style.fn"],
                           env=env, check=True, timeout=120)
        counts = {"documentation.c": 2, "empty.c": 0, "length.c": 1,
                  "rules.c": 11, "warnings.c": 2}
        for fixture in sorted(FIXTURES.rglob("*.c")):
            # Pin the oracle coverage, so empty scans cannot make the gate pass.
            expected = reference_diagnostics([fixture], False)
            assert expected.total() == counts[fixture.name], (fixture.name, expected)
            compare(binary, [fixture], fixture.name)
        # Directory traversal must include nested C files and ignore other extensions.
        compare(binary, [FIXTURES], "fixture directory")
        summary = subprocess.run([str(binary), "--style-only", "--summary",
                                  str(FIXTURES / "rules.c")],
                                 text=True, capture_output=True, timeout=120)
        assert summary.returncode == 1, summary.stdout + summary.stderr
        assert "missing_docs()" not in summary.stdout, summary.stdout
        assert "DIAG\t" not in summary.stdout, summary.stdout
        print("  PASS summary suppresses individual diagnostics")
        if not args.fixtures_only:
            compare(binary, [Path("src"), Path("lib")], "repository")
    print("Style checker diagnostic parity passed.")


if __name__ == "__main__":
    main()
