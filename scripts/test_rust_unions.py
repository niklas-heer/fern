#!/usr/bin/env python3
"""Run finite union programs and reject invalid conversions without replacing outputs."""
import argparse
import json
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "compiler-rs/tests/unions_native"


def rejected(compiler, environment, directory, cases):
    """Require parsed invalid source to fail checking while preserving an existing executable."""
    for name, source in cases.items():
        path = directory / (name + ".fn")
        path.write_text(source)
        parsed = run([compiler, "fmt", path], environment, directory)
        assert parsed.returncode == 0, (name, "invalid fixture must parse", parsed)
        output = directory / "existing-output"
        output.write_bytes(b"preserved executable\x00\xff")
        output.chmod(0o751)
        before = output.stat()
        result = run([compiler, "build", path, "-o", output], environment, directory)
        assert result.returncode == 1 and "error:" in result.stderr and "panicked" not in result.stderr, (name, result)
        assert output.read_bytes() == b"preserved executable\x00\xff", name
        after = output.stat()
        assert (after.st_mode, after.st_mtime_ns) == (before.st_mode, before.st_mtime_ns), name


def test_entries(compiler, environment, directory):
    """Result entries preserve union errors, test continuation and explicit process-exit rejection."""
    path = directory / "unit_library.fn"
    path.write_text('''fn append(text: String) -> Unit:
    match fs.append("events", text):
        Ok(_) -> ()
        Err(_) -> ()
fn test_good() -> Result(Unit, Int | String):
    let value: Int | String = "fern"
    match value:
        s: String -> if s == "fern": Ok(()) else: Err(s)
        n: Int -> Err(n)
fn test_failed() -> Result(Unit, Int | String): Err(4294967296)
fn test_exit() -> Result(Unit, Int | String):
    System.exit(0)
    Err("must not pass")
fn test_after(): append("after")
''')
    result = run([compiler, "test", path], environment, directory)
    assert result.returncode == 1 and "2/4 passed" in result.stdout, result
    assert "System.exit cannot terminate a test" in result.stderr, result
    assert "test_failed" in result.stderr and "test_exit" in result.stderr, result
    assert (directory / "events").read_text() == "after"
    for body, status in [("Ok(())", 0), ("Err(4294967296)", 1), ('Err("fern")', 1)]:
        path.write_text("fn main() -> Result(Unit, Int | String): " + body + "\n")
        result = run([compiler, "run", path], environment, directory)
        assert (result.returncode, result.stdout, result.stderr) == (status, "", "fern: main returned Err\n" if status else ""), result


def main():
    """Use explicit native artifacts and process-group deadlines for all fixture execution."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-bin", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    compiler = parser.parse_args().rust_bin.resolve()
    environment = dict(os.environ)
    environment.setdefault("FERN_QBE", str(ROOT / "bin/fern-qbe"))
    environment.setdefault("FERN_RUNTIME_LIB", str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted(FIXTURES.glob("*.fn"))
    invalid = json.loads((FIXTURES / "invalid.json").read_text())
    assert cases and invalid
    with tempfile.TemporaryDirectory(prefix="fern-union-native-") as temporary:
        directory = Path(temporary)
        for path in cases:
            result = run([compiler, "run", path], environment, directory)
            expected = path.with_suffix(".stdout").read_text()
            assert (result.returncode, result.stdout, result.stderr) == (0, expected, ""), (path.name, result)
        rejected(compiler, environment, directory, invalid)
        test_entries(compiler, environment, directory)
    print(f"Rust unions passed: {len(cases)} native programs, {len(invalid)} parsed atomic invalid programs")


if __name__ == "__main__":
    main()
