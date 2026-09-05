#!/usr/bin/env python3
"""Execute clause dispatch and million-step direct self-recursion specifications."""
import os
from pathlib import Path
import re
import tempfile

from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]
INVALID = {
    "missing_case": 'fn f(0: Int) -> 1\nfn main(): println(f(0))\n',
    "guard_only": 'fn f(n: Int) if n > 0 -> n\nfn main(): println(f(1))\n',
    "unreachable": 'fn f(n: Int) -> n\nfn f(0: Int) -> 1\nfn main(): ()\n',
    "separated": 'fn f(0: Int) -> 1\nfn other(): ()\nfn f(n: Int) -> n\nfn main(): ()\n',
    "arity": 'fn f(0: Int) -> 1\nfn f(n: Int, x: Int) -> n\nfn main(): ()\n',
    "types": 'fn f(0: Int) -> 1\nfn f(n: String) -> 2\nfn main(): ()\n',
    "returns": 'fn f(0: Int) -> Int: 1\nfn f(n: Int) -> String: "x"\nfn main(): ()\n',
    "visibility": 'pub fn f(0: Int) -> Int: 1\nfn f(n: Int) -> Int: n\nfn main(): ()\n',
    "discard_result": 'fn f(_: Result(Int, String)) -> 1\nfn main(): println(f(Ok(2)))\n',
    "discard_nested": 'fn f([_, .._]: List(Result(Int, String))) -> 1\nfn f([]: List(Result(Int, String))) -> 0\nfn main(): ()\n',
    "guard_type": 'fn f(n: Int) if n -> n\nfn f(n: Int) -> n\nfn main(): ()\n',
}


def check_tail_shape(compiler, source, environment, directory):
    """Require backedges and static scratch allocation before executing deep recursion."""
    result = run([compiler, "emit", source], environment, directory)
    assert result.returncode == 0, (source.name, result.stderr)
    functions = re.findall(r"function [wld] \$(f\d+)\([^\n]*\) \{\n(.*?)\n\}",
                           result.stdout, re.S)
    optimized = [(name, body) for name, body in functions if "@recur\n" in body]
    assert optimized, (source.name, "no self-tail-call elimination")
    for name, body in optimized:
        assert f"call ${name}(" not in body, (source.name, body)
        assert "alloc8" not in body.split("@recur\n", 1)[1], (source.name, body)


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    clauses = sorted((ROOT / "compiler-rs/tests/clauses").glob("*.fn"))
    clauses += [ROOT / "compiler-rs/tests/clauses/project/main.fn"]
    tails = sorted((ROOT / "compiler-rs/tests/tail_calls").glob("*.fn"))
    with tempfile.TemporaryDirectory(prefix="fern-functions-") as temporary:
        directory = Path(temporary)
        for source in clauses + tails:
            if source in tails and source.stem != "own_cleanup":
                check_tail_shape(compiler, source, environment, directory)
            result = run([compiler, "run", source], environment, directory)
            error = source.with_suffix(".stderr")
            expected = (int(error.exists()), source.with_suffix(".stdout").read_text(),
                        error.read_text() if error.exists() else "")
            assert (result.returncode, result.stdout, result.stderr) == expected, (source.name, result)
        for name, text in INVALID.items():
            source = directory / f"{name}.fn"
            source.write_text(text)
            output = directory / "preserved-output"
            output.write_text("existing output")
            result = run([compiler, "build", source, "-o", output], environment, directory)
            assert (result.returncode == 1 and "error:" in result.stderr
                    and "panicked" not in result.stderr and output.read_text() == "existing output"), (name, result)
    print(f"Rust functions passed: {len(clauses)} clause programs, {len(tails)} recursion programs, {len(INVALID)} invalid programs")


if __name__ == "__main__":
    main()
