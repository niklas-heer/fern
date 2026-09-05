#!/usr/bin/env python3
"""Native specification tests for Rust collections and error-value migration."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "lists": "100\n3\n40\n65\ntrue\n",
    "string_lists": "Ada\n林\n林\ntrue\nfalse\n",
    "bool_lists": "true\nfalse\ntrue\n2\n",
    "nested_lists": "-9223372036854775808\n9223372036854775807\n4294967296\n3\n2\n",
    "option_int": "-9223372036854775808\n9\ntrue\ntrue\n9223372036854775807\n",
    "option_string": "Hello, Fern\nHello, stranger\nretained pointer\n",
    "results": "ok: payload\nerror: unavailable\ntrue\ntrue\nfallback\n",
    "result_int": "-9223372036854775808\n9223372036854775807\n",
    "nested_sums": "nested\nfalse\n",
    "scalar_matches": "negative\nother\nlanguage\nother: tree\nyes\n",
    "match_scopes": "outside/inside!\noutside/outside\n",
    "unit_results": "saved\nfailed\n",
    "match_once": "evaluated\ncorrect\n",
    "inference": "0\ninferred\ncontext\n0\n",
    "multiline": "first\n",
    "propagation": "continued\nfour\ndivision by zero\n",
    "nested_propagation": "inner error\nbranch!\ninner error\n",
    "short_circuit_propagation": "false\nprobe\nfailed\n",
    "allocation_stress": "1500\nretained 🌿\nretained 🌿\n",
}
INVALID = {
    "missing_none": 'fn main():\n    match Some(1):\n        Some(n) -> println(n)\n',
    "missing_err": 'fn f(r: Result(Int, String)) -> Int:\n    match r:\n        Ok(n) -> n\nfn main(): 0\n',
    "mixed_list": 'fn main():\n    let values = [1, true]\n',
    "wrong_payload": 'fn f() -> Option(String): Some(1)\nfn main(): 0\n',
    "wrong_constructor": 'fn f() -> Option(Int): Ok(1)\nfn main(): 0\n',
    "escaped_binding": 'fn main():\n    match Some(1):\n        Some(n) -> println(n)\n        None -> println(0)\n    println(n)\n',
    "unresolved_none": 'fn main():\n    let value = None\n',
    "unresolved_list": 'fn main():\n    let value = []\n',
    "ignored_result": 'fn f() -> Result(Int, String): Ok(1)\nfn main():\n    f()\n    println("ignored")\n',
    "ignored_last_result": 'fn f() -> Result(Int, String): Ok(1)\nfn main(): f()\n',
    "unused_result_binding": 'fn f() -> Result(Int, String): Ok(1)\nfn main():\n    let ignored = f()\n',
    "unused_result_parameter": 'fn ignore(value: Result(Int, String)) -> Unit: ()\nfn main(): ignore(Err("lost"))\n',
    "unused_result_pattern": 'fn main():\n    let nested: Option(Result(Int, String)) = Some(Err("lost"))\n    match nested:\n        Some(result) -> println("ignored")\n        None -> println("empty")\n',
    "compound_equality": 'fn main(): println([1] == [1])\n',
    "nested_contains": 'fn main(): println(List.contains([[1]], [1]))\n',
    "unreachable_arm": 'fn main():\n    match true:\n        _ -> println(0)\n        true -> println(1)\n',
    "branch_type": 'fn main():\n    let value = match true:\n        true -> 1\n        false -> "no"\n',
    "incomplete_nested_pattern": 'fn main():\n    match Some(Some(1)):\n        Some(Some(n)) -> println(n)\n        None -> println(0)\n',
    "incomplete_guard": 'fn main():\n    match 1:\n        n if n > 0 -> println(n)\n',
    "try_outside_result": 'fn value() -> Result(Int, String): Ok(1)\nfn main(): println(value()?)\n',
    "try_wrong_error": 'fn value() -> Result(Int, String): Err("failure")\nfn other() -> Result(Int, Int): Ok(value()?)\nfn main(): 0\n',
    "try_non_result": 'fn value() -> Result(Int, String): Ok(1?)\nfn main(): 0\n',
}


def run(argv, environment, directory):
    """Run a literal command with a bounded deadline and capture diagnostic output."""
    return subprocess.run(list(map(str, argv)), cwd=directory, env=environment,
                          capture_output=True, text=True, timeout=30)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-bin", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    options = parser.parse_args()
    compiler = options.rust_bin.resolve()
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    with tempfile.TemporaryDirectory(prefix="fern-collections-") as temporary:
        directory = Path(temporary)
        for name, expected in CASES.items():
            source = ROOT / "compiler-rs/tests/collections" / f"{name}.fn"
            actual = run([compiler, "run", source], environment, directory)
            if actual.returncode or actual.stdout != expected:
                raise AssertionError(f"{name}: exit={actual.returncode}, stdout={actual.stdout!r}\n{actual.stderr}")
        # A deterministic oracle exercises wide signed list/Option payloads and immutable aliases.
        rng = random.Random(0xC011EC7)
        for index in range(12):
            values = [rng.randrange(-(2**50), 2**50) for _ in range(4)]
            source = directory / f"generated-{index}.fn"
            source.write_text(
                f"fn main():\n    let original = [{', '.join(map(str, values))}]\n"
                f"    let changed = List.push(original, {index})\n"
                "    println(Option.unwrap_or(Some(List.head(original)), 0))\n"
                "    println(List.head(List.reverse(changed)))\n"
                "    println(List.len(original))\n"
            )
            actual = run([compiler, "run", source], environment, directory)
            if actual.returncode or actual.stdout != f"{values[0]}\n{index}\n4\n":
                raise AssertionError(f"generated-{index}: {actual.stdout!r}\n{actual.stderr}")
        for name, text in INVALID.items():
            source = directory / f"{name}.fn"
            source.write_text(text)
            output = directory / "preserved-output"
            output.write_text("existing output")
            actual = run([compiler, "build", source, "-o", output], environment, directory)
            if (actual.returncode != 1 or "panicked" in actual.stderr
                    or f"{source}:" not in actual.stderr or "error:" not in actual.stderr
                    or output.read_text() != "existing output"):
                raise AssertionError(f"{name} must diagnose without overwriting: {actual.stderr}")
    print(f"Rust collections passed: {len(CASES) + 12} native programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
