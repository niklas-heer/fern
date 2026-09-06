#!/usr/bin/env python3
"""Build a verified local Zed package without modifying any editor profile (Python 3.11+)."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import subprocess
import tarfile
import tempfile
import time
import tomllib

ROOT = Path(__file__).resolve().parents[1]
RUST_VERSION = "1.97.1"
TARGET = "wasm32-wasip2"
WASM_TOOLS_VERSION = "1.258.0"
SOURCE_FILES = ["src/parser.c", "src/scanner.c", "src/tree_sitter/parser.h",
                "src/tree_sitter/alloc.h", "src/tree_sitter/array.h",
                "grammar.js", "src/grammar.json", "src/node-types.json", "tree-sitter-fern.wasm"]
QUERIES = ["highlights.scm", "outline.scm", "indents.scm", "brackets.scm"]


def command(argv, directory, timeout=300, limit=32 * 1024 * 1024, environment=None):
    """Capture literal arguments under aggregate output/deadline limits, reaping owned children."""
    process = subprocess.Popen(list(map(str, argv)), cwd=directory, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=True)
    selector = selectors.DefaultSelector()
    streams = [bytearray(), bytearray()]
    deadline = time.monotonic() + timeout
    reaped = False
    try:
        for index, stream in enumerate([process.stdout, process.stderr]):
            selector.register(stream, selectors.EVENT_READ, index)
        size = 0
        while selector.get_map():
            if time.monotonic() >= deadline:
                raise ValueError(f"command deadline exceeded: {argv[0]}")
            for key, _ in selector.select(min(0.05, max(0, deadline - time.monotonic()))):
                data = os.read(key.fd, min(65536, max(1, limit + 1 - size)))
                if not data:
                    selector.unregister(key.fileobj)
                    continue
                size += len(data)
                if size > limit:
                    raise ValueError(f"command output limit exceeded: {argv[0]}")
                streams[key.data].extend(data)
        process.wait(timeout=max(0.001, deadline - time.monotonic()))
        reaped = True
        if process.returncode:
            detail = bytes(streams[1]).decode("utf-8", errors="replace")
            raise ValueError(f"command failed ({process.returncode}): {argv[0]}\n{detail}")
        return bytes(streams[0])
    finally:
        selector.close()
        if not reaped:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait(timeout=5)
        process.stdout.close()
        process.stderr.close()


def revision(value):
    """Require a full immutable SHA-1 commit identity, never a ref or option."""
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{40}", value):
        raise ValueError("grammar rev must be a complete 40-character commit")
    return value


def output_path(path):
    """Refuse existing outputs, including dangling symlinks, without deleting user data."""
    path = Path(path).absolute()
    if path.exists() or path.is_symlink():
        raise ValueError(f"output already exists; choose a fresh directory: {path}")
    if not path.parent.is_dir():
        raise ValueError("output parent must already exist")
    return path


def configuration(source):
    """Check the exact scalar registration, monorepo path and selected language-server identity."""
    manifest = tomllib.loads((source / "extension.toml").read_text())
    language = tomllib.loads((source / "languages/fern/config.toml").read_text())
    if language.get("grammar") != "fern" or language.get("name") != "Fern":
        raise ValueError("Fern language must register grammar = \"fern\"")
    grammar = manifest.get("grammars", {}).get("fern", {})
    revision(grammar.get("rev"))
    if grammar.get("path") != "editor/tree-sitter-fern":
        raise ValueError("grammar path must select editor/tree-sitter-fern")
    if grammar.get("repository") != "https://github.com/niklas-heer/fern":
        raise ValueError("grammar repository must identify the Fern source repository")
    if manifest.get("language_servers", {}).get("fern-lsp", {}).get("languages") != ["Fern"]:
        raise ValueError("fern-lsp must register the Fern language")
    return manifest, grammar


def leb(data, offset):
    """Decode bounded WebAssembly u32 metadata lengths; full validation uses wasm-tools."""
    result = 0
    for index in range(5):
        if offset >= len(data):
            raise ValueError("truncated WebAssembly section length")
        value = data[offset]
        offset += 1
        if index == 4 and value > 15:
            raise ValueError("invalid WebAssembly section length")
        result |= (value & 127) << (7 * index)
        if value < 128:
            return result, offset
    raise ValueError("invalid WebAssembly section length")


def custom_sections(data):
    """Inspect bounded nested metadata after full validation, including component core modules."""
    pending = [(memoryview(data), 0)]
    sections = 0
    while pending:
        binary, depth = pending.pop()
        if depth > 64:
            raise ValueError("WebAssembly metadata nesting limit")
        component = binary[:8] == b"\0asm\x0d\0\x01\0"
        if not component and binary[:8] != b"\0asm\x01\0\0\0":
            raise ValueError("invalid nested WebAssembly header")
        offset = 8
        while offset < len(binary):
            sections += 1
            if sections > 100000:
                raise ValueError("WebAssembly metadata section limit")
            kind = binary[offset]
            size, begin = leb(binary, offset + 1)
            end = begin + size
            if end > len(binary):
                raise ValueError("truncated WebAssembly section")
            if kind == 0:
                length, payload = leb(binary, begin)
                if payload + length > end:
                    raise ValueError("truncated WebAssembly custom name")
                yield binary[payload:payload + length], binary[payload + length:end]
            elif component and kind in [1, 4]:
                if len(pending) >= 1024:
                    raise ValueError("WebAssembly pending module limit")
                pending.append((binary[begin:end], depth + 1))
            offset = end


def api_marker(data):
    """Require a WASI Preview2 component and one exact published Zed API version marker."""
    if len(data) > 32 * 1024 * 1024 or data[:8] != b"\0asm\x0d\0\x01\0":
        raise ValueError("extension must be a bounded WebAssembly component")
    versions = [payload for name, payload in custom_sections(data) if name == b"zed:api-version"]
    if versions != [b"\0\0\0\x07\0\0"]:
        raise ValueError("extension must declare exactly Zed API 0.7.0")
    return "0.7.0"


def grammar_source(repository, grammar, directory):
    """Read exact immutable compiler inputs from a user-supplied local repository mirror."""
    commit = grammar["rev"]
    actual = command(["git", "rev-parse", "--verify", commit + "^{commit}"], repository).decode().strip()
    if actual != commit:
        raise ValueError("grammar identity is not an exact commit")
    files = {}
    for name in SOURCE_FILES:
        data = command(["git", "show", commit + ":" + grammar["path"] + "/" + name], repository)
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        files[name] = hashlib.sha256(data).hexdigest()
    return files


def build_grammar(args, source, stage):
    """Build a portable parser through the project's pinned Tree-sitter/SDK toolchain."""
    sdk = args.wasi_sdk.resolve()
    if (sdk / "VERSION").read_text().splitlines()[0] != "29.0":
        raise ValueError("WASI SDK 29.0 is required for the portable grammar")
    version = command([args.tree_sitter, "--version"], source).decode().strip()
    if version != "tree-sitter 0.26.12":
        raise ValueError("Tree-sitter 0.26.12 is required")
    output = stage / "grammars/tree-sitter-fern.wasm"
    output.parent.mkdir()
    environment = dict(os.environ, TREE_SITTER_WASI_SDK_PATH=str(sdk),
                       XDG_CACHE_HOME=str(source / "fresh-cache"))
    command([args.tree_sitter, "build", "--wasm", "-o", output], source, environment=environment)
    command([args.wasm_tools, "validate", output], source)
    if output.read_bytes() != (source / "tree-sitter-fern.wasm").read_bytes():
        raise ValueError("rebuilt grammar does not match the pinned source artifact")
    packaged = output.with_name("fern.wasm")
    output.rename(packaged)
    return packaged


