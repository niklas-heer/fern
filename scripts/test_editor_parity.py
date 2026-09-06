#!/usr/bin/env python3
"""Exercise native/WASM editor parsing with bounded sources and deterministic edits."""
import argparse
import json
import os
import re
import xml.etree.ElementTree as ET
from pathlib import Path
import tempfile
from test_rust_numeric import run
ROOT = Path(__file__).resolve().parents[1]
GRAMMAR = ROOT / "editor/tree-sitter-fern"
CASES = GRAMMAR / "test/parity/cases.json"


def command(argv, success=True):
    """Run one literal tool command with a bounded lifetime and diagnostic capture."""
    result = run(argv, dict(os.environ), GRAMMAR)
    if success:
        assert result.returncode == 0, (argv, result.stdout, result.stderr)
    return result


def parse(tool, path, wasm, edit=None, xml=False):
    """Return the complete range-free tree after an optional byte-indexed edit."""
    args = [tool, "parse", "--no-ranges", "--timeout", "1000000"]
    if xml:
        args.append("--xml")
    if wasm:
        args.append("--wasm")
    if edit is not None:
        args += ["--edits", edit, "--"]
    return command(args + [str(path)], False)


def structured_tree(tool, path, wasm):
    """Read native byte-column ranges and declaration fields independently of tree display formatting."""
    result = parse(tool, path, wasm, xml=True)
    xml = result.stdout.split("</sources>")[0] + "</sources>"
    return ET.fromstring(xml)


def source_ranges(tool, path, source, wasm):
    """Require exact top-level identities and recover each source name from its UTF-8 byte range."""
    tree = structured_tree(tool, path, wasm)
    expected = re.findall(r"(?m)^(?:pub )?(?:fn|type|newtype) ([A-Za-z_][A-Za-z0-9_]*)", source)
    declarations = tree.findall("./source/source_file/*")
    names = [node.find("*[@field='name']") for node in declarations]
    names = [node for node in names if node is not None]
    assert [node.text for node in names] == expected, (path.name, expected, [n.text for n in names])
    lines = source.splitlines(keepends=True)
    for node in names:
        row, start, end = [int(node.attrib[k]) for k in ("srow", "scol", "ecol")]
        assert int(node.attrib["erow"]) == row
        assert lines[row].encode()[start:end].decode() == node.text, (path.name, node.attrib)


def valid_cases(tool, directory, cases, wasm):
    """Require every accepted fixture to have its independently specified structural nodes."""
    for case in cases:
        path = directory / (case["name"] + ".fn")
        path.write_text(case["source"], newline="")
        result = parse(tool, path, wasm)
        assert result.returncode == 0, (case["name"], result.stdout, result.stderr)
        assert "ERROR" not in result.stdout and "MISSING" not in result.stdout, case["name"]
        source_ranges(tool, path, case["source"], wasm)
        for node in case["nodes"]:
            assert "(" + node in result.stdout, (case["name"], node, result.stdout)


def invalid_cases(tool, directory, cases, wasm):
    """Malformed fixtures must remain erroneous and recover the following declaration."""
    for case in cases:
        path = directory / (case["name"] + ".fn")
        path.write_text(case["source"])
        result = parse(tool, path, wasm)
        assert result.returncode != 0, (case["name"], result.stdout)
        assert "ERROR" in result.stdout or "MISSING" in result.stdout, case["name"]
        tree = structured_tree(tool, path, wasm)
        recovered = tree.findall(".//function_definition/*[@field='name']")
        assert any(node.text == "after" for node in recovered), (case["name"], result.stdout)


def edited_cases(tool, directory, cases, wasm):
    """Compare fresh and incremental trees after eight fixed source edits, including UTF-8."""
    for case in cases:
        source, old, new = case["source"], case["old"], case["new"]
        at = source.index(old)
        position = len(source[:at].encode())
        edit = f"{position} {len(old.encode())} {new}"
        path = directory / (case["name"] + ".fn")
        path.write_text(source)
        incremental = parse(tool, path, wasm, edit)
        path.write_text(source[:at] + new + source[at + len(old):])
        fresh = parse(tool, path, wasm)
        assert fresh.returncode == incremental.returncode == 0, (case["name"], fresh, incremental)
        assert fresh.stdout == incremental.stdout, (case["name"], fresh.stdout, incremental.stdout)


def main():
    """Run the fixed native or WASM corpus without changing checked-in source artifacts."""
    options = argparse.ArgumentParser()
    options.add_argument("--tree-sitter", default=os.environ.get("TREE_SITTER_CLI", "tree-sitter"))
    options.add_argument("--wasm", action="store_true")
    options.add_argument("--rust", default=os.environ.get("FERN_RUST"))
    args = options.parse_args()
    assert command([args.tree_sitter, "--version"]).stdout.strip() == "tree-sitter 0.26.12"
    cases = json.loads(CASES.read_text())
    assert [len(cases[k]) for k in ["valid", "invalid", "edits"]] == [24, 8, 8]
    assert all(len(case["source"].encode()) <= 1024 * 1024 for group in cases.values() for case in group)
    with tempfile.TemporaryDirectory(prefix="fern-editor-parity-") as temporary:
        directory = Path(temporary)
        if args.rust:
            for case in cases["valid"]:
                path = directory / "rust_oracle.fn"
                path.write_text(case["source"] + "\nfn main(): ()\n")
                command([args.rust, "check", str(path)])
        valid_cases(args.tree_sitter, directory, cases["valid"], args.wasm)
        invalid_cases(args.tree_sitter, directory, cases["invalid"], args.wasm)
        edited_cases(args.tree_sitter, directory, cases["edits"], args.wasm)
    print(f"Editor {'WASM' if args.wasm else 'native'}: 24 valid, 8 recovery, 8 incremental cases")

if __name__ == "__main__":
    main()
