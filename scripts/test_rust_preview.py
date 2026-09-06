#!/usr/bin/env python3
"""Move and execute an explicitly supplied preview; never rebuild its native inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

import package_rust_preview as preview


def invoke(arguments, directory, environment, expected=0):
    """Run literal argv with a bounded wait and retain useful diagnostics on failure."""
    result = subprocess.run(arguments, cwd=directory, env=environment, capture_output=True, timeout=30)
    if result.returncode != expected:
        raise ValueError(f"{arguments[1:]} returned {result.returncode}: {result.stderr!r} {result.stdout!r}")
    return result


def component_failures(binary, moved, source, tests, inputs, environment, directory):
    """Prove missing siblings fail closed and explicit component overrides still work."""
    selected = [("fern-qbe", "FERN_QBE"), ("libfern_runtime.a", "FERN_RUNTIME_LIB"),
                ("fern-test-supervisor", "FERN_TEST_SUPERVISOR")]
    for name, variable in selected:
        component = moved / name
        saved = moved / (name + ".removed")
        component.rename(saved)
        try:
            command = [str(binary), "test", str(tests)] if name == "fern-test-supervisor" else [
                str(binary), "build", str(source), "-o", str(directory / "should-not-exist")]
            result = invoke(command, directory, environment, expected=1)
            if b"preview package" not in result.stderr or name.encode() not in result.stderr:
                raise ValueError(f"missing {name} used development fallback: {result.stderr!r}")
            if (directory / "should-not-exist").exists():
                raise ValueError("missing helper published a native executable")
            override = dict(environment, **{variable: str(inputs[name])})
            invoke(command, directory, override)
            (directory / "should-not-exist").unlink(missing_ok=True)
        finally:
            saved.rename(component)


def smoke(inputs, workspace):
    """Stage, archive, verify, relocate, then run commands without developer fallbacks."""
    before = {name: preview.transfer(path, name) for name, path in inputs.items()}
    staging = workspace / "stage"
    system = {"Darwin": "macos", "Linux": "linux"}.get(platform.system())
    arch = {"arm64": "arm64", "aarch64": "arm64", "x86_64": "x86_64"}.get(platform.machine())
    manifest = preview.stage(inputs, staging, "0.1.0-preview.1", system, arch)
    archive = workspace / "preview.tar.gz"
    preview.package(staging, archive)
    moved = workspace / "moved ü $([literal])"
    preview.extract(archive, moved)
    shutil.rmtree(staging)
    environment = dict(os.environ)
    for key in ("FERN_QBE", "FERN_RUNTIME_LIB", "FERN_TEST_SUPERVISOR", "LIBRARY_PATH"):
        environment.pop(key, None)
    forbidden = workspace / "forbidden tools"; forbidden.mkdir()
    for name in ("cargo", "python", "python3", "qbe", "fern", "fern-rs"):
        path = forbidden / name
        path.write_text("#!/bin/sh\necho unexpected developer tool >&2\nexit 91\n")
        path.chmod(0o755)
    environment["PATH"] = str(forbidden) + os.pathsep + environment.get("PATH", "")
    binary = moved / "fern-rs"
    source = workspace / "main ü $literal.fn"
    source.write_text('fn main(): println("relocated preview")\n')
    invoke([str(binary), "check", str(source)], workspace, environment)
    emitted = invoke([str(binary), "emit", str(source)], workspace, environment)
    if b"export function" not in emitted.stdout:
        raise ValueError("emit did not publish QBE")
    executable = workspace / "program ü $literal"
    invoke([str(binary), "build", str(source), "-o", str(executable)], workspace, environment)
    for command in ([str(executable)], [str(binary), "run", str(source)]):
        if invoke(command, workspace, environment).stdout != b"relocated preview\n":
            raise ValueError("relocated native output mismatch")
    tests = workspace / "tests.fn"
    tests.write_text("fn test_unit(): ()\nfn test_result()->Result((),String): Ok(())\n")
    result = invoke([str(binary), "test", str(tests)], workspace, environment)
    if b"2 passed" not in result.stdout:
        raise ValueError(f"native Unit/Result test summary mismatch: {result.stdout!r}")
    docs = workspace / "docs ü.html"
    invoke([str(binary), "doc", str(source), "--html", "-o", str(docs)], workspace, environment)
    if b"<html" not in docs.read_bytes().lower():
        raise ValueError("documentation was not generated")
    component_failures(binary, moved, source, tests, inputs, environment, workspace)
    if preview.validate_stage(moved) != manifest:
        raise ValueError("relocation changed package bytes")
    if before != {name: preview.transfer(path, name) for name, path in inputs.items()}:
        raise ValueError("immutable component inputs changed")
    return {"platform": [system, arch], "files": manifest["files"],
            "archive_sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
            "checks": "check/emit/build/run/Unit+Result tests/docs; three missing helpers and three explicit overrides"}


def main():
    """Accept only explicit existing compiler/backend/runtime inputs for the smoke gate."""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("compiler", "qbe", "supervisor", "runtime"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    try:
        with tempfile.TemporaryDirectory(prefix="fern preview smoke ü ") as temporary:
            workspace = Path(temporary)
            readme = workspace / "PREVIEW.md"; readme.write_text(preview.PREVIEW, encoding="utf-8")
            inputs = dict(zip(preview.EXECUTABLES, (args.compiler.resolve(), args.qbe.resolve(), args.supervisor.resolve())))
            inputs.update({"libfern_runtime.a": args.runtime.resolve(),
                           "LICENSE": Path(__file__).resolve().parents[1] / "LICENSE", "PREVIEW.md": readme})
            print(json.dumps(smoke(inputs, workspace), sort_keys=True, indent=2))
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f"preview smoke: {error}\n")


if __name__ == "__main__":
    main()
