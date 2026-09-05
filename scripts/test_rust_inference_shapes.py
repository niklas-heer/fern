#!/usr/bin/env python3
"""Verify delayed shape inference preserves native evaluation and error behavior."""
import os
from pathlib import Path
import tempfile

from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    'unknown_record': 'fn read(value) -> value.field\nfn main(): ()\n',
    'unknown_tuple': 'fn read((first, ..tail)) -> first\nfn main(): ()\n',
    'unknown_iterable': 'fn visit(items): for item in items: println(item)\nfn main(): ()\n',
    'missing_field': 'type Box:\n    value: Int\nfn consume(x: Box) -> Unit: ()\nfn read(x):\n    let value = x.missing\n    consume(x)\n    value\nfn main(): ()\n',
    'invalid_update': 'type Box:\n    value: Int\nfn consume(x: Box) -> Unit: ()\nfn read(x):\n    let changed = %{x | value: true}\n    consume(x)\n    changed\nfn main(): ()\n',
    'invalid_suffix': 'fn read((first, ..tail)) -> Int: tail + first\nfn main(): ()\n',
    'ignored_result': 'type Box:\n    value: Result(Int, String)\nfn consume(x: Box) -> Unit: match x.value:\n    Ok(_) -> ()\n    Err(_) -> ()\nfn read(x):\n    let ignored = x.value\n    consume(x)\n    ()\nfn main(): ()\n',
    'captured_result': 'type Box:\n    value: Result(Int, String)\nfn consume(x: Box) -> Unit: match x.value:\n    Ok(_) -> ()\n    Err(_) -> ()\nfn read(x):\n    let callback = () -> x.value\n    consume(x)\n    callback\nfn main(): ()\n',
}


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted((ROOT / "compiler-rs/tests/inference_shapes").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-inference-shapes-") as temporary:
        directory = Path(temporary)
        for source in cases:
            result = run([compiler, "run", source], environment, directory)
            expected = (0, source.with_suffix(".stdout").read_text(), "")
            assert (result.returncode, result.stdout, result.stderr) == expected, (source.name, result)
        for name, text in INVALID.items():
            source = directory / f"{name}.fn"
            source.write_text(text)
            output = directory / "preserved-output"
            output.write_text("existing output")
            result = run([compiler, "build", source, "-o", output], environment, directory)
            assert (result.returncode == 1 and "error:" in result.stderr
                    and "panicked" not in result.stderr and output.read_text() == "existing output"), (name, result)
    print(f"Rust inferred shapes passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
