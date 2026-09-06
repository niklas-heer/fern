#!/usr/bin/env python3
"""Bounded native105A actor oracles, with literal arguments and exact process outcomes."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(argv, environment, directory, timeout):
    """Bound direct-child-only fixtures; never signal a numerical group after possible reaping."""
    child = subprocess.Popen(list(map(str, argv)), cwd=directory, env=environment,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        stdout, stderr = child.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as error:
        # Native fixtures start no OS descendants; language actors are in-process.
        # Compiler-tool descendants are outside this direct-child cleanup scope.
        # Popen.kill checks the owned child status. No PID/group fallback is permitted.
        child.kill()
        child.stdout.close()
        child.stderr.close()
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass  # A kernel-stalled direct child is reported; never guess at other identities.
        raise AssertionError(f"direct child timeout: {argv}") from error
    return child.returncode, stdout.decode("utf-8"), stderr.decode("utf-8")


def invalid_cases(compiler, fixtures, environment, directory):
    """Require genuine semantic rejection and preserve an already-existing executable atomically."""
    cases = json.loads((fixtures / "invalid.json").read_text())
    for case in cases:
        source = directory / (case["name"] + ".fn")
        source.write_text(case["source"])
        parsed = run([compiler, "parse", source], environment, directory, 10)
        if parsed[0] != 0:
            raise AssertionError((case["name"], "negative source must parse", parsed))
        binary = directory / "preserved"
        binary.write_text("preserved output")
        result = run([compiler, "build", source, "-o", binary], environment, directory, 10)
        if (result[0] != 1 or case["diagnostic"] not in result[2]
                or "panicked" in result[2] or binary.read_text() != "preserved output"):
            raise AssertionError((case["name"], result))
    return len(cases)


def main():
    """Compile independently before execution and compare every native stream and status exactly."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-bin", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    args = parser.parse_args()
    environment = dict(os.environ)
    environment.pop("LIBRARY_PATH", None)
    environment.setdefault("FERN_QBE", str(ROOT / "bin/fern-qbe"))
    environment.setdefault("FERN_RUNTIME_LIB", str(ROOT / "bin/libfern_runtime.a"))
    fixtures = ROOT / "compiler-rs/tests/actors"
    cases = json.loads((fixtures / "cases.json").read_text())
    with tempfile.TemporaryDirectory(prefix="fern-actors-native-") as temporary:
        directory = Path(temporary)
        for case in cases:
            source = fixtures / case["file"]
            binary = directory / source.stem
            built = run([args.rust_bin.resolve(), "build", source, "-o", binary],
                        environment, directory, 30)
            if built[0] != 0:
                raise AssertionError((source.name, "build", built))
            actual = run([binary], environment, directory, 15)
            expected = case["exit"], case["stdout"], case["stderr"]
            if actual != expected:
                raise AssertionError((source.name, actual, expected))
        invalid = invalid_cases(args.rust_bin.resolve(), fixtures, environment, directory)
    print(f"Rust native actors passed: {len(cases)} programs, {invalid} parsed semantic rejections")


if __name__ == "__main__":
    main()
