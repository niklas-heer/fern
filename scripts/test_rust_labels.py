#!/usr/bin/env python3
"""Exercise optional source-call labels through native execution and atomic rejection."""
import argparse
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "compiler-rs/tests/labels_native"
SENTINEL = b"existing executable\x00must survive failed label checking\xff"


def accepted(compiler, environment, directory, source):
    """Compare native status, stdout and stderr against the adjacent exact output fixture."""
    actual = run([compiler, "run", source], environment, directory)
    expected = (0, source.with_suffix(".stdout").read_text(encoding="utf-8"), "")
    assert (actual.returncode, actual.stdout, actual.stderr) == expected, (source.name, actual)


def rejected(compiler, environment, directory, source):
    """A source error must preserve an existing output's bytes, mode and modification time."""
    output = directory / "preserved-output"
    output.write_bytes(SENTINEL)
    output.chmod(0o751)
    before = output.stat()
    actual = run([compiler, "build", source, "-o", output], environment, directory)
    assert (actual.returncode == 1 and actual.stdout == "" and "error:" in actual.stderr
            and "panicked" not in actual.stderr), (source.name, actual)
    assert output.is_file() and output.read_bytes() == SENTINEL, source.name
    after = output.stat()
    assert (after.st_mode, after.st_mtime_ns) == (before.st_mode, before.st_mtime_ns), source.name


def main():
    """Use existing tools only; every compiler and executable process has a bounded deadline."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-bin", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    compiler = parser.parse_args().rust_bin.resolve()
    environment = dict(os.environ)
    environment.setdefault("FERN_QBE", str(ROOT / "bin/fern-qbe"))
    environment.setdefault("FERN_RUNTIME_LIB", str(ROOT / "bin/libfern_runtime.a"))
    sources = sorted(FIXTURES.glob("*.fn"))
    positive = [source for source in sources if not source.name.startswith("invalid_")]
    negative = [source for source in sources if source.name.startswith("invalid_")]
    assert positive and negative, "label native corpus is missing"
    assert set(FIXTURES.glob("*.stdout")) == {source.with_suffix(".stdout") for source in positive}
    with tempfile.TemporaryDirectory(prefix="fern-label-native-") as temporary:
        directory = Path(temporary)
        for source in positive:
            accepted(compiler, environment, directory, source)
        for source in negative:
            rejected(compiler, environment, directory, source)
    print(f"Rust optional labels passed: {len(positive)} native programs, {len(negative)} atomic invalid programs")


if __name__ == "__main__":
    main()
