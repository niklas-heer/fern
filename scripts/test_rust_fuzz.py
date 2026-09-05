#!/usr/bin/env python3
"""Deterministic bounded mutation smoke tests across the Rust frontend boundaries."""
import os
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SEED = 0xFE12A
LIMIT = 192


def run(compiler, action, path):
    result = subprocess.run([compiler, action, path], capture_output=True, timeout=5)
    if result.returncode not in (0, 1) or b"panicked" in result.stderr:
        raise AssertionError(f"{action} failed abnormally: {path.read_text()!r}\n{result.stderr!r}")
    return result


def main():
    compiler = ROOT / "compiler-rs/target/debug/fern-rs"
    corpus = sorted((ROOT / "compiler-rs/tests/collections").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/types").glob("*.fn"))
    sources = [path.read_text() for path in corpus]
    tokens = ["(", ")", "[", "]", ":", "\n", "    ", '"', "🌿", "\\", "?", "None", "if", "#"]
    rng = random.Random(SEED)
    accepted = 0
    with tempfile.TemporaryDirectory(prefix="fern-rust-fuzz-") as temporary:
        path = Path(temporary) / "case.fn"
        for index in range(LIMIT):
            source = rng.choice(sources)
            for _ in range(rng.randrange(1, 5)):
                start = rng.randrange(len(source) + 1)
                end = min(len(source), start + rng.randrange(5))
                source = source[:start] + rng.choice(tokens) + source[end:]
            path.write_text(source)
            checked = run(compiler, "check", path)
            emitted = run(compiler, "emit", path)
            if checked.returncode == 0:
                accepted += 1
                if emitted.returncode:
                    raise AssertionError(f"checked program cannot lower (seed {SEED}, case {index}): {source!r}\n{emitted.stderr!r}")
            formatted = run(compiler, "fmt", path)
            if formatted.returncode:
                if path.read_text() != source:
                    raise AssertionError("failed formatter modified source")
            else:
                canonical = path.read_bytes()
                if run(compiler, "fmt", path).returncode or path.read_bytes() != canonical:
                    raise AssertionError("formatter is not idempotent")
                again = run(compiler, "emit", path)
                if again.returncode != emitted.returncode or (not again.returncode and again.stdout != emitted.stdout):
                    raise AssertionError("formatting changed lowering behavior")
    print(f"Rust fuzz smoke passed: {LIMIT} mutations, {accepted} accepted, seed {SEED:#x}")


if __name__ == "__main__":
    main()
