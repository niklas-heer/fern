#!/usr/bin/env python3
"""Native abrupt control flow and function-exit cleanup specifications for the Rust frontend."""
from __future__ import annotations

import argparse
import os
import signal
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "wrong_return": 'fn bad() -> Int: return true\nfn main(): ()\n',
    "unreachable": 'fn main():\n    return ()\n    println("never")\n',
    "non_unit_cleanup": 'fn main(): defer 1\n',
    "return_from_cleanup": 'fn main(): defer return ()\n',
    "propagate_from_cleanup": 'fn main(): defer println(Ok(1)?)\n',
    "let_else_continues": 'fn main():\n    let Some(x) = Some(1) else: ()\n    println(x)\n',
    "let_else_failure_scope": 'fn bad(value: Option(Int)) -> Int:\n    let Some(x) = value else: return x\n    x\nfn main(): ()\n',
    "condition_no_fallback": 'fn main():\n    match:\n        true -> ()\n',
    "condition_wrong_type": 'fn main():\n    match:\n        1 -> ()\n        _ -> ()\n',
    "ignored_result_cleanup": 'fn main():\n    let result: Result(Int, String) = Ok(1)\n    defer println("cleanup")\n    ()\n',
}



def run(argv, environment, directory):
    """Execute literal arguments with captured output and a bounded deadline."""
    process = subprocess.Popen(list(map(str, argv)), cwd=directory, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=30)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        stdout, stderr = process.communicate(timeout=5)
        raise AssertionError(f"command timed out: {argv}; stdout={stdout!r}; stderr={stderr!r}")
    return subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)


def main():
    """Compare exact native output, then verify invalid programs preserve output files."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-bin", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    compiler = parser.parse_args().rust_bin.resolve()
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted((ROOT / "compiler-rs/tests/control").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-control-") as temporary:
        directory = Path(temporary)
        for source in cases:
            expected = source.with_suffix(".stdout").read_text()
            actual = run([compiler, "run", source], environment, directory)
            if actual.returncode or actual.stdout != expected:
                raise AssertionError(f"{source.name}: exit={actual.returncode}, stdout={actual.stdout!r}\n{actual.stderr}")
        for name, text in INVALID.items():
            source = directory / f"{name}.fn"
            source.write_text(text)
            output = directory / "preserved-output"
            output.write_text("existing output")
            actual = run([compiler, "build", source, "-o", output], environment, directory)
            if (actual.returncode != 1 or "panicked" in actual.stderr
                    or "error:" not in actual.stderr or output.read_text() != "existing output"):
                raise AssertionError(f"{name}: {actual.stderr}")
    print(f"Rust control passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
