#!/usr/bin/env python3
"""Validate all scalar classifications against primary Unicode data in three native modes."""
import argparse
import os
from pathlib import Path
import shlex
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def checked(argv, env, directory):
    """Run a literal bounded command and retain diagnostic output on failure."""
    result = run(argv, env, directory)
    assert result.returncode == 0, (argv, result.returncode, result.stdout, result.stderr)
    return result.stdout


def oracle():
    """Independently parse primary categories into one expected byte per code point."""
    values = bytearray(0x110000)
    source = ROOT / "deps/unicode/16.0.0/DerivedGeneralCategory.txt"
    for line in source.read_text().splitlines():
        raw = line.partition("#")[0].strip()
        if not raw:
            continue
        code, category = raw.split(";")
        if category.strip() != "Nd":
            continue
        low, _, high = code.strip().partition("..")
        start, end = int(low, 16), int(high or low, 16)
        values[start:end + 1] = b"\x01" * (end - start + 1)
    assert sum(values) == 760
    return values


def main():
    """Compile isolated runtime code and check exhaustive, malformed and cap oracles."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, default=ROOT / "bin/libfern_runtime.a")
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop("LIBRARY_PATH", None)
    env.update(ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    cflags = shlex.split(checked(["pkg-config", "--cflags", "bdw-gc"], env, ROOT))
    libs = shlex.split(checked(["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], env, ROOT))
    variants = [("debug", ["-O0", "-g"]), ("release", ["-O2", "-DNDEBUG"]),
                ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]
    expected = oracle()
    with tempfile.TemporaryDirectory(prefix="fern-decimal-") as temporary:
        directory = Path(temporary)
        for name, flags in variants:
            binary = directory / name
            checked(["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Iruntime",
                     *flags, *cflags, ROOT / "tests/fixtures/decimal_runtime.c", ROOT / "runtime/fern_decimal.c",
                     args.runtime.resolve(), *libs, "-pthread", "-lm", "-o", binary], env, ROOT)
            actual = checked([binary, "all"], env, directory).encode()
            assert actual == expected, (name, "scalar oracle mismatch")
            for mode in ["text", "limit"]:
                assert checked([binary, mode], env, directory) == "ok\n", (name, mode)
            for mode in ["oversize", "oversize_nondecimal"]:
                result = run([binary, mode], env, directory)
                assert (result.returncode, result.stdout, result.stderr) == (1, "", "fern: runtime error: string size limit exceeded\n"), (name, mode, result)
            print(f"{name}: all1114112 code points and4 decimal text/limit groups passed")


if __name__ == "__main__":
    main()
