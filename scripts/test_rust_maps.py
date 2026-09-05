#!/usr/bin/env python3
"""Native immutable map and record-update specifications for the Rust frontend."""
from __future__ import annotations

import argparse
import os
import signal
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "float_keys": 'fn main(): let values = %{1.0: 1}\n',
    "compound_keys": 'fn main(): let values = %{[1]: 1}\n',
    "wrong_key": 'fn main(): println(Map.contains(%{1: 2}, true))\n',
    "wrong_value": 'fn main(): let changed = Map.put(%{1: 2}, 1, "wrong")\n',
    "unresolved_empty": 'fn main(): let empty = Map.new()\n',
    "duplicate_update": 'type Item:\n    value: Int\nfn main(): let changed = %{Item(1) | value: 2, value: 3}\n',
    "wrong_update": 'type Item:\n    value: Int\nfn main(): let changed = %{Item(1) | value: "wrong"}\n',
    "unknown_update": 'type Item:\n    value: Int\nfn main(): let changed = %{Item(1) | missing: 2}\n',
    "ignored_result": 'fn fail() -> Result(Int, String): Err("bad")\nfn main(): let hidden = %{1: fail()}\n',
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
    cases = sorted((ROOT / "compiler-rs/tests/maps").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-maps-") as temporary:
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
    print(f"Rust maps passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