def build_extension(args, source, scratch, stage):
    """Build locked dependencies using the separate pinned component toolchain, then validate all bytes."""
    version = command([args.cargo, "--version"], source).decode().split()
    if version[:2] != ["cargo", RUST_VERSION]:
        raise ValueError(f"extension build requires Cargo {RUST_VERSION}; compiler MSRV stays separate")
    environment = dict(os.environ, CARGO_TARGET_DIR=str(scratch / "target"))
    command([args.cargo, "build", "--locked", "--offline", "--release", "--target", TARGET],
            source, environment=environment)
    component = scratch / "target" / TARGET / "release/zed_fern.wasm"
    command([args.wasm_tools, "validate", component], source)
    api_marker(component.read_bytes())
    output = stage / "extension.wasm"
    command([args.wasm_tools, "strip", "--delete", r"^\.debug.*", component, "-o", output], source)
    command([args.wasm_tools, "validate", output], source)
    api_marker(output.read_bytes())


def copy_resources(source, stage):
    """Copy only runtime language resources; exclude source caches and the historical test WASM copy."""
    language = stage / "languages/fern"
    language.mkdir(parents=True)
    for name in ["config.toml", *QUERIES]:
        shutil.copyfile(source / "languages/fern" / name, language / name)
    shutil.copyfile(source / "LICENSE", stage / "LICENSE")
    text = (source / "extension.toml").read_text()
    text += '\n[lib]\nkind = "Rust"\nversion = "0.7.0"\n'
    (stage / "extension.toml").write_text(text)


