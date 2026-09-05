#!/usr/bin/env python3
"""Native lazy iteration and typed with-handler specifications for the Rust frontend."""
from __future__ import annotations

import argparse
import os
import signal
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "break_outside": 'fn main(): break\n',
    "continue_outside": 'fn main(): continue\n',
    "cross_lambda_break": 'fn main():\n    for n in 0..3:\n        let f = () -> break\n        f()\n',
    "defer_break": 'fn main():\n    for n in 0..3:\n        defer break\n',
    "float_range": 'fn main(): for n in 1.0..2.0: println(n)\n',
    "refutable_iteration": 'fn main(): for Some(n) in [Some(1)]: println(n)\n',
    "scalar_iteration": 'fn main(): for n in 1: println(n)\n',
    "with_plain_value": 'fn main(): with x <- 1 do println(x)\n',
    "with_impossible_ok": 'fn main(): with x <- Ok(1) do println(x) else Ok(_) -> ()\n',
    "with_nonexhaustive": 'fn failure() -> Result(Int, Bool): Err(false)\nfn main(): with x <- failure() do println(x) else Err(true) -> ()\n',
    "with_success_scope": 'fn failure() -> Result(Int, Int): Err(0)\nfn main(): with x <- failure() do println(x) else Err(_) -> println(x)\n',
    "with_wrong_return": 'fn failure() -> Result(Int, Int): Err(0)\nfn main(): with x <- failure() do println(x) else Err(_) -> true\n',
    "stray_bind": 'fn main(): let x <- Ok(1)\n',
    "loop_result_drop": 'fn main(): for value in [Ok(1)]: println(1)\n',
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
    cases = sorted((ROOT / "compiler-rs/tests/iteration").glob("*.fn"))
    cases += sorted((ROOT / "compiler-rs/tests/with").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-iteration-") as temporary:
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
    print(f"Rust iteration/with passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
