#!/usr/bin/env python3
"""Native specification gate for generic types, nested matches and project modules."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "tuples": "once\n9223372036854775807\ntrue\n🌿\n1.25\nswapped\n42\n7\n42\n0\n-1\n",
    "interpolation": "Hello 🌿, answer=42!\n9223372036854775807 -9223372036854775808\nbool=true/false float=1.25\nliteral {brace}; nested 🌿 # text\n1\n2\n1:2\ngeneric=42\n",
    "floats": "3.125\n3.5\ntrue\ntrue\ntrue\ntrue\n1.5\n2.25\n4.5\ntrue\n",
    "pipes": "input\nconfig\n10\n",
    "generic_identity": "42\ngeneric\ntrue\n",
    "custom_sum": "active\nnow\nwaiting: approval\n",
    "mutual_result_tree": "false\ntrue\ntrue\n",
    "mutual_result_list": "false\ntrue\nfalse\ntrue\n",
    "mutual_result_wrapped_map": "false\ntrue\n",
    "recursive_tree": "4294967338\n4294967296\nretained 🌿\n",
    "records": "9223372036854775807\nhello\ntrue\nfallback\n",
    "nested_guards": "yes\nno\ninner empty\nouter empty\n1\n2\n2\n",
    "project/main": "Fern inventory\n42\n",
}
INVALID = {
    "missing_nested": "fn main():\n    match Some(true):\n        Some(true) -> println(1)\n        None -> println(0)\n",
    "guard_not_total": "fn main():\n    match true:\n        x if x -> println(1)\n",
    "duplicate_bindings": "type Pair:\n    P(Int, Int)\nfn main():\n    match P(1, 2):\n        P(x, x) -> println(x)\n",
    "unknown_type": "fn value(x: Missing) -> Missing: x\nfn main(): 0\n",
    "generic_mismatch": "fn add(x: a, y: a) -> a: x\nfn main(): println(add(x: 1, y: true))\n",
    "nominal_mismatch": "type A:\n    value: Int\ntype B:\n    value: Int\nfn main():\n    let a: A = B(1)\n",
    "escaping_guard": "fn main():\n    match Some(1):\n        Some(x) if x > 0 -> println(x)\n        _ -> println(x)\n",
}


def invoke(compiler, source, environment):
    return subprocess.run([compiler, "run", source], env=environment,
                          capture_output=True, text=True, timeout=30)


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    for name, expected in CASES.items():
        result = invoke(compiler, ROOT / "compiler-rs/tests/types" / f"{name}.fn", environment)
        if result.returncode or result.stdout != expected:
            raise AssertionError(f"{name}: {result.returncode}, {result.stdout!r}\n{result.stderr}")
    with tempfile.TemporaryDirectory(prefix="fern-types-") as temporary:
        for name, source in INVALID.items():
            path = Path(temporary) / f"{name}.fn"
            path.write_text(source)
            result = invoke(compiler, path, environment)
            if result.returncode != 1 or "error:" not in result.stderr or "panicked" in result.stderr:
                raise AssertionError(f"{name}: must diagnose invalid source: {result.stderr}")
    print(f"Rust types passed: {len(CASES)} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
