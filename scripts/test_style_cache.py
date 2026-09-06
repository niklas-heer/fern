#!/usr/bin/env python3
"""Native style cache tests use isolated authored inputs and real literal-argv C builds."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def snapshot(directory):
    """Copy only authored build inputs; ordinary tests never touch the checkout's artifacts."""
    root = directory / "checkout 'literal 🌿"
    root.mkdir()
    for name in ("src", "lib", "include", "runtime", "deps", "scripts/bootstrap", "compiler-rs/backend"):
        shutil.copytree(ROOT / name, root / name, ignore=shutil.ignore_patterns(".git", "*.o"))
    for name in ("Justfile", "scripts/check_style", "scripts/check_style.fn"):
        (root / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, root / name)
    return root


def execute(root, env, args=("--help",), cwd=None, timeout=180):
    """Run one literal invocation with bounded outer test cleanup, retaining all native bytes."""
    process = subprocess.Popen([root / "scripts/check_style", *args], cwd=cwd or root, env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
    try:
        out, err = process.communicate(timeout=timeout)
    except BaseException:
        os.killpg(process.pid, 9)
        process.communicate()
        raise
    return process.returncode, out, err


def entries(cache):
    """Count complete bundles, never incomplete work directories."""
    return sorted(cache.glob("*/entry.*/ready"))


def main():
    """Pin cold/warm reuse, changed bytes, failed rebuild and literal argument/cwd behavior."""
    with tempfile.TemporaryDirectory(prefix="fern-style-cache-test-") as temporary:
        directory = Path(temporary)
        root = snapshot(directory)
        cache = directory / "cache"
        log = directory / "compiler.log"
        compiler = directory / "literal cc"
        compiler.write_text('#!/bin/bash\nprintf "compile\\n" >> "$FERN_STYLE_TEST_LOG"\nexec /usr/bin/clang "$@"\n')
        compiler.chmod(0o700)
        env = dict(os.environ, FERN_STYLE_CACHE=str(cache), FERN_STYLE_CC=str(compiler),
                   FERN_STYLE_TEST_LOG=str(log))
        env.pop("LIBRARY_PATH", None)
        expected = subprocess.run([ROOT / "bin/check_style", "--help"], capture_output=True, check=True).stdout
        start = time.monotonic()
        result = execute(root, env)
        assert result == (0, expected, b""), result
        cold = time.monotonic() - start
        first_log = log.read_bytes()
        assert not list(cache.glob("*/work.*")), "successful cold staging leaked"
        assert len(entries(cache)) == 1
        first_entry = entries(cache)[0]
        start = time.monotonic()
        assert execute(root, env, cwd=directory) == (0, expected, b"")
        warm = time.monotonic() - start
        assert log.read_bytes() == first_log, "warm cache invoked a compiler"
        source = root / "runtime/fern_runtime.c"
        original = source.read_bytes()
        status = source.stat()
        source.write_bytes(original.replace(b"#include", b"#include", 1) + b"\n/* cache content mutation */\n")
        os.utime(source, ns=(status.st_atime_ns, status.st_mtime_ns))
        assert execute(root, env) == (0, expected, b"")
        assert len(entries(cache)) == 2 and log.read_bytes() != first_log
        source.write_bytes(original + b"\n#error intentional cache rebuild failure\n")
        failed = execute(root, env)
        assert failed[0] == 125 and failed[1] == b"" and b"intentional cache rebuild failure" in failed[2], failed
        assert len(entries(cache)) == 2
        assert not list(cache.glob("*/work.*")), "failed build staging leaked"
        source.write_bytes(original)
        assert execute(root, env) == (0, expected, b"")
        dependency = first_entry / "dependencies"
        dependency.write_bytes(b"\n".join(dependency.read_bytes().splitlines()[1:]) + b"\n")
        before_corrupt = log.read_bytes()
        corrupted = execute(root, env)
        assert corrupted[0] == 125 and corrupted[1] == b"", corrupted
        assert not list(cache.glob("*/work.*")), "corrupt metadata staging leaked"
        assert log.read_bytes() == before_corrupt, "corrupt metadata must not become a cache hit or rebuild"
        print(json.dumps({"cold_seconds": round(cold, 3), "warm_validation_seconds": round(warm, 3),
                          "cases": 6}))


if __name__ == "__main__":
    main()
