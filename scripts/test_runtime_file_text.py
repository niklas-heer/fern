#!/usr/bin/env python3
"""Compile isolated file-text runtime and verify native text/signal/descriptor contracts."""
import argparse
import os
from pathlib import Path
import shlex
import tempfile
from test_rust_numeric import run
ROOT = Path(__file__).resolve().parents[1]


def checked(argv, env, directory):
    """Run one literal bounded command and surface complete failure output."""
    result = run(argv, env, directory)
    assert result.returncode == 0, (argv, result.returncode, result.stdout, result.stderr)
    return result.stdout


def main():
    """Verify debug/release/sanitized builds against a caller-selected frozen archive."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, default=ROOT / "bin/libfern_runtime.a")
    args = parser.parse_args()
    env = dict(os.environ)
    env.pop("LIBRARY_PATH", None)
    env["ASAN_OPTIONS"] = "detect_leaks=0"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    cflags = shlex.split(checked(["pkg-config", "--cflags", "bdw-gc"], env, ROOT))
    libs = shlex.split(checked(["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], env, ROOT))
    variants = [("debug", ["-O0", "-g"]), ("release", ["-O2", "-DNDEBUG"]),
                ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]
    with tempfile.TemporaryDirectory(prefix="fern-file-text-") as temporary:
        directory = Path(temporary)
        for name, flags in variants:
            binary = directory / name
            sources = [ROOT / "tests/fixtures/file_text_runtime.c"]
            if (ROOT / "runtime/fern_file_text.c").exists():
                sources.append(ROOT / "runtime/fern_file_text.c")
            checked(["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Iruntime",
                     *flags, *cflags, *sources, args.runtime.resolve(), *libs, "-pthread", "-lm", "-o", binary], env, ROOT)
            state_binary = directory / ("state-" + name)
            checked(["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-Iruntime",
                     *flags, *cflags, ROOT / "tests/fixtures/file_text_state.c", args.runtime.resolve(),
                     *libs, "-pthread", "-lm", "-o", state_binary], env, ROOT)
            assert checked([state_binary, directory / ("state-file-" + name)], env, directory) == "ok:state\n"
            for mode in ["valid", "invalid", "limits", "buffered"]:
                result = run([binary, mode, directory / (mode + name)], env, directory)
                assert result.returncode == 0, (name, mode, result)
                assert result.stdout == f"ok:{mode}\n", (name, mode, result)
                assert result.stderr == "", (name, mode, result)
            print(f"{name}: 5 file-text groups passed")

if __name__ == "__main__":
    main()
