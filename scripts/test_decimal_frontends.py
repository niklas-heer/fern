#!/usr/bin/env python3
"""Finite native decimal predicate, callback, size-fault and atomic rejection corpus."""
import argparse
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "basic": (0, "true\n" * 3 + "false\n" * 7, ""),
    "cap": (0, "true\n", ""),
    "oversize": (1, "", "fern: runtime error: string size limit exceeded\n"),
    "higher_order": (0, "once\ntrue\ntrue\nfalse\n2\n", ""),
    "fault": (1, "done\n", "fern: runtime error: string size limit exceeded\n"),
    "cleanup_fault": (1, "done\n", "fern: runtime error: integer division by zero\n"),
}


def main():
    """Use caller-selected frontends/runtime, keeping C callback/cleanup limits explicit."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    parser.add_argument("--runtime", type=Path, default=ROOT / "bin/libfern_runtime.a")
    parser.add_argument("--qbe", type=Path, default=ROOT / "bin/fern-qbe")
    parser.add_argument("--c", action="store_true")
    args = parser.parse_args()
    environment = dict(os.environ, FERN_RUNTIME_LIB=str(args.runtime.resolve()), FERN_QBE=str(args.qbe.resolve()))
    environment.pop("LIBRARY_PATH", None)
    count = 0
    with tempfile.TemporaryDirectory(prefix="fern-decimal-source-") as temporary:
        directory = Path(temporary)
        for name, expected in CASES.items():
            if args.c and name in ("higher_order", "fault", "cleanup_fault"):
                continue
            source = ROOT / f"compiler-rs/tests/decimal/{name}.fn"
            binary = directory / name
            built = run([args.compiler.resolve(), "build", source, "-o", binary], environment, directory)
            assert built.returncode == 0, (name, built.stderr)
            actual = run([binary], environment, directory)
            assert (actual.returncode, actual.stdout, actual.stderr) == expected, (name, actual)
            count += 1
        for value in ["1", "", '"1", "2"']:
            source = directory / "invalid.fn"
            source.write_text(f"fn main(): String.is_decimal({value})\n")
            output = directory / "preserved"
            output.write_text("preserved")
            result = run([args.compiler.resolve(), "build", source, "-o", output], environment, directory)
            assert result.returncode != 0 and "error:" in result.stderr, result
            assert output.read_text() == "preserved"
    print(f"Decimal source contracts passed: {count} native cases and3 atomic rejections")


if __name__ == "__main__":
    main()
