#!/usr/bin/env python3
"""Entry results, source-level inference and guarded runtime access contracts."""
import argparse
import os
from pathlib import Path
import tempfile

from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "main_ok": ('fn main() -> Result((), String):\n    defer println("done")\n    Ok(())\n', 0, "done\n", ""),
    "main_err": ('fn main() -> Result((), String):\n    defer println("done")\n    Err("missing")\n', 1, "done\n", "fern: main returned Err\n"),
    "main_custom": ('type Problem:\n    Missing(String)\n    Invalid(Int)\nfn main() -> Result((), Problem): Err(Missing("file"))\n', 1, "", "fern: main returned Err\n"),
    "main_propagate": ('fn read() -> Result(Int, Int): Err(9223372036854775807)\nfn main() -> Result((), Int):\n    defer println("done")\n    let value = read()?\n    println(value)\n    Ok(())\n', 1, "done\n", "fern: main returned Err\n"),
    "main_fault": ('fn main() -> Result((), String):\n    defer println(1 / 0)\n    Err("original")\n', 1, "", "fern: runtime error: integer division by zero\n"),
    "infer_forward": ('fn answer(): twice(21)\nfn twice(x: Int): x * 2\nfn main(): println(answer())\n', 0, "42\n", ""),
    "infer_recursive": ('fn identity(x: a, n: Int):\n    if n > 0: identity(x, n - 1)\n    else: x\nfn main():\n    println(identity(2.5, 3))\n    println(identity(7, 2))\n', 0, "2.5\n7\n", ""),
    "infer_update": ('type Box:\n    value: Int\nfn read(): %{make() | value: 43}\nfn make(): Box(42)\nfn main(): println(read().value)\n', 0, "43\n", ""),
    "index_negative": ('fn main():\n    defer println("done")\n    println(List.get([7], -1))\n', 1, "done\n", "fern: runtime error: list index out of bounds\n"),
    "index_high": ('fn main():\n    defer println("done")\n    let at: (List(Float), Int) -> Float = List.get\n    println(at([7.0], 9223372036854775807))\n', 1, "done\n", "fern: runtime error: list index out of bounds\n"),
    "head_empty": ('fn main():\n    defer println("done")\n    let first: (List(String)) -> String = List.head\n    println(first([]))\n', 1, "done\n", "fern: runtime error: head of empty list\n"),
    "repeat_overflow": ('fn main():\n    defer println("done")\n    println(String.repeat("abcd", 4611686018427387904))\n', 1, "done\n", "fern: runtime error: string size limit exceeded\n"),
    "repeat_indirect": ('fn main():\n    defer println("done")\n    let repeat: (String, Int) -> String = String.repeat\n    println(repeat("x", 16777217))\n', 1, "done\n", "fern: runtime error: string size limit exceeded\n"),
    "repeat_empty": ('fn main():\n    println(String.len(String.repeat("", 9223372036854775807)))\n    println(String.len(String.repeat("abc", -1)))\n    println(String.len(String.repeat("🌿", 3)))\n', 0, "0\n0\n12\n", ""),
    "valid_access": ('fn main():\n    println(List.get([0.0, 9.25], 1))\n    println(List.head([9223372036854775807]))\n    println(List.get(["a", "🌿"], 1))\n', 0, "9.25\n9223372036854775807\n🌿\n", ""),
    "callback_fault": ('fn take(xs: List(Int)) -> Int:\n    defer println("callback")\n    List.head(xs)\nfn main():\n    defer println("main")\n    println(List.len(List.map([[], [7]], take)))\n', 1, "callback\nmain\n", "fern: runtime error: head of empty list\n"),
    "slice_boundary": ('fn main():\n    defer println("done")\n    println(String.slice("é", 0, 1))\n', 1, "done\n", "fern: runtime error: String.slice indices must be UTF-8 character boundaries\n"),
    "slice_indirect": ('fn main():\n    defer println("done")\n    let slice: (String, Int, Int) -> String = String.slice\n    println(slice("é", 1, 1))\n', 1, "done\n", "fern: runtime error: String.slice indices must be UTF-8 character boundaries\n"),
    "slice_clamp": ('fn main():\n    println(String.slice("aé🌿z", 1, 7))\n    println(String.slice("é", -9, 99))\n    println(String.len(String.slice("é", 99, -9)))\n', 0, "é🌿\né\n0\n", ""),
    "split_scalars": ('fn main():\n    for part in String.split("aé🌿é", ""): println(part)\n    println(List.len(String.split("", "")))\n', 0, "a\né\n🌿\ne\ń\n0\n", ""),
    "split_invalid_bytes": ('fn main():\n    defer println("done")\n    let text = Result.unwrap_or(File.read("invalid-utf8.txt"), "")\n    println(List.len(String.split(text, "")))\n', 1, "done\n", "fern: runtime error: String.split requires valid UTF-8 input\n"),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", choices=list(CASES))
    args = parser.parse_args()
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = {args.only: CASES[args.only]} if args.only else CASES
    with tempfile.TemporaryDirectory(prefix="fern-boundaries-") as temporary:
        directory = Path(temporary)
        (directory / "invalid-utf8.txt").write_bytes(b"\xc0\xaf")
        for name, (source, code, stdout, stderr) in cases.items():
            path = directory / f"{name}.fn"
            path.write_text(source)
            result = run([compiler, "run", path], environment, directory)
            assert (result.returncode, result.stdout, result.stderr) == (code, stdout, stderr), (name, result)
    print(f"Rust entry/access contracts passed: {len(cases)} native programs")


if __name__ == "__main__":
    main()
