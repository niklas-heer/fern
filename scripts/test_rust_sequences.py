#!/usr/bin/env python3
"""Native list/tuple pattern behavior and explicit failure-handling requirements."""
import os
from pathlib import Path
import tempfile

from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "rest_middle": 'fn main(): match [1, 2]:\n    [..rest, last] -> println(last)\n',
    "rest_constructor": 'fn main(): match [1]:\n    [..Some(x)] -> println(x)\n',
    "rest_twice": 'fn main(): match [1]:\n    [..a, ..b] -> println(0)\n',
    "missing_empty": 'fn main(): match [1]:\n    [x, .._] -> println(x)\n',
    "only_fixed": 'fn main(): match [1]:\n    [] -> println(0)\n    [x] -> println(x)\n',
    "guard_not_coverage": 'fn main(): match [1]:\n    [] -> println(0)\n    [x, .._] if x > 0 -> println(x)\n',
    "missing_false": 'fn main(): match [true]:\n    [] -> println(0)\n    [true, .._] -> println(1)\n',
    "tuple_arity": 'fn main(): let (a, b, ..rest) = (1,)\n',
    "refutable_let": 'fn main(): let [first, ..rest] = [1]\n',
    "refutable_for": 'fn main(): for [first, ..rest] in [[1]]: println(first)\n',
    "refutable_with": 'fn main() -> Result((), String):\n    with\n        [first, ..rest] <- Ok([1])\n    do\n        Ok(())\n',
    "discard_result_prefix": 'fn main():\n    let items: List(Result(Int, String)) = [Ok(1)]\n    match items:\n        [] -> println(0)\n        [_, ..rest] -> println(List.len(rest))\n',
    "discard_result_rest": 'fn main():\n    let items: List(Result(Int, String)) = [Ok(1)]\n    match items:\n        [.._] -> println(0)\n',
    "discard_result_tuple": 'fn main():\n    let item: (Int, Result(Int, String)) = (1, Ok(2))\n    let (head, .._) = item\n    println(head)\n',
    "wide_pattern": 'fn main(): match [1]:\n    [' + ', '.join(['_'] * 129) + '] -> println(0)\n    _ -> println(1)\n',
}


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted((ROOT / "compiler-rs/tests/sequences").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-sequences-") as temporary:
        directory = Path(temporary)
        for path in cases:
            result = run([compiler, "run", path], environment, directory)
            expected = (0, path.with_suffix(".stdout").read_text(), "")
            assert (result.returncode, result.stdout, result.stderr) == expected, (path.name, result)
        for name, source in INVALID.items():
            path = directory / f"{name}.fn"
            path.write_text(source)
            output = directory / "preserved-output"
            output.write_text("existing output")
            result = run([compiler, "build", path, "-o", output], environment, directory)
            assert (result.returncode == 1 and "error:" in result.stderr
                    and "panicked" not in result.stderr and output.read_text() == "existing output"), (name, result)
    print(f"Rust sequence patterns passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
