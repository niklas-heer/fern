#!/usr/bin/env python3
"""Opt-in pinned Zed package gate: locked builds, exact artifacts, hostile inputs and native/WASM parity."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import tempfile

import package_zed as package

ROOT = Path(__file__).resolve().parents[1]


def must_fail(argv, directory, label):
    """Require an explicit tool rejection, never a skipped or unavailable check."""
    try:
        package.command(argv, directory)
    except ValueError as error:
        if "command failed" in str(error):
            return
        raise
    raise AssertionError(label + " was unexpectedly accepted")


def malformed_package(args, directory, built):
    """Validate complete component bytes and staged queries, rejecting corruption before installation."""
    invalid = directory / "invalid-component.wasm"
    invalid.write_bytes((built / "extension/extension.wasm").read_bytes() + b"\0\x7f")
    must_fail([args.wasm_tools, "validate", invalid], directory, "truncated component")
    bad = directory / "bad-query"
    shutil.copytree(built / "extension", bad)
    (bad / "languages/fern/highlights.scm").write_text("(nonexistent_node) @keyword\n")
    must_fail([args.node, ROOT / "scripts/test_zed_wasm.cjs", args.web_runtime, bad],
              directory, "invalid staged query")
    metadata = json.loads((built / "build.json").read_text())
    grammar = (built / "extension/grammars/fern.wasm").read_bytes()
    expected = package.command(["git", "show", metadata["grammar"]["rev"] +
                               ":editor/tree-sitter-fern/tree-sitter-fern.wasm"], args.grammar_repository)
    assert grammar == expected, "staged grammar must retain exact pinned artifact identity"


def source_gates(args):
    """Exercise actual host adapter tests and the source-identity-aware native/WASM editor corpus."""
    source = ROOT / "editor/zed-fern"
    for command in [[args.cargo, "test", "--locked", "--offline"], [args.cargo, "fmt", "--check"],
                    [args.cargo, "clippy", "--locked", "--offline", "--all-targets", "--", "-D", "warnings"]]:
        print(package.command(command, source).decode(), end="", flush=True)
    print(package.command([sys.executable, ROOT / "scripts/test_zed_package.py"], ROOT).decode(), end="")
    for mode in [[], ["--wasm"]]:
        print(package.command([sys.executable, ROOT / "scripts/test_editor_parity.py", "--tree-sitter",
                               args.tree_sitter, "--rust", args.rust, *mode], ROOT).decode(), end="", flush=True)


def main():
    """Build twice in fresh directories, verify reproducibility, and leave GUI smoke opt-in separately."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--grammar-repository", type=Path, required=True)
    parser.add_argument("--cargo", default="cargo")
    parser.add_argument("--wasi-sdk", type=Path, required=True)
    parser.add_argument("--wasm-tools", required=True)
    parser.add_argument("--tree-sitter", required=True)
    parser.add_argument("--web-runtime", type=Path, required=True)
    parser.add_argument("--node", default="node")
    parser.add_argument("--rust", type=Path, required=True)
    args = parser.parse_args()
    args.source = ROOT / "editor/zed-fern"
    source_gates(args)
    with tempfile.TemporaryDirectory(prefix="fern zed package gate ") as temporary:
        directory = Path(temporary)
        results = []
        for name in ["first", "second"]:
            args.output = directory / name
            results.append(package.package(args))
            print("Built and validated package " + name, flush=True)
        assert (results[0] / "archive.tar.gz").read_bytes() == (results[1] / "archive.tar.gz").read_bytes()
        assert (results[0] / "build.json").read_bytes() == (results[1] / "build.json").read_bytes()
        malformed_package(args, directory, results[0])
        print(package.command([args.node, ROOT / "scripts/test_editor_wasm.cjs", args.web_runtime,
                               results[0] / "extension/grammars/fern.wasm"], ROOT).decode(), end="")
    print("Zed package: reproducible locked builds, native/WASM source parity and hostile inputs passed")


if __name__ == "__main__":
    main()
