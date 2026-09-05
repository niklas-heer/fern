#!/usr/bin/env python3
"""Run pattern-anchored parameter inference through native execution and rejection."""
import os
from pathlib import Path
import tempfile

from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "public_pattern": 'pub fn f(0) -> Int: 0\npub fn f(n) -> Int: n\nfn main(): ()\n',
    "mixed_scalars": 'fn f(0) -> 0\nfn f(true) -> 1\nfn main(): ()\n',
    "mixed_payloads": 'fn f(Some(0)) -> 0\nfn f(Some(true)) -> 1\nfn f(None) -> 2\nfn main(): ()\n',
    "annotation_conflict": 'fn f(0) -> 0\nfn f(n: Bool) -> 1\nfn main(): ()\n',
    "tuple_conflict": 'fn f((0, true)) -> 0\nfn f((n, flag, x)) -> 1\nfn main(): ()\n',
    "unanchored_rest": 'fn f((head, ..tail)) -> head\nfn main(): println(f((1, true)))\n',
    "discard_result": 'fn f(Ok(0)) -> 0\nfn f(Err("missing")) -> 1\nfn f(_) -> 2\nfn main(): ()\n',
}


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted((ROOT / "compiler-rs/tests/parameters").glob("*.fn"))
    cases += [ROOT / "compiler-rs/tests/parameters/project/main.fn"]
    with tempfile.TemporaryDirectory(prefix="fern-parameters-") as temporary:
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
    print(f"Rust parameter inference passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
