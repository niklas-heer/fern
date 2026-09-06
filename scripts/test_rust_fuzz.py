#!/usr/bin/env python3
"""Deterministic bounded mutation smoke tests across the Rust frontend boundaries."""
import os
from pathlib import Path
import random
import subprocess
import tempfile
from test_rust_boundaries import CASES as BOUNDARY_CASES

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
    corpus += sorted((ROOT / "compiler-rs/tests/closures").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/maps").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/control").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/iteration").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/with").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/numeric").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/literals").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/numeric_faults").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/sequences").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/clauses").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/parameters").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/generic_schemes").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/whole_signatures").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/inference_shapes").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/aliases").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/newtypes_native").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/json_values/valid").glob("*.fn"))
    corpus += sorted((ROOT / "compiler-rs/tests/unions_native").glob("*.fn"))
    sources = [path.read_text() for path in corpus]
    sources.extend(case[0] for case in BOUNDARY_CASES.values())
    tokens = ["(", ")", "[", "]", ":", "\n", "    ", '"', "🌿", "\\", "?", "None", "if", "#", "|"]
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
