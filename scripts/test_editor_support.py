#!/usr/bin/env python3
"""Check reproducible editor artifacts with pinned native/WASM parsers and the Rust syntax oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
from test_rust_numeric import run
ROOT = Path(__file__).resolve().parents[1]
GRAMMAR = ROOT / "editor/tree-sitter-fern"
ZED = ROOT / "editor/zed-fern/languages/fern"
GENERATED = ["grammar.js", "src/parser.c", "src/grammar.json", "src/node-types.json",
             "src/tree_sitter/alloc.h", "src/tree_sitter/array.h", "src/tree_sitter/parser.h"]


def command(argv, directory=ROOT):
    """Run literal arguments with a process-group deadline; surface complete tool errors."""
    result = run(argv, dict(os.environ), directory)
    assert result.returncode == 0, (argv, result.stdout, result.stderr)
    if result.stdout:
        print(result.stdout, end="")
    return result


def generate(tool, directory, update):
    """Generate through official tooling in isolation and compare all published source artifacts."""
    copied = directory / "grammar"
    shutil.copytree(GRAMMAR, copied)
    command([tool, "generate", "--abi", "14"], copied)
    for name in GENERATED:
        generated, published = copied / name, GRAMMAR / name
        if update:
            published.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(generated, published)
        assert generated.read_bytes() == published.read_bytes(), f"stale generated file: {name}"
    return copied


def scanner(directory):
    """Check scanner lifecycle, malformed state and explicit bounds under release and sanitizers."""
    source = GRAMMAR / "test/scanner_bounds.c"
    for flags in [["-O2"], ["-g", "-fsanitize=address,undefined"]]:
        binary = directory / "scanner"
        command([os.environ.get("CC", "clang"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                 *flags, "-I" + str(GRAMMAR / "src"), source, "-o", binary])
        command([binary])


def wasm(tool, runtime, directory, copied, update):
    """Build twice, require byte reproducibility, and execute both exact published module copies."""
    outputs = [directory / "first/tree-sitter-fern.wasm", directory / "second/tree-sitter-fern.wasm"]
    for output in outputs:
        output.parent.mkdir()
        command([tool, "build", "--wasm", "-o", output], copied)
    data = outputs[0].read_bytes()
    assert data == outputs[1].read_bytes(), "WASM builds are not deterministic"
    for published in [GRAMMAR / "tree-sitter-fern.wasm", ZED / "fern.wasm"]:
        if update:
            shutil.copyfile(outputs[0], published)
        assert published.read_bytes() == data, f"stale WASM artifact: {published}"
        command(["node", ROOT / "scripts/test_editor_wasm.cjs", runtime, published])


def queries(tool, directory):
    """Compile every editor query natively against a representative declaration source."""
    source = directory / "queries.fn"
    source.write_text('type Name = String\nnewtype Id = Id(Int)\nfn unwrap(Id(v): Id) -> Int: v\n')
    for name in ["highlights", "outline", "indents", "brackets"]:
        result = command([tool, "query", ZED / (name + ".scm"), source], GRAMMAR)
        assert "capture:" in result.stdout, name + " has no captures"


def web_runtime(path):
    """Verify the portable web runtime bytes against the reviewed official release archive."""
    lock = json.loads((ROOT / "scripts/editor/toolchain.json").read_text())
    runtime = Path(path).resolve()
    assert runtime.name == "web-tree-sitter.cjs", "use the official runtime entrypoint"
    for name, digest in lock["web_runtime_files"].items():
        assert hashlib.sha256((runtime.parent / name).read_bytes()).hexdigest() == digest, name


def main():
    """Provisioning is explicit; missing tools/oracles fail instead of silently skipping a gate."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tree-sitter", default=os.environ.get("TREE_SITTER_CLI", "tree-sitter"))
    parser.add_argument("--web-runtime", default=os.environ.get("TREE_SITTER_WEB_RUNTIME"), required=False)
    parser.add_argument("--rust", default=os.environ.get("FERN_RUST"), required=False)
    parser.add_argument("--update", action="store_true", help="publish official generated sources and WASM")
    args = parser.parse_args()
    assert args.web_runtime and args.rust, "set TREE_SITTER_WEB_RUNTIME and FERN_RUST (or pass flags)"
    web_runtime(args.web_runtime)
    sdk = os.environ.get("TREE_SITTER_WASI_SDK_PATH")
    assert sdk, "set an explicitly provisioned WASI SDK path"
    assert (Path(sdk) / "VERSION").read_text().splitlines()[0] == "29.0", "WASI SDK must be 29.0"
    assert command([args.tree_sitter, "--version"]).stdout.strip() == "tree-sitter 0.26.12"
    command([sys.executable, ROOT / "scripts/generate_editor_support.py", "--check"])
    command([sys.executable, ROOT / "scripts/test_editor_generator.py"])
    with tempfile.TemporaryDirectory(prefix="fern-editor-gate-") as temporary:
        directory = Path(temporary)
        copied = generate(args.tree_sitter, directory, args.update)
        scanner(directory)
        command([args.tree_sitter, "test"], GRAMMAR)
        queries(args.tree_sitter, directory)
        wasm(args.tree_sitter, args.web_runtime, directory, copied, args.update)
        command([args.tree_sitter, "test", "--wasm"], GRAMMAR)
        for mode in [[], ["--wasm"]]:
            command([sys.executable, ROOT / "scripts/test_editor_parity.py", "--tree-sitter",
                     args.tree_sitter, "--rust", args.rust, *mode])
    print("Editor support: reproducible native/query/WASM and Rust syntax gates passed")

if __name__ == "__main__":
    main()
