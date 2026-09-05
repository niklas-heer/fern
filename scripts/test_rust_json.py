#!/usr/bin/env python3
"""Execute the finite Rust native JSON migration contract; C source stays legacy."""
import json
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    fixtures = ROOT / "compiler-rs/tests/json_values"
    cases = json.loads((fixtures / "cases.json").read_text())
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    with tempfile.TemporaryDirectory(prefix="fern-json-native-") as temporary:
        directory = Path(temporary)
        for name, expected in cases["native"].items():
            source = fixtures / "valid" / f"{name}.fn"
            result = run([compiler, "run", source], environment, directory)
            actual = (result.returncode, result.stdout, result.stderr)
            wanted = (expected["exit"], expected["stdout"], expected["stderr"])
            assert actual == wanted, (name, actual, wanted)
        for name in cases["invalid"]:
            source = fixtures / "invalid" / f"{name}.fn"
            output = directory / "preserved"
            output.write_text("previous artifact")
            result = run([compiler, "build", source, "-o", output], environment, directory)
            assert result.returncode == 1 and "error:" in result.stderr, (name, result)
            assert "panicked" not in result.stderr, (name, result.stderr)
            assert output.read_text() == "previous artifact"
    print(f"Rust JSON: {len(cases['native'])} native programs, {len(cases['invalid'])} semantic rejections")


if __name__ == "__main__":
    main()
