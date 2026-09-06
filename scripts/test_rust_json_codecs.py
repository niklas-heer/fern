#!/usr/bin/env python3
"""Run typed JSON source/native parity and atomic checking failures using built artifacts."""
import json
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def main():
    """Keep native dependencies explicit; this gate does not build the C frontend or runtime."""
    compiler = Path(os.environ.get("FERN_RS", ROOT / "compiler-rs/target/debug/fern-rs"))
    environment = dict(os.environ)
    environment.setdefault("FERN_QBE", str(ROOT / "bin/fern-qbe"))
    environment.setdefault("FERN_RUNTIME_LIB", str(ROOT / "bin/libfern_runtime.a"))
    corpus = ROOT / "compiler-rs/tests/json_codecs_native"
    sources = sorted(corpus.glob("*.fn"))
    sources += sorted((ROOT / "compiler-rs/tests/json_recursive_native").glob("*.fn"))
    invalid = json.loads((corpus / "invalid.json").read_text())
    assert sources and invalid, "typed JSON corpus is missing"
    with tempfile.TemporaryDirectory(prefix="fern-json-codecs-") as temporary:
        directory = Path(temporary)
        for source in sources:
            actual = run([compiler, "run", source], environment, directory)
            expected = source.with_suffix(".stdout").read_text()
            assert (actual.returncode, actual.stdout, actual.stderr) == (0, expected, ""), (source.name, actual)
        for name, text in invalid.items():
            source = directory / f"{name}.fn"
            source.write_text(text)
            output = directory / "preserved"
            output.write_bytes(b"existing executable")
            output.chmod(0o751)
            before = output.stat()
            actual = run([compiler, "build", source, "-o", output], environment, directory)
            after = output.stat()
            assert actual.returncode != 0 and "error:" in actual.stderr and "panicked" not in actual.stderr, (name, actual)
            assert output.read_bytes() == b"existing executable", name
            assert (before.st_mode, before.st_mtime_ns) == (after.st_mode, after.st_mtime_ns), name
    print(f"Rust typed JSON: {len(sources)} native programs, {len(invalid)} atomic invalid programs")


if __name__ == "__main__":
    main()
