#!/usr/bin/env python3
"""Finite native source oracles for explicit file-text Result completion/publication."""
import argparse
import os
from pathlib import Path
import sys
import tempfile
from test_rust_numeric import run
ROOT = Path(__file__).resolve().parents[1]


def main():
    """Build source through a selected frontend and require exact native IO results."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "compiler-rs/target/debug/fern-rs")
    parser.add_argument("--runtime", type=Path, default=ROOT / "bin/libfern_runtime.a")
    parser.add_argument("--qbe", type=Path, default=ROOT / "bin/fern-qbe")
    args = parser.parse_args()
    environment = dict(os.environ, FERN_RUNTIME_LIB=str(args.runtime.resolve()), FERN_QBE=str(args.qbe.resolve()))
    environment.pop("LIBRARY_PATH", None)
    sources = ROOT / "compiler-rs/tests/file_text"
    with tempfile.TemporaryDirectory(prefix="fern-file-source-") as temporary:
        directory = Path(temporary)
        binaries = {}
        for source in sorted(sources.glob("*.fn")):
            binary = directory / source.stem
            result = run([args.compiler.resolve(), "build", source, "-o", binary], environment, directory)
            assert result.returncode == 0, (source, result)
            binaries[source.stem] = binary
        cases = [("roundtrip", b"", "6\n1\n🌿é!\n7\n0\n0\n"),
                 ("limits", b"", "16777216\n16777216\n-3\n-3\n16777216\n")]
        cases += [("read_invalid", data, "-3\n") for data in
                  [b"a\0b", b"\xc0\xaf", b"\xed\xa0\x80", b"\xf0\x9f"]]
        for index, (name, data, expected) in enumerate(cases):
            path = directory / f"input-{index}"
            path.write_bytes(data)
            result = run([binaries[name], path], environment, directory)
            assert (result.returncode, result.stdout, result.stderr) == (0, expected, ""), (name, result)
        path = directory / "too-large"
        with path.open("wb") as output:
            output.truncate(16777217)
        result = run([binaries["read_invalid"], path], environment, directory)
        assert (result.returncode, result.stdout, result.stderr) == (0, "-3\n", ""), result
        wrapper = ("import os,resource,signal,sys; "
                   "signal.signal(signal.SIGXFSZ,signal.SIG_IGN); "
                   "resource.setrlimit(resource.RLIMIT_FSIZE,(0,0)); "
                   "os.execv(sys.argv[1],sys.argv[1:])")
        result = run([sys.executable, "-c", wrapper, binaries["write_failure"], directory / "limited"], environment, directory)
        assert (result.returncode, result.stdout, result.stderr) == (0, "-3\n0\n-3\n", ""), result
    print("File text source contracts passed: 8 native cases")

if __name__ == "__main__":
    main()
