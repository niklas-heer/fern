#!/usr/bin/env python3
"""Literal argv, captured streams and process failures through the shared runtime."""
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    flags = subprocess.check_output(["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], text=True)
    with tempfile.TemporaryDirectory(prefix="fern-process-") as temporary:
        binary = Path(temporary) / "process'with quote"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Iruntime",
                        "tests/fixtures/process_runtime.c", "bin/libfern_runtime.a",
                        *shlex.split(flags), "-pthread", "-o", binary], cwd=ROOT, check=True)
        cases = [(["missing"], "-1\n"), (["empty"], "-1\n"),
                 (["streams"], "7\nchild output\nchild error\n"),
                 (["closed"], "7\nchild output\nchild error\n"), (["bulk"], "0\n524288\n524288\n"), (["signaled"], "-1\n\n\n")]
        for text in ["", "literal ; $(not-a-command) $HOME `nope`\n🌿", "'" * 8192]:
            cases.append((["literal", text], text))
        for args, expected in cases:
            result = subprocess.run([binary, *args], cwd=temporary, capture_output=True, text=True, timeout=10)
            if result.returncode or result.stdout != expected:
                raise AssertionError(f"{args[0]}: {result.returncode}, {result.stdout!r}\n{result.stderr}")
    print(f"Runtime process contracts passed: {len(cases)} cases")


if __name__ == "__main__":
    main()
