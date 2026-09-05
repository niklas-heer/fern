#!/usr/bin/env python3
"""Compile and run strings through the real assembler and runtime."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    """Check decoded escapes, Unicode, interpolation, regexes, and long literals."""
    source = r'''fn main():
    println("quote: \" slash: \\ tab:\tend")
    println("line1\nline2")
    println("Grüße 🌿")
    let value = 42
    println("\"{value}\"\\")
    if Regex.is_match("(hello)", "\\(hello\\)"):
        println("regex matched")
'''
    source += '    println("' + 'x' * 4096 + '")\n'
    expected = 'quote: " slash: \\ tab:\tend\nline1\nline2\nGrüße 🌿\n"42"\\\nregex matched\n'
    expected += 'x' * 4096 + '\n'
    with tempfile.TemporaryDirectory(prefix="fern-string-codegen-") as temp:
        path = Path(temp) / "strings.fn"
        binary = Path(temp) / "strings"
        path.write_text(source)
        env = dict(os.environ)
        env.pop("LIBRARY_PATH", None)
        build = subprocess.run([str(ROOT / "bin/fern"), "build", "-o", str(binary), str(path)],
                               cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
        assert build.returncode == 0, build.stdout + build.stderr
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        assert result.stdout == expected, f"String output differs: {result.stdout[:200]!r}"
    print("PASS string codegen: escapes, Unicode, interpolation, regex, 4096-byte literal")


if __name__ == "__main__":
    main()
