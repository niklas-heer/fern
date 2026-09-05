#!/usr/bin/env python3
"""Native numeric, literal and controlled runtime-fault specifications for the Rust frontend."""
from __future__ import annotations

import argparse
import os
import signal
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "bad_binary_digit": 'fn main(): println(0b102)\n',
    "bad_octal_digit": 'fn main(): println(0o8)\n',
    "missing_digits": 'fn main(): println(0x)\n',
    "overflow_hex": 'fn main(): println(0x8000000000000000)\n',
    "overflow_negative": 'fn main(): println(-0x8000000000000001)\n',
    "bad_separator": 'fn main(): println(1__2)\n',
    "float_bits": 'fn main(): println(1.0 &&& 2.0)\n',
    "float_complement": 'fn main(): println(~~~1.0)\n',
    "mixed_power": 'fn main(): println(2 ** 3.0)\n',
    "open_comment": 'fn main(): () /* unfinished\n',
    "open_triple": 'fn main(): println("""unfinished)\n',
    "orphan_doc": '@doc """No declaration"""\n',
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


def runtime_faults(compiler, environment, directory):
    """Require controlled exit, stable first error and complete observable cleanup order."""
    cases = sorted((ROOT / "compiler-rs/tests/numeric_faults").glob("*.fn"))
    for source in cases:
        actual = run([compiler, "run", source], environment, directory)
        if (actual.returncode != 1 or actual.stdout != source.with_suffix(".stdout").read_text()
                or actual.stderr != source.with_suffix(".stderr").read_text()):
            raise AssertionError(f"{source.name}: exit={actual.returncode}, stdout={actual.stdout!r}, stderr={actual.stderr!r}")
    return len(cases)


def main():
    """Compare exact native output, then verify invalid programs preserve output files."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-bin", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    compiler = parser.parse_args().rust_bin.resolve()
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted((ROOT / "compiler-rs/tests/numeric").glob("*.fn"))
    cases += sorted((ROOT / "compiler-rs/tests/literals").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-numeric-") as temporary:
        directory = Path(temporary)
        for source in cases:
            expected = source.with_suffix(".stdout").read_text()
            actual = run([compiler, "run", source], environment, directory)
            if actual.returncode or actual.stdout != expected:
                raise AssertionError(f"{source.name}: exit={actual.returncode}, stdout={actual.stdout!r}\n{actual.stderr}")
        fault_count = runtime_faults(compiler, environment, directory)
        for name, text in INVALID.items():
            source = directory / f"{name}.fn"
            source.write_text(text)
            output = directory / "preserved-output"
            output.write_text("existing output")
            actual = run([compiler, "build", source, "-o", output], environment, directory)
            if (actual.returncode != 1 or "panicked" in actual.stderr
                    or "error:" not in actual.stderr or output.read_text() != "existing output"):
                raise AssertionError(f"{name}: {actual.stderr}")
    print(f"Rust numeric/literals passed: {len(cases)} native programs, {fault_count} runtime faults, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
