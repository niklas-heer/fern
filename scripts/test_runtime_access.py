#!/usr/bin/env python3
"""Check stable shared-runtime list failures in both debug and release builds."""
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    flags = subprocess.check_output(["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], text=True)
    with tempfile.TemporaryDirectory(prefix="fern-access-") as temporary:
        binary = Path(temporary) / "access"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Iruntime",
                        "tests/fixtures/access_runtime.c", "bin/libfern_runtime.a",
                        *shlex.split(flags), "-pthread", "-o", binary], cwd=ROOT, check=True)
        for mode in ["negative", "end", "huge", "head_empty", "get", "head"]:
            valid = mode in ["get", "head"]
            error = "head of empty list" if mode == "head_empty" else "list index out of bounds"
            expected = (0, "9223372036854775807\n", "") if valid else (1, "", f"fern: runtime error: {error}\n")
            result = subprocess.run([binary, mode], cwd=temporary, capture_output=True,
                                    text=True, timeout=10)
            assert (result.returncode, result.stdout, result.stderr) == expected, (mode, result)
    print("Runtime list access contracts passed: 6 cases")


if __name__ == "__main__":
    main()
