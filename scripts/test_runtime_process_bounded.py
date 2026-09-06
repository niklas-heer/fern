#!/usr/bin/env python3
"""Decision85 native capture state-machine oracles in debug/release/ASan+UBSan builds."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from test_rust_numeric import run
ROOT = Path(__file__).resolve().parents[1]


def command(argv, env, directory):
    """Run a literal command with a deadline and process-group cleanup on timeout."""
    result = run(argv, env, directory)
    if result.returncode:
        raise AssertionError(f"{argv}: {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result.stdout


def main():
    """Compile only the new runtime module against a caller-selected frozen runtime archive."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, default=ROOT / "bin/libfern_runtime.a")
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop("LIBRARY_PATH", None)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    cflags = shlex.split(command(["pkg-config", "--cflags", "bdw-gc"], env, ROOT))
    libs = shlex.split(command(["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], env, ROOT))
    variants = [("debug", ["-O0", "-g"]), ("release", ["-O2", "-DNDEBUG"]),
                ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]
    with tempfile.TemporaryDirectory(prefix="fern-bounded-process-") as temporary:
        directory = Path(temporary)
        for name, flags in variants:
            binary = directory / ("process ' literal 🌿 " + name)
            sources = [ROOT / "tests/fixtures/process_bounded_runtime.c"]
            if (ROOT / "runtime/fern_process.c").exists():
                sources.append(ROOT / "runtime/fern_process.c")
            command(["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Iruntime",
                     *flags, *cflags, *sources, args.runtime.resolve(), *libs, "-pthread", "-lm", "-o", binary], env, ROOT)
            state_binary = directory / ("state-" + name)
            command(["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Iruntime",
                     *flags, *cflags, ROOT / "tests/fixtures/process_bounded_state.c", args.runtime.resolve(),
                     *libs, "-pthread", "-lm", "-o", state_binary], env, ROOT)
            assert command([state_binary], env, directory) == "ok:state\n"
            cases = [(mode, str(directory / (mode + name))) for mode in
                     ["basic", "output", "invalid", "spawn", "lifecycle", "fds", "signals", "exhausted", "arguments", "escaped", "maximum", "path"]]
            cases += [("closed", str(mask)) for mask in range(8)]
            for mode, extra in cases:
                output = command([binary, mode, extra], env, directory)
                assert output == f"ok:{mode}\n", (name, mode, output)
            print(f"{name}: 21 bounded process groups passed")

if __name__ == "__main__":
    main()
