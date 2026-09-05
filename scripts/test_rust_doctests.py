#!/usr/bin/env python3
"""Execute documentation expectations through the real checked native pipeline."""
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
        ("private helpers", "let answer=add(2,3)\nanswer # => 5\nSome(answer) # => Some(_)",
         "fn add(a: Int,b: Int)->Int: a+b\nfn main()->Int:99", True, "1/1 passed"),
        ("mismatch", "add(2,3) # => 6", "fn add(a: Int,b: Int)->Int: a+b", False, "example 1"),
        ("timeout", "spin(0)", "fn spin(n: Int)->Int: spin(n+1)", False, "timed out"),
        ("output limit", "spam(0)", 'fn spam(n: Int)->Int:\n    println("Fern output")\n    spam(n+1)', False, "output limit"),
        ("original Unit entry", "main() # => ()", "fn main(): 42", True, "1/1 passed"),
        ("original Result entry", "main() # => Err(7)", "fn main()->Result((),Int):Err(7)", True, "1/1 passed"),
        ("original Int entry", "main() # => 42", "fn main()->Int:42", True, "1/1 passed"),
        ("evaluate once", "tick() # => Ok(1)", 'fn tick()->Result(Int,Int):\n    let written=fs.append("count", "x")?\n    Ok(1)', True, "1/1 passed"),
        ("literal markers", '"# => literal" # => "# => literal"', "fn library():()", True, "1/1 passed"),
    ]
    with tempfile.TemporaryDirectory(prefix="fern-doc-native-") as temporary:
        directory = Path(temporary)
        path = directory / "library.fn"
        for name, code, body, success, message in cases:
            source = f'@doc """\n```fern\n{code}\n```\n"""\n{body}\n'
            path.write_text(source)
            result = run([compiler, "test", "--doc", "--timeout", "1", path], environment, directory)
            assert (result.returncode == (0 if success else 1)
                    and message in result.stdout + result.stderr
                    and "panicked" not in result.stderr), (name, result)
            assert path.read_text() == source
            if name == "evaluate once":
                assert (directory / "count").read_text() == "x"
        (directory / "helper.fn").write_text("pub fn value()->Int:42\n")
        path.write_text('import helper as h\n@doc """```fern\nlet value=h.value()\nvalue # => 42\n```\n```fern\nlet value="Fern"\nvalue # => "Fern"\n```"""\nfn library():()\n')
        result = run([compiler, "test", "--doc", directory], environment, directory)
        assert result.returncode == 0 and "2/2 passed" in result.stdout, result
        (directory / "helper.fn").write_text("pub fn main()->Int:42\n")
        path.write_text('import helper.{main}\n@doc """```fern\nmain() # => 42\n```"""\nfn library():()\n')
        result = run([compiler, "test", "--doc", path], environment, directory)
        assert result.returncode == 0 and "1/1 passed" in result.stdout, result
    print(f"Rust native doctests passed: {len(cases)} execution cases and independent imported examples")


if __name__ == "__main__":
    main()
