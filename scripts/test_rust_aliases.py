#!/usr/bin/env python3
"""Run transparent type aliases through native execution and atomic rejection."""
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    directory = ROOT / "compiler-rs/tests/aliases"
    cases = sorted(directory.glob("*.fn")) + [directory / "modules/main.fn"]
    invalid = sorted((directory / "invalid").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-aliases-") as temporary:
        workspace = Path(temporary)
        for source in cases:
            result = run([compiler, "run", source], environment, workspace)
            expected = (0, source.with_suffix(".stdout").read_text(), "")
            assert (result.returncode, result.stdout, result.stderr) == expected, (source, result)
        for source in invalid:
            output = workspace / "preserved-output"
            output.write_text("existing output")
            result = run([compiler, "build", source, "-o", output], environment, workspace)
            assert (result.returncode == 1 and "error:" in result.stderr
                    and "panicked" not in result.stderr and output.read_text() == "existing output"), (source, result)
    print(f"Rust aliases passed: {len(cases)} native programs, {len(invalid)} invalid programs")


if __name__ == "__main__":
    main()
