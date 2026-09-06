#!/usr/bin/env python3
"""Prove standalone build supervision before any native cache/default changes."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(argv, **kwargs):
    """Capture finite fixture processes with an outer timeout and owned cleanup."""
    child = subprocess.Popen(list(map(str, argv)), stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, start_new_session=True, **kwargs)
    try:
        out, err = child.communicate(timeout=8)
    except subprocess.TimeoutExpired:
        os.killpg(child.pid, signal.SIGKILL)
        child.communicate(timeout=5)
        raise AssertionError(f"supervisor test timed out: {argv}")
    return child.returncode, out, err


def assert_stopped(pid):
    """Bound polling; an orphan zombie has stopped even if its reaper is slow."""
    for _ in range(100):
        result = subprocess.run(["ps", "-o", "stat=", "-p", str(pid)],
                                capture_output=True, timeout=2)
        if not result.stdout.strip() or result.stdout.strip().startswith(b"Z"):
            return
        time.sleep(0.01)
    raise AssertionError(f"descendant remains alive: {pid}")


def ready(path):
    """Wait for explicit child readiness rather than guessing spawn timing."""
    for _ in range(200):
        if path.exists():
            text = path.read_text()
            if text.endswith("\n"):
                return int(text)
        time.sleep(0.01)
    raise AssertionError("child readiness was not published")


def literal_cases(command):
    """Preserve exact argv, binary output, ordinary statuses and both stream caps."""
    for status in (0, 7, 127):
        assert run(command("exit", status)) == (status, b"", b"")
    values = ["", "a b", "'\";$(touch nope)", "é🌿", "line\nbreak"]
    expected = b"".join(str(len(v.encode())).encode()+b":"+v.encode()+b"\n" for v in values)
    assert run(command("args", *values)) == (0, expected, b"")
    assert run(["/bin/bash", "-c", 'exec "$@"', "supervisor-test",
                *command("args", *values)]) == (0, expected, b"")
    assert run(command("streams")) == (7, b"out\0\xf0\x9f\x8c\xbf", b"err\n")
    assert run(command("dual", cap=409600)) == (0, b"x"*409600, b"x"*409600)
    for fd in (1, 2):
        for size in (0, 1, 4096):
            actual = run(command("emit", size, fd, cap=size))
            assert actual == (0, b"x"*size if fd == 1 else b"",
                              b"x"*size if fd == 2 else b""), actual
        actual = run(command("emit", 4097, fd, cap=4096))
        assert actual[0] == 125 and b"output limit exceeded" in actual[2], actual
    assert run(command("args", *([""]*4094), cap=16384)) == (0, b"0:\n"*4094, b"")
    assert run(command("args", *([""]*4095)))[0] == 125


def group_cases(command, directory):
    """Reap exited leaders only after group cleanup, preserving unrelated processes."""
    actual = run(command("close_wait", timeout=50))
    assert actual[0] == 125 and b"deadline exceeded" in actual[2], actual
    sibling = subprocess.Popen(["/bin/sleep", "5"])
    try:
        path = directory/"descendant.pid"
        assert run(command("descendant", path))[0] == 0
        assert_stopped(ready(path))
        assert sibling.poll() is None
        path = directory/"timeout.pid"
        actual = run(command("descendant_wait", path, timeout=500))
        assert actual[0] == 125 and b"deadline exceeded" in actual[2], actual
        assert_stopped(ready(path))
        assert sibling.poll() is None
    finally:
        sibling.terminate()
        sibling.wait(timeout=3)


def interruption_cases(command, directory):
    """Interruptions work with inherited blocked signals and clean ready descendants."""
    cases = [(signal.SIGINT, False), (signal.SIGTERM, False),
             (signal.SIGHUP, False), (signal.SIGTERM, True)]
    for sig, blocked in cases:
        path = directory/f"signal-{sig}-{blocked}.pid"
        block = (lambda: signal.pthread_sigmask(signal.SIG_BLOCK, {sig})) if blocked else None
        process = subprocess.Popen(list(map(str, command("descendant_wait", path))),
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   start_new_session=True, preexec_fn=block)
        try:
            pid = ready(path)
            process.send_signal(sig)
            out, err = process.communicate(timeout=5)
            assert process.returncode == 128+sig and b"interrupted" in err, (out, err)
            assert_stopped(pid)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.communicate(timeout=5)


def descriptor_cases(command):
    """Closed stdio is supported; publication failure cannot silently become success."""
    for mask in range(8):
        def close_stdio():
            for fd in range(3):
                if mask & (1 << fd):
                    os.close(fd)
        assert run(command("exit", 7), preexec_fn=close_stdio)[0] == 7
    actual = run(command("emit", 1, 1), preexec_fn=lambda: os.close(1))
    assert actual[0] == 125 and b"supervisor IO failed" in actual[2], actual
    actual = run(command("exit", 0),
                 preexec_fn=lambda: signal.signal(signal.SIGCHLD, signal.SIG_IGN))
    assert actual[0] == 125 and b"SIGCHLD" in actual[2], actual
    read_fd, write_fd = os.pipe()
    os.close(read_fd)
    try:
        def broken_stderr():
            os.dup2(write_fd, 2)
        actual = run(command(timeout=0), pass_fds=(write_fd,), preexec_fn=broken_stderr)
        assert actual[0] == 125, actual
        actual = run(command("emit", 1, 2), pass_fds=(write_fd,), preexec_fn=broken_stderr)
        assert actual[0] == 125, actual
    finally:
        os.close(write_fd)


def rejection_cases(supervisor, helper, directory):
    """Invalid limits and non-executable text cannot trigger an implicit shell."""
    script = directory/"not shell"
    script.write_text('touch "'+str(directory/'escaped')+'"\n')
    script.chmod(0o700)
    assert run([supervisor, 2000, 4096, "--", script])[0] == 125
    assert not (directory/'escaped').exists()
    missing = run([supervisor, 2000, 4096, "--", directory/'missing'])
    assert missing[0] == 125 and b"spawn failed" in missing[2], missing
    for args in [[0, 4096, "--", helper], [1, 16777217, "--", helper],
                 [1, 0, "--", "relative"], [600001, 0, "--", helper],
                 [1, 0, "--", ""], ["-1", 0, "--", helper]]:
        assert run([supervisor, *args])[0] == 125


def matrix(supervisor, helper, directory):
    """Run five independent finite behavioral groups against each build mode."""
    def command(*args, timeout=2000, cap=4096):
        return [supervisor, timeout, cap, "--", helper, *args]
    literal_cases(command)
    group_cases(command, directory)
    interruption_cases(command, directory)
    descriptor_cases(command)
    rejection_cases(supervisor, helper, directory)


def main():
    """Compile isolated artifacts in debug, release and sanitizer modes, without runtime."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    env = dict(os.environ)
    env.pop("LIBRARY_PATH", None)
    env.update(ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    os.environ.update(ASAN_OPTIONS=env["ASAN_OPTIONS"], UBSAN_OPTIONS=env["UBSAN_OPTIONS"])
    with tempfile.TemporaryDirectory(prefix="fern-style-supervisor-") as temporary:
        directory = Path(temporary)
        helper = directory/"literal 'é helper"
        common = ["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
        subprocess.run([*common, ROOT/'tests/fixtures/style_supervisor_child.c', "-o", helper],
                       check=True, env=env)
        for name, flags in [("debug", ["-O0", "-g"]), ("release", ["-O2", "-DNDEBUG"]),
                            ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined"])]:
            binary = directory/name
            subprocess.run([*common, *flags, ROOT/'scripts/bootstrap/style_supervisor.c',
                            "-o", binary], check=True, env=env)
            state = directory/(name+'-state')
            subprocess.run([*common, *flags, ROOT/'tests/fixtures/style_supervisor_state.c',
                            "-o", state], check=True, env=env)
            checked = run([state])
            assert checked[0] == 0, checked
            case = directory/(name+'-cases')
            case.mkdir()
            matrix(binary, helper, case)
            print(name+": five behavioral groups + private ownership matrix passed")


if __name__ == "__main__":
    main()
