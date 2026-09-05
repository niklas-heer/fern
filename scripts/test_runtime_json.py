#!/usr/bin/env python3
"""Test the new native JSON core without migrating the legacy source API.

Requires bin/libfern_runtime.a from the selected checkout. All instrumented JSON
objects and test binaries stay in a temporary directory; shared builds are never
started here. Debug, release and ASan/UBSan variants exercise the same oracle.
"""
from pathlib import Path
import os
import decimal
import math
import random
import struct
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def numeric_cases():
    """Produce exact-decimal signed64 and explicit binary64 conversion oracles."""
    randomizer = random.Random(70)
    numbers, expected = [], []
    for _ in range(6000):
        whole = str(randomizer.randrange(10 ** randomizer.randrange(1, 40)))
        fraction = "." + "".join(str(randomizer.randrange(10)) for _ in range(
            randomizer.randrange(1, 40))) if randomizer.randrange(2) else ""
        exponent = "e" + str(randomizer.randrange(-400, 401)) if randomizer.randrange(2) else ""
        text = ("-" if randomizer.randrange(2) else "") + whole + fraction + exponent
        number = decimal.Decimal(text)
        integral = number.to_integral_value()
        integer = ("err:9" if integral != number else "err:8" if not -(2**63) <= integral < 2**63
                   else "ok:" + str(int(integral)))
        floating = float(text)
        binary = ("err:8" if not math.isfinite(floating) or (floating == 0.0 and number != 0)
                  else "ok:" + struct.pack(">d", floating).hex())
        numbers.append(text)
        expected.append(integer + " " + binary)
    return "\n".join(numbers) + "\n", expected


def main():
    environment = dict(os.environ)
    environment.pop("LIBRARY_PATH", None)
    environment["ASAN_OPTIONS"] = "detect_leaks=0"
    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    cflags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "bdw-gc"], text=True, env=environment))
    libraries = shlex.split(subprocess.check_output(
        ["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], text=True, env=environment))
    variants = [("debug", ["-g", "-O0"]), ("release", ["-O2", "-DNDEBUG"]),
                ("sanitized", ["-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]
    numeric_input, numeric_expected = numeric_cases()
    with tempfile.TemporaryDirectory(prefix="fern-json-core-") as directory:
        for name, flags in variants:
            for fixture in ["json_value_runtime", "json_value_budget", "json_value_numeric"]:
                binary = Path(directory) / f"{fixture}-{name}"
                sources = [f"tests/fixtures/{fixture}.c"]
                if fixture != "json_value_budget":
                    sources.append("runtime/fern_json.c")
                command = ["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                           "-Iruntime", *cflags, *flags, *sources, "bin/libfern_runtime.a",
                           *libraries, "-pthread", "-lm", "-o", str(binary)]
                subprocess.run(command, cwd=ROOT, env=environment, check=True, timeout=60)
                run = subprocess.run([binary], cwd=ROOT, env=environment, check=False,
                                     capture_output=True, text=True, timeout=30,
                                     input=numeric_input if fixture == "json_value_numeric" else None)
                if run.returncode:
                    raise RuntimeError(f"{name}/{fixture}: {run.stderr}")
                if fixture == "json_value_numeric":
                    if run.stdout.splitlines() != numeric_expected:
                        raise RuntimeError(f"{name}: decimal conversion oracle mismatch")
                    print(f"{name}: 6000 exact decimal and binary64 oracles passed")
                else:
                    print(f"{name}/{fixture}: {run.stdout.strip()}")


if __name__ == "__main__":
    main()
