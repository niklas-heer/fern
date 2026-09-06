#!/usr/bin/env python3
"""A source edit during a cold build permits exactly one fresh snapshot retry."""
import os
from pathlib import Path
import subprocess
import tempfile
from test_style_cache import ROOT, execute, snapshot


def main():
    """Use a literal compiler wrapper that mutates one declared input once or on every attempt."""
    with tempfile.TemporaryDirectory(prefix="fern-style-cache-retry-") as temporary:
        directory = Path(temporary)
        root = snapshot(directory)
        compiler = directory / "cc"
        compiler.write_text('''#!/bin/bash
for arg in "$@"; do
    if [[ $arg == src/main.c && ( ! -f "$FERN_STYLE_TEST_MARKER" || $FERN_STYLE_TEST_ALWAYS == 1 ) ]]; then
        printf '\\n/* changing input */\\n' >> "$FERN_STYLE_TEST_SOURCE"
        printf 'changed\\n' > "$FERN_STYLE_TEST_MARKER"
    fi
done
exec /usr/bin/clang "$@"
''')
        compiler.chmod(0o700)
        env = dict(os.environ, FERN_STYLE_CACHE=str(directory / "cache"), FERN_STYLE_CC=str(compiler),
                   FERN_STYLE_TEST_MARKER=str(directory / "changed"),
                   FERN_STYLE_TEST_SOURCE=str(root / "runtime/fern_runtime.c"), FERN_STYLE_TEST_ALWAYS="0")
        env.pop("LIBRARY_PATH", None)
        expected = subprocess.run([ROOT / "bin/check_style", "--help"], capture_output=True, check=True).stdout
        result = execute(root, env)
        assert result == (0, expected, b""), result
        env["FERN_STYLE_CACHE"] = str(directory / "changing-cache")
        env["FERN_STYLE_TEST_ALWAYS"] = "1"
        result = execute(root, env)
        assert result[0] == 125 and result[1] == b"" and b"inputs changed" in result[2], result
        print("two source-stability retry cases passed")


if __name__ == "__main__":
    main()
