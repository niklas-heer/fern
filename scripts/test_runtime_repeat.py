#!/usr/bin/env python3
"""Verify bounded repetition in the actual shared runtime, including release mode."""
from pathlib import Path
import argparse
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 16 * 1024 * 1024


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--red-safe", action="store_true",
                        help="Only test the safe, moderately oversized pre-fix case")
    args = parser.parse_args()
    flags = subprocess.check_output(["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], text=True)
    cases = [("x", LIMIT + 1, 1, "")]
    if not args.red_safe:
        cases += [("abcd", 1 << 62, 1, ""), ("🌿", (1 << 63) - 1, 1, ""),
                  ("", (1 << 63) - 1, 0, "0\n"), ("abc", -1, 0, "0\n"),
                  ("abc", 0, 0, "0\n"), ("🌿", 3, 0, "12\n"),
                  ("abcd", LIMIT // 4, 0, f"{LIMIT}\n")]
    with tempfile.TemporaryDirectory(prefix="fern-repeat-") as temporary:
        binary = Path(temporary) / "repeat"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Iruntime",
                        "tests/fixtures/repeat_runtime.c", "bin/libfern_runtime.a",
                        *shlex.split(flags), "-pthread", "-o", binary], cwd=ROOT, check=True)
        for value, count, code, output in cases:
            actual = subprocess.run([binary, value, str(count)], capture_output=True,
                                    text=True, timeout=10)
            error = "fern: runtime error: string size limit exceeded\n" if code else ""
            assert (actual.returncode, actual.stdout, actual.stderr) == (code, output, error), actual
    print(f"Runtime repetition contracts passed: {len(cases)} cases")


if __name__ == "__main__":
    main()
