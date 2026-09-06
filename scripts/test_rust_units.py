#!/usr/bin/env python3
"""Execute source-owned unit tests through checked native entries."""
import os
from pathlib import Path
import tempfile
from test_rust_numeric import run

ROOT = Path(__file__).resolve().parents[1]


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    environment = dict(os.environ)
    environment.setdefault("FERN_QBE", str(ROOT / "bin/fern-qbe"))
    environment.setdefault("FERN_RUNTIME_LIB", str(ROOT / "bin/libfern_runtime.a"))
    append = '''fn append(text:String)->():
    match fs.append("events",text):
        Ok(_) -> ()
        Err(_) -> ()
'''
    cases = [
        ("early successful process exit", "fn test_exit()->Result((),Int):\n    System.exit(0)\n    Err(1)\n", False, "System.exit cannot terminate a test", None),
        ("first class process exit", "fn test_exit()->():\n    let stop=System.exit\n    stop(0)\n    ()\n", False, "System.exit cannot terminate a test", None),
        ("unused process exit", "fn main():System.exit(0)\nfn test_ok():()\n", True, "1/1 passed", None),
        ("newtype capabilities and errors", 'newtype Key=Key(String)\nfn test_keys()->Result((),Key):\n    if List.contains([Key("fern")],Key("fer"+"n")):Ok(())\n    else:Err(Key("missing"))\n', True, "1/1 passed", None),
        ("private helper", "fn helper(x:Int)->Int:x+1\nfn test_answer()->Result((),Int):\n    if helper(41)==42:Ok(())\n    else:Err(1)\n", True, "1/1 passed", None),
        ("main direct and closure", append + '''fn main():
    defer append("x")
    4.25
fn test_main()->Result((),Int):
    main()
    let action=main
    action()
    let text=fs.read("events")?
    if text=="xx":Ok(())
    else:Err(1)
''', True, "1/1 passed", "xx"),
        ("continue after failure", append + "fn test_first()->Result((),Int):Err(9)\nfn test_later():append(\"x\")\n", False, "1/2 passed", "x"),
        ("parameter failure continues", append + 'fn test_bad(value:Int):()\nfn test_later():append("x")\n', False, "1/2 passed", "x"),
        ("fault cleanup and continuation", append + '''fn test_fault()->():
    defer append("c")
    println(1/0)
    ()
fn test_later():append("x")
''', False, "1/2 passed", "cx"),
        ("combined docs", '@doc """```fern\nhelper() # => 42\n```"""\nfn helper()->Int:42\nfn test_unit():()\n', True, "2/2 passed", None),
        ("bad result", "fn test_bad():false\n", False, "Unit or Result", None),
        ("integer truncation", "fn test_bad():256\n", False, "Unit or Result", None),
        ("generic specialization", "fn test_generic()->Result((),e):Ok(())\nfn demand()->Result((),String):test_generic()\n", False, "cannot be generic", None),
        ("timeout", "fn spin(n:Int)->Int:spin(n+1)\nfn test_timeout()->():\n    spin(0)\n    ()\n", False, "timed out", None),
        ("output limit", 'fn spam(n:Int)->Int:\n    println("Fern output")\n    spam(n+1)\nfn test_output()->():\n    spam(0)\n    ()\n', False, "output limit", None),
    ]
    with tempfile.TemporaryDirectory(prefix="fern-unit-native-") as temporary:
        directory = Path(temporary)
        path = directory / "library.fn"
        for name, source, success, message, effects in cases:
            (directory / "events").unlink(missing_ok=True)
            path.write_text(source)
            result = run([compiler, "test", "--timeout", "1", path], environment, directory)
            assert result.returncode == (0 if success else 1), (name, result)
            assert message in result.stdout + result.stderr, (name, result)
            assert "panicked" not in result.stderr, (name, result)
            assert path.read_text() == source
            if effects is not None:
                assert (directory / "events").read_text() == effects, (name, result)
        path.write_text('@doc """```fern\nSystem.exit(0)\n1 # => 2\n```"""\nfn library():()\n')
        result = run([compiler, "test", "--doc", path], environment, directory)
        assert result.returncode == 1 and "System.exit cannot terminate a test" in result.stderr, result
        path.write_text('import helper\nfn test_local()->Result((),Int):\n    if helper.value()==42:Ok(())\n    else:Err(1)\n')
        (directory / "helper.fn").write_text('pub fn value()->Int:42\nfn test_remote()->Result((),Int):Err(1)\n')
        result = run([compiler, "test", path], environment, directory)
        assert result.returncode == 0 and "1/1 passed" in result.stdout, result
        result = run([compiler, "test", directory], environment, directory)
        assert result.returncode == 1 and "1/2 passed" in result.stdout and "test_remote" in result.stderr, result
        path.write_text('@doc """```fern\n42 # => 42\n```"""\nfn test_with_argument(value:Int):()\n')
        result = run([compiler, "test", "--doc", path], environment, directory)
        assert result.returncode == 0 and "doc tests: 1/1 passed" in result.stdout, result
    print(f"Rust native unit tests passed: {len(cases)} cases, imported source identity, directory totals and doc-only selection")


if __name__ == "__main__":
    main()
