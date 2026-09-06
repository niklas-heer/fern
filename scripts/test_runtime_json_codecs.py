#!/usr/bin/env python3
"""Exercise typed JSON ABI and shared budgets in debug, release and sanitizers.

Only JSON objects and test binaries are built, in an owned temporary directory.
The runtime archive must already exist; this never builds the C frontend.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    """Check exact native boundary/counter oracles under each supported build profile."""
    environment = dict(os.environ)
    environment.pop("LIBRARY_PATH", None)
    environment["ASAN_OPTIONS"] = "detect_leaks=0"
    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    archive = environment.get("FERN_RUNTIME_LIB", str(ROOT / "bin/libfern_runtime.a"))
    cflags = shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "bdw-gc"], text=True, env=environment, timeout=30))
    libraries = shlex.split(subprocess.check_output(
        ["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"],
        text=True, env=environment, timeout=30))
    profiles = [("debug", ["-g", "-O0"]), ("release", ["-O2", "-DNDEBUG"]),
                ("sanitized", ["-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]
    with tempfile.TemporaryDirectory(prefix="fern-json-codec-runtime-") as directory:
        for name, flags in profiles:
            for fixture in ["typed_runtime", "path_boundary", "shared_budget", "recursive_runtime"]:
                binary = Path(directory) / f"{fixture}-{name}"
                sources = [f"tests/json_codecs/{fixture}.c"]
                if fixture in ("typed_runtime", "recursive_runtime"):
                    sources.append("runtime/fern_json.c")
                command = ["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                           "-Iruntime", *cflags, *flags, *sources, archive, *libraries,
                           "-pthread", "-lm", "-o", str(binary)]
                subprocess.run(command, cwd=ROOT, env=environment, check=True, timeout=60)
                actual = subprocess.run([binary], cwd=ROOT, env=environment, check=False,
                                        capture_output=True, text=True, timeout=30)
                if actual.returncode or actual.stderr:
                    raise RuntimeError(f"{name}/{fixture}: {actual.returncode}: {actual.stderr}")
                print(f"{name}/{fixture}: {actual.stdout.strip()}")


if __name__ == "__main__":
    main()
