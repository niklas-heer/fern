#!/usr/bin/env python3
"""Bounded native style launcher, dependency grammar and cache lifecycle regressions."""
import os
import shutil
import signal
import fcntl
import resource
import time
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PARSER = None


def parser_case(mode, data):
    """Feed metadata bytes to the authored parser with a finite outer timeout."""
    return subprocess.run([PARSER, mode], input=data,
                          capture_output=True, timeout=5, env=dict(os.environ, LC_ALL="C"))


def parser_cases():
    """Preserve escaped paths without interpreting them as shell input or Make recipes."""
    examples = [
        ("flags", b"-I/a\\ b -L\"/c d\" '-DVALUE=x y' -lssl\n",
         [b"-I/a b", b"-L/c d", b"-DVALUE=x y", b"-lssl"]),
        ("flags", b"'' x\\$HOME `literal`\n", [b"", b"x$HOME", b"`literal`"]),
        ("includes", b'. /a b\n.. /c\\\\d\\"e\n', [b"/a b", b'/c\\d"e']),
        ("deps", b"fern-object: /a\\ b.h \\\n /c\\\\d.h /e\\#f.h /g$$h.h /colon:file.h\n",
         [b"/a b.h", b"/c\\d.h", b"/e#f.h", b"/g$h.h", b"/colon:file.h"]),
    ]
    for mode, data, expected in examples:
        result = parser_case(mode, data)
        assert result.returncode == 0, result.stderr
        assert result.stdout == b"".join(item+b"\0" for item in expected), result.stdout
    for mode, data in [("flags", b"'unclosed"), ("flags", b"ending\\"),
                       ("flags", b"x\0y"), ("deps", b"wrong: /a.h"),
                       ("deps", b"fern-object: /a \\\n"),
                       ("deps", b"fern-object: /a.h # hidden"),
                       ("deps", b"fern-object: /a.h $bad"),
                       ("deps", b"fern-object: /a\rb.h"),
                       ("flags", b"x "*4097), ("flags", b"x"*16385),
                       ("deps", b"fern-object: "+b"/x "*16385)]:
        assert parser_case(mode, data).returncode != 0, (mode, data[:40])
    print("metadata parser literal and rejection cases passed")


def actual_dependencies(directory):
    """Decode real Clang Make escaping for external headers with literal punctuation."""
    folder = Path(directory)/"external space"
    folder.mkdir()
    header = folder/"header #$colon:back\\slash.h"
    header.write_text("#define VALUE 42\n")
    source = folder/"source space.c"
    source.write_text('#include "'+str(header)+'"\nint value(void) { return VALUE; }\n')
    dependency = folder/"object.d"
    compile_result = subprocess.run(["clang", "-H", "-MD", "-MF", dependency, "-MT", "fern-object", "-c", source,
                    "-o", folder/"object.o"], check=True, capture_output=True)
    result = parser_case("deps", dependency.read_bytes())
    assert result.returncode == 0, (dependency.read_bytes(), result.stderr)
    assert result.stdout.split(b"\0")[0] == os.fsencode(source), result.stdout
    includes = parser_case("includes", compile_result.stderr)
    assert includes.returncode == 0, (compile_result.stderr, includes.stderr)
    assert includes.stdout == os.fsencode(header)+b"\0", includes.stdout


def foreground_cases(directory):
    """Final native execution preserves statuses/stdio and removes only its owned run files."""
    directory = Path(directory)
    supervisor = directory/"supervisor"
    helper = directory/"child"
    for output, source in [(supervisor, "scripts/bootstrap/style_supervisor.c"),
                           (helper, "tests/fixtures/style_supervisor_child.c")]:
        subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", ROOT/source,
                        "-o", output], check=True)
    for status in (0, 7, 127):
        run = directory/("run."+str(status))
        run.mkdir(mode=0o700)
        (run/"owner").write_text("FERN_STYLE_RUN_V1\n")
        shutil.copy2(helper, run/"program")
        result = subprocess.run([supervisor, "--run", run, "--", run/"program", "exit", str(status)],
                                capture_output=True, timeout=5)
        assert (result.returncode, result.stdout, result.stderr) == (status, b"", b""), result
        assert not run.exists()


def descriptor_boundary(directory):
    """Bash3.2 closes high inherited fds without changing the caller or 0/1/2."""
    descriptor = os.open(Path(directory)/"inherited", os.O_CREAT | os.O_RDWR, 0o600)
    high = fcntl.fcntl(descriptor, fcntl.F_DUPFD, 900)
    try:
        def lower_soft_limit():
            _, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
            resource.setrlimit(resource.RLIMIT_NOFILE, (256, hard))
        script = 'source "$1"; shift; style_clean_exec "$@"'
        probe = 'if [[ -e /dev/fd/900 ]]; then exit 99; fi; printf "%s\\n" "$1"'
        result = subprocess.run(["/bin/bash", "-c", script, "fd-boundary",
                                 ROOT/"scripts/bootstrap/style_common.sh", "/bin/bash", "-c", probe,
                                 "literal", "space é"], input=b"", capture_output=True, timeout=5,
                                pass_fds=(high,), preexec_fn=lower_soft_limit)
        assert (result.returncode, result.stdout, result.stderr) == (0, "space é\n".encode(), b""), result
        assert fcntl.fcntl(high, fcntl.F_GETFD) >= 0
    finally:
        os.close(high)
        os.close(descriptor)


def launcher_red(directory):
    """A cold user command must reach the native checker without Python or build chatter."""
    cache = Path(directory)/"cache"
    expected = subprocess.run([ROOT/"bin/check_style", "--help"], capture_output=True, check=True).stdout
    actual = subprocess.run([ROOT/"scripts/check_style", "--help"], cwd=directory,
                            env=dict(os.environ, FERN_STYLE_CACHE=str(cache)),
                            capture_output=True, timeout=180)
    assert (actual.returncode, actual.stdout, actual.stderr) == (0, expected, b""), actual


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="fern-style-metadata-") as directory:
        PARSER = str(Path(directory)/"metadata")
        subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        ROOT/"scripts/bootstrap/style_metadata.c", "-o", PARSER], check=True)
        parser_cases()
        actual_dependencies(directory)
        foreground_cases(directory)
        descriptor_boundary(directory)
        launcher_red(directory)
