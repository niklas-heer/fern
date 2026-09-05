#!/usr/bin/env python3
"""Check zero-cost nominal representations through real native execution."""
import os
import json
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def main():
    """Run each literal source fixture with exact output and a process-group deadline."""
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ)
    environment.setdefault("FERN_QBE", str(ROOT / "bin/fern-qbe"))
    environment.setdefault("FERN_RUNTIME_LIB", str(ROOT / "bin/libfern_runtime.a"))
    sources = sorted((ROOT / "compiler-rs/tests/newtypes_native").glob("*.fn"))
    assert sources, "newtype native corpus is missing"
    invalid = json.loads((ROOT / "compiler-rs/tests/newtypes_native/invalid.json").read_text())
    with tempfile.TemporaryDirectory(prefix="fern-newtype-native-") as temporary:
        for source in sources:
            actual = run([compiler, "run", source], environment, Path(temporary))
            expected = source.with_suffix(".stdout").read_text()
            assert (actual.returncode, actual.stdout, actual.stderr) == (0, expected, ""), (source.name, actual)
        for name, source in invalid.items():
            path = Path(temporary) / f"invalid_{name}.fn"
            path.write_text(source)
            actual = run([compiler, "check", path], environment, Path(temporary))
            assert actual.returncode != 0 and "error:" in actual.stderr, (name, actual)
    print(f"Rust newtypes: {len(sources)} native representation programs, {len(invalid)} invalid programs")


if __name__ == "__main__":
    main()