def verify_web_runtime(runtime):
    """Require the reviewed pinned runtime bytes before executing JavaScript dependency code."""
    lock = json.loads((ROOT / "scripts/editor/toolchain.json").read_text())
    runtime = runtime.resolve()
    if runtime.name != "web-tree-sitter.cjs":
        raise ValueError("use the official web-tree-sitter.cjs entry point")
    for name, digest in lock["web_runtime_files"].items():
        if hashlib.sha256((runtime.parent / name).read_bytes()).hexdigest() != digest:
            raise ValueError("web runtime checksum mismatch: " + name)


def write_archive(stage, output):
    """Use stable tar metadata and gzip time so identical package bytes produce identical archives."""
    with output.open("wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, filename="") as zipped:
        with tarfile.open(fileobj=zipped, mode="w") as archive:
            for path in sorted(stage.rglob("*")):
                info = archive.gettarinfo(str(path), arcname=str(path.relative_to(stage)))
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ""
                info.mode = 0o755 if path.is_dir() else 0o644
                if path.is_file():
                    with path.open("rb") as contents:
                        archive.addfile(info, contents)
                else:
                    archive.addfile(info)


def package(args):
    """Publish a new complete stage only after component, grammar and resource validation succeeds."""
    output = output_path(args.output)
    source = args.source.resolve()
    manifest, grammar = configuration(source)
    version = command([args.wasm_tools, "--version"], source).decode().strip()
    if version.split()[:2] != ["wasm-tools", WASM_TOOLS_VERSION]:
        raise ValueError("wasm-tools " + WASM_TOOLS_VERSION + " is required")
    with tempfile.TemporaryDirectory(prefix=".fern-zed-build-", dir=output.parent) as temporary:
        scratch = Path(temporary)
        stage = scratch / "result/extension"
        stage.mkdir(parents=True)
        inputs = grammar_source(args.grammar_repository.resolve(), grammar, scratch / "grammar")
        build_grammar(args, scratch / "grammar", stage)
        build_extension(args, source, scratch, stage)
        copy_resources(source, stage)
        verify_web_runtime(args.web_runtime)
        command([args.node, ROOT / "scripts/test_zed_wasm.cjs", args.web_runtime, stage], source)
        files = {str(p.relative_to(stage)): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in sorted(stage.rglob("*")) if p.is_file()}
        metadata = {"grammar": grammar, "grammar_inputs": inputs, "files": files,
                    "rust": RUST_VERSION, "target": TARGET, "wasm_tools": WASM_TOOLS_VERSION,
                    "wasi_sdk": "29.0", "tree_sitter": "0.26.12", "api": "0.7.0", "official_packager": False}
        (scratch / "result/build.json").write_text(json.dumps(metadata, indent=2) + "\n")
        write_archive(stage, scratch / "result/archive.tar.gz")
        if output.exists() or output.is_symlink():
            raise ValueError("output appeared during build; refusing replacement")
        (scratch / "result").rename(output)
    return output


def main():
    """Require provisioned tools and a local source mirror; perform no downloads or installations."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "editor/zed-fern")
    parser.add_argument("--output", type=Path, required=True, help="new output directory; existing paths refused")
    parser.add_argument("--grammar-repository", type=Path, required=True, help="local mirror containing pinned commit")
    parser.add_argument("--cargo", default="cargo")
    parser.add_argument("--wasi-sdk", type=Path, required=True)
    parser.add_argument("--wasm-tools", required=True)
    parser.add_argument("--tree-sitter", required=True)
    parser.add_argument("--web-runtime", type=Path, required=True)
    parser.add_argument("--node", default="node")
    args = parser.parse_args()
    try:
        print(package(args))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    main()
