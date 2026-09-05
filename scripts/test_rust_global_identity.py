#!/usr/bin/env python3
"""Native module aliases retain identity while source-root shadowing stays lexical."""
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ, FERN_QBE=str(ROOT / "bin/fern-qbe"),
                       FERN_RUNTIME_LIB=str(ROOT / "bin/libfern_runtime.a"))
    cases = [
        ("pub fn value(x:a)->a:x\n", "fn main():\n    let model=41\n    let direct=m.value(model)\n    let callable=m.value\n    let piped=model |> m.value()\n    let capture=() -> m.value(model)\n    println(direct+callable(piped)+capture())\n", "123\n"),
        ("pub fn value(x:Int)->Int:x+1\n", "fn caller(model):m.value(model)\nfn main():println(caller(3))\n", "4\n"),
        ("pub fn value(x:Int)->Int:x+1\n", "type Callbacks:\n    value: fn(Int)->Int\nfn main():\n    let m=Callbacks((x)->x+2)\n    println(m.value(1))\n", "3\n"),
        ("pub fn apply(f:fn(Int)->Int,x:Int)->Int:f(x)\n", "type Number=Int\nfn main():\n    let model=3\n    println(m.apply((x:Number)->x+1,model))\n    println(model |> m.apply((x:Number)->x+2,_))\n", "4\n5\n"),
    ]
    with tempfile.TemporaryDirectory(prefix="fern-global-identity-") as temporary:
        directory = Path(temporary)
        source = directory / "main.fn"
        model = directory / "model.fn"
        for library, body, expected in cases:
            model.write_text(library)
            source.write_text("import model as m\n" + body)
            result = run([compiler, "run", source], environment, directory)
            assert (result.returncode, result.stdout, result.stderr) == (0, expected, ""), result
        model.write_text("pub fn value(x:Int)->Int:x+1\n")
        invalid = [
            "fn main():\n    let m=3\n    println(m.value(1))\n",
            'fn main():\n    let model="wrong"\n    println(m.value(model))\n',
            'fn main():\n    let model="wrong"\n    println(model |> m.value())\n',
            'fn main():\n    let model="wrong"\n    println((() -> m.value(model))())\n',
        ]
        for index, body in enumerate(invalid):
            source.write_text("import model as m\n" + body)
            output = directory / "preserved"
            output.write_text("original artifact")
            result = run([compiler, "build", source, "-o", output], environment, directory)
            assert result.returncode == 1 and "error:" in result.stderr and "panicked" not in result.stderr, result
            assert output.read_text() == "original artifact"
            if index:
                assert "String" in result.stderr and "field access" not in result.stderr, result
    print(f"Rust module identities passed: {len(cases)} native programs, {len(invalid)} invalid programs")


if __name__ == "__main__":
    main()
