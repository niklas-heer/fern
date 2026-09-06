#!/usr/bin/env python3
"""Apple AArch64 must never use its reserved x18/w18 register, including scratch paths."""
import argparse
import hashlib
import os
import platform
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def build(directory):
    """Compile only an isolated QBE target driver, with no shared compiler/runtime artifacts."""
    binary = directory / "qbe-test"
    sources = sorted((ROOT / "deps/qbe").glob("*.c"))
    sources = [s for s in sources if s.name != "main.c"]
    for target in ("arm64", "amd64", "rv64"):
        sources += sorted((ROOT / "deps/qbe" / target).glob("*.c"))
    env = dict(os.environ)
    env.pop("LIBRARY_PATH", None)
    subprocess.run(["clang", "-std=c99", "-D_GNU_SOURCE", "-O2", "-g",
                    "-I" + str(ROOT / "deps/qbe"), *map(str, sources),
                    ROOT / "tests/fixtures/qbe_apple_driver.c", "-lm", "-o", binary],
                   check=True, env=env, timeout=120)
    return binary


def native_result(directory, target, assembly, clobber=False):
    """Use integer-only x18 fault injection, never invalid-pointer/crashing native probes."""
    if platform.machine().lower() not in ("arm64", "aarch64"):
        return
    if platform.system() == "Darwin" and target != "arm64_apple":
        return
    if target == "arm64_apple" and platform.system() != "Darwin":
        assembly = assembly.replace(".subsections_via_symbols", "")
    if clobber:
        lines = []
        for line in assembly.splitlines():
            lines.append(line)
            if re.match(r"\s*(mov|movz|movk)\s+[wx]18,", line):
                lines.append("    mov x18, xzr")
        assembly = "\n".join(lines) + "\n"
    suffix = target + ("-clobber" if clobber else "")
    source, binary = directory / (suffix + ".s"), directory / suffix
    source.write_text(assembly)
    flags = ["-DQBE_APPLE_SYMBOLS"] if target == "arm64_apple" else []
    subprocess.run(["clang", *flags, source, ROOT / "tests/fixtures/qbe_apple_results.c",
                    "-o", binary], check=True, timeout=30)
    actual = subprocess.run([binary], capture_output=True, timeout=10)
    assert actual.returncode == 0, (target, clobber, actual.returncode, actual.stderr)
    print(target + (" deterministic x18-clobber" if clobber else " native") + " results passed", flush=True)


def main():
    """Structural regression remains deterministic without causing a native pointer crash."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--allow-reserved-for-red", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="fern-qbe-apple-") as temporary:
        directory = Path(temporary)
        driver = build(directory)
        source = (ROOT / "tests/fixtures/qbe_apple_registers.ssa").read_bytes()
        for target in ("arm64", "arm64_apple"):
            result = subprocess.run([driver, target], input=source, capture_output=True,
                                    check=True, timeout=20)
            assembly = result.stdout.decode()
            print(target, "assembly SHA256", hashlib.sha256(result.stdout).hexdigest(), flush=True)
            (directory / (target + ".s")).write_text(assembly)
            if target == "arm64_apple":
                forbidden = re.findall(r"^.*\b[wx]18\b.*$", assembly, re.MULTILINE)
                if not args.allow_reserved_for_red:
                    assert not forbidden, "Apple reserved register emitted: " + repr(forbidden)
            native_result(directory, target, assembly)
            if target == "arm64_apple":
                native_result(directory, target, assembly, clobber=True)
        print("Apple reserved-register assembly invariant passed")


if __name__ == "__main__":
    main()
