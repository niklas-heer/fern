#!/usr/bin/env python3
"""Test UTF-8 string contracts in isolated extracted functions or the real runtime."""
from pathlib import Path
import argparse
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ERROR = "fern: runtime error: String.slice indices must be UTF-8 character boundaries\n"


def function(source, name):
    """Extract one selected C function, whose braces do not occur inside literals."""
    match = re.search(r"^(?:static )?[\w* ]+\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    if match is None:
        return ""
    depth = 1
    end = match.end()
    while end < len(source) and depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    assert depth == 0, name
    return source[match.start():end] + "\n"


def isolated_source():
    """Use actual function bodies with test-only allocation and argument plumbing."""
    source = (ROOT / "runtime/fern_runtime.c").read_text()
    prefix = """#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FERN_ALLOC malloc
#define FERN_REALLOC realloc
#define FERN_RC_TYPE_STRING_LIST 0
typedef struct { int64_t len; int64_t cap; char** data; } FernStringList;
static void* fern_rc_alloc(size_t n, int type) { (void)type; return calloc(1, n); }
static int test_argc; static char** test_argv;
static int64_t fern_args_count(void) { return test_argc; }
static const char* fern_arg(int64_t i) { return test_argv[i]; }
"""
    names = ["fern_str_slice_is_valid", "fern_str_slice", "fern_utf8_scalar_width",
             "fern_str_split_is_valid", "fern_split_append", "fern_str_split"]
    body = "\n".join(function(source, name) for name in names)
    fixture = (ROOT / "tests/fixtures/utf8_runtime.c").read_text().replace('#include "fern_runtime.h"', '')
    return prefix + body + fixture + "\nint main(int argc,char** argv) { test_argc=argc; test_argv=argv; return fern_main(); }\n"


def cases():
    """Cover byte clamping, scalar boundaries, and scalar-versus-grapheme splitting."""
    tests = []
    for text, start, end, result in [("aé🌿z", 1, 7, "é🌿"), ("é", -10, 99, "é"),
                                    ("é", 99, -4, ""), ("é", 2, 0, ""),
                                    ("", -(1 << 63), (1 << 63) - 1, ""),
                                    ("aé🌿z", 3, 7, "🌿"),
                                    ("é", (1 << 63) - 1, -(1 << 63), ""),
                                    ("é", -(1 << 63), -(1 << 63), "")]:
        tests.append((["slice", text, str(start), str(end)], 0, result.encode().hex() + "\n", ""))
    for start, end in [(0, 1), (1, 2), (1, 1), (1, -5)]:
        tests.append((["slice", "é", str(start), str(end)], 1, "", ERROR))
    for text, separator, result in [("aé🌿é", "", ["a", "é", "🌿", "e", "́"]),
                                    ("", "", []), ("abc", "", ["a", "b", "c"]),
                                    ("é,,🌿,", ",", ["é", "", "🌿", ""]),
                                    ("🌿é🌿", "🌿", ["", "é", ""]), ("", ",", [""])]:
        output = str(len(result)) + "\n" + "".join(part.encode().hex() + "\n" for part in result)
        tests.append((["split", text, separator], 0, output, ""))
    for value in [b"\x80", b"\xc0\xaf", b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xf0\x9f"]:
        tests.append((["split", value, ""], 1, "", "fern: runtime error: String.split requires valid UTF-8 input\n"))
    tests += [(["validate_split", "é🌿", ""], 0, "1\n", ""),
              (["validate_split", b"\xed\xa0\x80", ""], 0, "0\n", ""),
              (["validate_split", b"\xff", ","], 0, "1\n", "")]
    return tests


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--isolated", action="store_true")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="fern-utf8-") as temporary:
        binary = Path(temporary) / "utf8"
        if args.isolated:
            source = Path(temporary) / "utf8.c"
            source.write_text(isolated_source())
            inputs = [str(source)]
        else:
            flags = subprocess.check_output(["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], text=True)
            inputs = ["-Iruntime", "tests/fixtures/utf8_runtime.c", "bin/libfern_runtime.a",
                      *shlex.split(flags), "-pthread"]
        for mode in [[], ["-DNDEBUG", "-O2"]]:
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *mode,
                            *inputs, "-o", str(binary)], cwd=ROOT, check=True, timeout=60)
            for arguments, code, output, error in cases():
                actual = subprocess.run([str(binary), *arguments], capture_output=True, text=True, timeout=10)
                assert (actual.returncode, actual.stdout, actual.stderr) == (code, output, error), (arguments, actual)
    print(f"UTF-8 runtime contracts passed: {len(cases())} cases in debug/release harnesses")


if __name__ == "__main__":
    main()
