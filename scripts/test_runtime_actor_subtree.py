#!/usr/bin/env python3
"""Verify actual actor subtree termination and existing lifecycle scenarios."""
import argparse
import os
from pathlib import Path
import shlex
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def checked(argv, environment, directory):
    """Run a literal bounded command and preserve its complete failure report."""
    result = run(argv, environment, directory)
    assert result.returncode == 0, (argv, result.returncode, result.stdout, result.stderr)
    return result.stdout


def main():
    """Instrument the complete actor implementation against a frozen runtime archive."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runtime", type=Path, default=ROOT / "bin/libfern_runtime.a")
    args = parser.parse_args()
    environment = dict(os.environ)
    environment.pop("LIBRARY_PATH", None)
    environment["ASAN_OPTIONS"] = "detect_leaks=0"
    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    packages = ["bdw-gc", "sqlite3", "openssl"]
    cflags = shlex.split(checked(["pkg-config", "--cflags", *packages], environment, ROOT))
    libs = shlex.split(checked(["pkg-config", "--libs", *packages], environment, ROOT))
    variants = [("debug", ["-O0", "-g"]), ("release", ["-O2", "-DNDEBUG"]),
                ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]
    with tempfile.TemporaryDirectory(prefix="fern-actor-subtree-") as temporary:
        directory = Path(temporary)
        for name, flags in variants:
            common = ["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                      "-Iruntime", "-Iinclude", "-Ideps/civetweb/include", "-Ideps/linenoise", *flags, *cflags]
            binary = directory / name
            checked([*common, ROOT / "tests/fixtures/actor_subtree_state.c", args.runtime.resolve(),
                     *libs, "-pthread", "-lm", "-o", binary], environment, ROOT)
            for mode in ["normal", "shutdown", "failure", "fault", "send-fault", "strategy", "deep", "preterminated", "restart-atomic", "spawn-atomic"]:
                result = run([binary], {**environment, "FERN_SUBTREE_MODE": mode}, directory)
                assert (result.returncode, result.stdout, result.stderr) == (0, "", ""), (name, mode, result)
            previous = directory / (name + "-previous")
            checked([*common, ROOT / "runtime/fern_runtime.c", ROOT / "tests/fixtures/runtime_actor_scenarios.c",
                     ROOT / "lib/fernsim.c", ROOT / "lib/arena.c", args.runtime.resolve(),
                     *libs, "-pthread", "-lm", "-o", previous], environment, ROOT)
            for mode in ["time-zero", "single-replacement", "forest", "invalid-pid", "terminated-sibling", "simulation"]:
                result = run([previous], {**environment, "FERN_ACTOR_SCENARIO": mode}, directory)
                assert (result.returncode, result.stdout, result.stderr) == (0, "", ""), (name, mode, result)
            print(f"{name}: 10 subtree groups and 6 prior actor scenarios passed")


if __name__ == "__main__":
    main()
