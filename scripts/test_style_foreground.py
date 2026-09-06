#!/usr/bin/env python3
"""Final-checker supervision preserves stdio/status and deletes only held invocation files."""
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def invocation(directory, helper, label):
    """Create the exact private ownership protocol, retaining a hardlink to the selected inode."""
    run = directory/("run."+label)
    run.mkdir(mode=0o700)
    (run/"owner").write_text("FERN_STYLE_RUN_V1\n")
    os.link(helper, run/"program")
    return run


def command(supervisor, run, *args):
    """Preserve literal arguments rather than joining them into shell text."""
    return list(map(str, [supervisor, "--run", run, "--", run/"program", *args]))


def ready(path):
    """Use bounded explicit readiness before mutation or interruption."""
    for _ in range(200):
        if path.exists() and path.read_bytes() == b"ready\n":
            return
        time.sleep(0.01)
    raise AssertionError("foreground fixture did not become ready")


def ordinary(supervisor, helper, directory):
    """Normal exits, direct signals and all closed-stdio combinations keep their exact status."""
    for status in (0, 7, 127):
        run = invocation(directory, helper, str(status))
        result = subprocess.run(command(supervisor, run, "exit", status),
                                capture_output=True, timeout=5)
        assert (result.returncode, result.stdout, result.stderr) == (status, b"", b""), result
        assert not run.exists()
    run = invocation(directory, helper, "stdin")
    result = subprocess.run(command(supervisor, run, "stdin"), input="literal\0🌿".encode(),
                            capture_output=True, timeout=5)
    assert (result.returncode, result.stdout, result.stderr) == (7, "literal\0🌿".encode(), b"")
    run = invocation(directory, helper, "streams")
    result = subprocess.run(command(supervisor, run, "streams"), capture_output=True, timeout=5)
    assert (result.returncode, result.stdout, result.stderr) == (7, "out\0🌿".encode(), b"err\n")
    run = invocation(directory, helper, "signal")
    result = subprocess.run(command(supervisor, run, "die"), capture_output=True, timeout=5)
    assert (result.returncode, result.stdout, result.stderr) == (143, b"", b"")
    for mask in range(8):
        run = invocation(directory, helper, "fds"+str(mask))
        def close_stdio():
            for fd in range(3):
                if mask & (1 << fd):
                    os.close(fd)
        result = subprocess.run(command(supervisor, run, "fds"), input=b"", capture_output=True,
                                timeout=5, preexec_fn=close_stdio)
        assert result.returncode == mask, (mask, result)
        assert not run.exists()


def waiting(supervisor, helper, directory, label, status):
    """Start a finite ready child for controlled cleanup and pruning races."""
    run = invocation(directory, helper, label)
    marker, release = directory/(label+".ready"), directory/(label+".release")
    process = subprocess.Popen(command(supervisor, run, "wait_exit", marker, release, status),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
    ready(marker)
    return run, release, process


def controlled(supervisor, helper, directory):
    """Interruption and pruning cannot lose retained identity or the selected program inode."""
    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        run, _, process = waiting(supervisor, helper, directory, "interrupt"+str(sig), 0)
        process.send_signal(sig)
        out, err = process.communicate(timeout=5)
        assert process.returncode == 128+sig and not out and b"interrupted" in err
        assert not run.exists()
    selected = directory/"selected"
    os.link(helper, selected)
    run, release, process = waiting(supervisor, selected, directory, "pruning", 7)
    selected.unlink()
    release.touch()
    out, err = process.communicate(timeout=5)
    assert (process.returncode, out, err) == (7, b"", b"")
    assert not run.exists()


def cleanup_cases(supervisor, helper, directory):
    """Renamed paths and extra files are not deleted; first native errors survive cleanup failure."""
    for status in (0, 7):
        run, release, process = waiting(supervisor, helper, directory, "renamed"+str(status), status)
        moved = directory/("moved"+str(status))
        run.rename(moved)
        run.mkdir(mode=0o700)
        (run/"sentinel").write_text("keep")
        release.touch()
        out, err = process.communicate(timeout=5)
        assert process.returncode == (125 if status == 0 else status) and not out and b"IO failed" in err
        assert (run/"sentinel").read_text() == "keep"
        assert list(moved.iterdir()) == []
    run, release, process = waiting(supervisor, helper, directory, "extra", 7)
    (run/"extra").write_text("keep")
    release.touch()
    out, err = process.communicate(timeout=5)
    assert process.returncode == 7 and not out and b"IO failed" in err
    assert (run/"extra").read_text() == "keep"
    assert not (run/"program").exists() and not (run/"owner").exists()


def main():
    """Run each final-mode group in isolated debug, release and sanitizer builds."""
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
    env.pop("LIBRARY_PATH", None)
    os.environ.update(ASAN_OPTIONS=env["ASAN_OPTIONS"], UBSAN_OPTIONS=env["UBSAN_OPTIONS"])
    with tempfile.TemporaryDirectory(prefix="fern-style-foreground-") as temporary:
        directory = Path(temporary)
        helper = directory/"helper"
        common = ["clang", "-std=c11", "-Wall", "-Wextra", "-Werror"]
        subprocess.run([*common, ROOT/"tests/fixtures/style_supervisor_child.c", "-o", helper],
                       check=True, env=env)
        modes = [("debug", ["-O0", "-g"]), ("release", ["-O2", "-DNDEBUG"]),
                 ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined"])]
        for name, flags in modes:
            supervisor = directory/name
            subprocess.run([*common, *flags, ROOT/"scripts/bootstrap/style_supervisor.c", "-o", supervisor],
                           check=True, env=env)
            cases = directory/(name+"-cases")
            cases.mkdir(mode=0o700)
            ordinary(supervisor, helper, cases)
            controlled(supervisor, helper, cases)
            cleanup_cases(supervisor, helper, cases)
            print(name+": 21 foreground cases passed")


if __name__ == "__main__":
    main()
