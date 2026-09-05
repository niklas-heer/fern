#!/usr/bin/env python3
"""Exercise generalized private signatures through native execution and rejection."""
import os
from pathlib import Path
import tempfile

from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    'public_omission': 'pub fn identity(value) -> Int: value\nfn main(): ()\n',
    'rigid_return': 'fn bad(value: a) -> a: 1\nfn main(): ()\n',
    'rigid_collapse': 'fn bad(value: a, other: b) -> a: other\nfn main(): ()\n',
    'unanchored_recursion': 'fn forever(value) -> forever(value)\nfn main(): ()\n',
    'growing_recursion': 'fn grow(value) -> grow([value])\nfn main(): ()\n',
    'self_application': 'fn apply(value) -> value(value)\nfn main(): ()\n',
    'local_monomorphic': 'fn identity(value) -> value\nfn main():\n    let callback = identity\n    println(callback(1))\n    println(callback("wrong"))\n',
    'bool_add': 'fn add(left, right) -> left + right\nfn main(): println(add(true, false))\n',
    'list_add': 'fn add(left, right) -> left + right\nfn main(): println(List.len(add([1], [2])))\n',
    'callback_numeric': 'fn square(value) -> value * value\nfn main(): println(List.len(List.map([true], square)))\n',
    'compound_display': 'fn describe(value) -> "value={value}"\nfn main(): println(describe([1]))\n',
    'float_key': 'fn lookup(key) -> Map.get(%{key: 1}, key)\nfn main(): println(Option.unwrap_or(lookup(1.5), 0))\n',
    'unused_unknown': 'fn broken(value) -> unknown\nfn main(): ()\n',
    'unused_result': 'fn broken(value) ->\n    let ignored: Result(Int, String) = Ok(1)\n    value\nfn main(): ()\n',
}


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = sorted((ROOT / "compiler-rs/tests/whole_signatures").glob("*.fn"))
    cases += [ROOT / "compiler-rs/tests/whole_signatures/project/main.fn"]
    with tempfile.TemporaryDirectory(prefix="fern-whole-signatures-") as temporary:
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
    print(f"Rust whole signatures passed: {len(cases)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
