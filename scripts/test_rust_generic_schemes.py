#!/usr/bin/env python3
"""Validate generic definitions independently of callers and preserve native capabilities."""
import os
from pathlib import Path
import tempfile

from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "nominal_map_key": 'type Bag(a):\n    values: Map(a, Int)\nfn bad(bag: Bag(Float), phantom: a) -> a: phantom\nfn main(): ()\n',
    "unknown_name": 'fn bad(x: a) -> a: missing\nfn main(): ()\n',
    "universal_literal": 'fn bad(x: a) -> a: 1\nfn main(): ()\n',
    "universal_conversion": 'fn bad(x: a) -> b: x\nfn main(): ()\n',
    "concrete_return": 'fn bad(x: a) -> Int: x\nfn main(): ()\n',
    "branch_mismatch": 'fn bad(x: a, flag: Bool) -> a: if flag: x else: "wrong"\nfn main(): ()\n',
    "callback_return": 'fn bad(x: a) -> List(a): List.map([x], (item) -> 0)\nfn main(): ()\n',
    "compound_contains": 'fn bad(x: a) -> Bool: List.contains([[1]], [1])\nfn main(): ()\n',
    "unused_result": 'fn bad(x: a) -> a:\n    let ignored: Result(Int, String) = Ok(1)\n    x\nfn main(): ()\n',
    "nested_coverage": 'fn bad(x: a, ys: List(Bool)) -> a:\n    match ys:\n        [] -> x\n        [true, .._] -> x\nfn main(): ()\n',
    "clause_returns": 'fn bad(Some(x): Option(a)) -> a: 1\nfn bad(None: Option(a)) -> a: "wrong"\nfn main(): ()\n',
    "boolean_numeric": 'fn square(x: a) -> a: x * x\nfn main(): println(square(true))\n',
    "function_numeric": 'fn square(x: a) -> a: x * x\nfn main():\n    let callback: (Bool) -> Bool = square\n    println(callback(true))\n',
    "callback_numeric": 'fn square(x: a) -> a: x * x\nfn main(): println(List.len(List.map([true], square)))\n',
    "compound_display": 'fn describe(x: a) -> String: "value={x}"\nfn main(): println(describe([1]))\n',
    "map_key": 'fn lookup(key: k) -> Option(Int): Map.get(%{key: 1}, key)\nfn main(): println(Option.unwrap_or(lookup(1.5), 0))\n',
}


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted((ROOT / "compiler-rs/tests/generic_schemes").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-generic-schemes-") as temporary:
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
    print(f"Rust generic schemes passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
