#!/usr/bin/env python3
"""Finite standalone native-test supervisor protocol and lifecycle oracles."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 262144
TRAILER = b"\nFERN_TEST_END 1\n"


def decode(packet):
    """Validate the entire bounded version-one record, without accepting partial success."""
    assert len(packet) <= 2 * LIMIT + 256
    line, body = packet.split(b"\n", 1)
    assert len(line) < 128
    fields = line.split(b" ")
    assert len(fields) == 6 and fields[:2] == [b"FERN_TEST", b"1"]
    kind = fields[2]
    assert kind in (b"N", b"E")
    values = []
    for text in fields[3:]:
        assert text and len(text) <= 10 and text.isdigit()
        assert text == str(int(text)).encode()
        values.append(int(text))
    status, outlen, errlen = values
    assert outlen <= LIMIT and errlen <= LIMIT
    if kind == b"N":
        assert status <= 65535
        assert (status & 255) == 0 or (status < 256 and
            0 < os.WTERMSIG(status) < signal.NSIG and os.WIFSIGNALED(status))
    else:
        assert 1 <= status <= 8
    assert len(body) == outlen + errlen + len(TRAILER)
    assert body[outlen + errlen:] == TRAILER
    return kind, status, body[:outlen], body[outlen:outlen + errlen]


def protocol_cases():
    """The independent receiver rejects ambiguous, oversized and truncated records."""
    good = b"FERN_TEST 1 N 32000 0 0\n" + TRAILER
    assert decode(good) == (b"N", 125 << 8, b"", b"")
    bad = [good[:-1], good + b"x", good.replace(b" 1 N", b" 2 N"),
           good.replace(b"32000", b"032000"), good.replace(b"32000", b"65536"),
           good.replace(b"N 32000", b"E 0"), good.replace(b"N 32000", b"N 127"), good.replace(b"N 32000", b"N 128"),
           good.replace(b"N 32000", b"N 511"),
           good.replace(b"0 0\n", b"262145 0\n"), good.replace(b"N 32000", b"X 0")]
    for packet in bad:
        try:
            decode(packet)
        except (AssertionError, ValueError):
            continue
        raise AssertionError(f"malformed record accepted: {packet!r}")


def execute(argv, *, disconnect=False, preexec_fn=None):
    """Keep liveness open while reading; communicate would close it prematurely."""
    child = subprocess.Popen(list(map(str, argv)), stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             start_new_session=True, preexec_fn=preexec_fn)
    if disconnect:
        child.stdin.close()
    # These are test-only bounded reader threads; production Rust uses a synchronous read.
    import concurrent.futures
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        out = pool.submit(child.stdout.read, 2 * LIMIT + 257)
        err = pool.submit(child.stderr.read, 4096)
        try:
            code = child.wait(timeout=8)
            data, errors = out.result(timeout=2), err.result(timeout=2)
        except BaseException:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=5)
            raise
        finally:
            if not disconnect:
                child.stdin.close()
    assert code == 0 and not errors, (code, data, errors)
    return decode(data)


def ready(path):
    """Wait for child readiness without using a guessed spawn delay."""
    for _ in range(200):
        if path.exists() and path.read_text().endswith("\n"):
            return int(path.read_text())
        time.sleep(0.01)
    raise AssertionError("child did not report readiness")


def stopped(pid):
    """An orphan zombie is stopped, even when the host reaper is delayed."""
    for _ in range(100):
        result = subprocess.run(["ps", "-o", "stat=", "-p", str(pid)],
                                capture_output=True, timeout=2)
        if not result.stdout.strip() or result.stdout.strip().startswith(b"Z"):
            return
        time.sleep(0.01)
    raise AssertionError(f"owned descendant still alive: {pid}")


def matrix(binary, fixture, directory):
    """Exercise independent native status, stream, ownership and cancellation semantics."""
    def command(*args, timeout=2000):
        return [binary, timeout, directory, "--", fixture, *args]
    values = ["", "a b", "é🌿", "\";$(touch nope)", "line\nbreak"]
    expected = b"".join(str(len(v.encode())).encode() + b":" + v.encode() + b"\n" for v in values)
    assert execute(command("args", *values)) == (b"N", 0, expected, b"")
    for status in (0, 7, 125, 127, 255):
        assert execute(command("exit", status)) == (b"N", status << 8, b"", b"")
    actual = execute(command("die"))
    assert actual[0] == b"N" and os.WTERMSIG(actual[1]) == signal.SIGTERM
    assert execute(command("streams")) == (b"N", 7 << 8, b"out\0\xf0\x9f\x8c\xbf", b"err\n")
    result = execute(command("leaks"))
    assert result == (b"N", 0, b"", b""), result
    assert execute(command("stdin")) == (b"N", 7 << 8, b"", b"")
    for fd in (1, 2):
        for length in (0, 4096, LIMIT):
            result = execute(command("emit", length, fd))
            assert result == (b"N", 0, b"x" * length if fd == 1 else b"",
                              b"x" * length if fd == 2 else b"")
        assert execute(command("emit", LIMIT + 1, fd))[:2] == (b"E", 4)
    assert execute(command("close_wait", timeout=50))[:2] == (b"E", 3)
    assert execute(command("exit", 0), disconnect=True)[:2] == (b"E", 8)
    assert execute([binary, 2000, directory, "--", directory / "missing"])[:2] == (b"E", 2)
    for duration in (0, -1, 60001):
        assert execute(command("exit", 0, timeout=duration))[:2] == (b"E", 1)
    assert execute(command("exit", 0), preexec_fn=lambda: signal.signal(
        signal.SIGCHLD, signal.SIG_IGN))[:2] == (b"E", 7)
    sibling = subprocess.Popen(["/bin/sleep", "5"])
    try:
        for mode, timeout, expected in (("descendant", 2000, b"N"),
                                        ("descendant_wait", 500, b"E")):
            path = directory / f"{mode}.pid"
            result = execute(command(mode, path, timeout=timeout))
            assert result[0] == expected, result
            stopped(ready(path))
            assert sibling.poll() is None
            path.unlink()
    finally:
        sibling.terminate()
        sibling.wait(timeout=3)
    path = directory / "escaped.pid"
    assert execute(command("escaped", path, timeout=500))[:2] == (b"E", 3)
    ready(path)
    path.unlink()
    assert list(directory.iterdir()) == [], list(directory.iterdir())


def cancellation_cases(binary, fixture, directory):
    """Ready children permit deterministic signal and parent-disconnect cleanup assertions."""
    for mode in ("disconnect", "signal"):
        path = directory / (mode + ".pid")
        args = [binary, 3000, directory, "--", fixture, "descendant_wait", path]
        child = subprocess.Popen(list(map(str, args)), stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            pid = ready(path)
            if mode == "disconnect":
                child.stdin.close()
            else:
                child.send_signal(signal.SIGTERM)
            child.wait(timeout=5)
            packet = child.stdout.read(2 * LIMIT + 257)
            assert child.returncode == 0 and not child.stderr.read(4096)
            assert decode(packet)[:2] == (b"E", 8 if mode == "disconnect" else 6)
            stopped(pid)
        finally:
            child.stdin.close()
            if child.poll() is None:
                child.kill()
                child.wait(timeout=5)
            path.unlink(missing_ok=True)
    assert list(directory.iterdir()) == []


def publication_cases(binary, fixture, directory):
    """A non-reading or closed parent pipe cannot hang native result publication."""
    args = [binary, 2000, directory, "--", fixture, "emit", LIMIT, 1]
    child = subprocess.Popen(list(map(str, args)), stdin=subprocess.PIPE,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        assert child.wait(timeout=5) == 125
        packet = child.stdout.read(2 * LIMIT + 257)
        assert len(packet) < LIMIT
        assert not child.stderr.read(4096)
    finally:
        child.stdin.close()
        if child.poll() is None:
            child.kill()
            child.wait(timeout=5)
    assert list(directory.iterdir()) == []
    for fd in (0, 1):
        child = subprocess.Popen(list(map(str, args)), stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 preexec_fn=lambda: os.close(fd))
        assert child.wait(timeout=5) == 125
        child.stdin.close()
    assert execute(args, preexec_fn=lambda: os.close(2))[:2] == (b"N", 0)


def ownership_cases(binary, fixture, directory):
    """Reject FIFO/symlink inputs and pre-existing state without modifying other owners."""
    def invoke(path):
        return execute([binary, 1000, path, "--", fixture, "exit", 0])
    plain = directory / "not-a-shell-script"
    escaped = directory / "unexpected-shell"
    plain.write_text(f"touch '{escaped}'\n")
    plain.chmod(0o700)
    assert execute([binary, 1000, directory, "--", plain])[:2] == (b"E", 2)
    assert not escaped.exists()
    plain.unlink()
    marker = directory / "marker"
    marker.write_text("unchanged")
    link = directory / "link"
    link.symlink_to(directory, target_is_directory=True)
    assert invoke(link)[:2] == (b"E", 5)
    fifo = directory / "fifo"
    os.mkfifo(fifo)
    assert invoke(fifo)[:2] == (b"E", 5)
    staging = directory / "capture"
    staging.mkdir(mode=0o700)
    (staging / "foreign").write_text("owned elsewhere")
    assert invoke(directory)[:2] == (b"E", 5)
    assert (staging / "foreign").read_text() == "owned elsewhere"
    assert marker.read_text() == "unchanged"
    (staging / "foreign").unlink()
    staging.rmdir()
    marker.unlink()
    fifo.unlink()
    link.unlink()


def concurrent_cases(binary, fixture, directory):
    """Independent native invocations never share staging files or test statuses."""
    import concurrent.futures
    def worker(index):
        private = directory / str(index)
        private.mkdir(mode=0o700)
        result = execute([binary, 2000, private, "--", fixture, "exit", index])
        assert result == (b"N", index << 8, b"", b"")
        assert list(private.iterdir()) == []
        private.rmdir()
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(worker, range(16)))


def main():
    """Build only isolated standalone artifacts; never build the shared runtime/compiler."""
    argparse.ArgumentParser(description=__doc__).parse_args()
    protocol_cases()
    env = dict(os.environ)
    env.pop("LIBRARY_PATH", None)
    env.update(ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
    os.environ.update(ASAN_OPTIONS=env["ASAN_OPTIONS"], UBSAN_OPTIONS=env["UBSAN_OPTIONS"])
    with tempfile.TemporaryDirectory(prefix="fern-test-supervisor-") as tmp:
        directory = Path(tmp)
        common = ["clang", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
        fixture = directory / "literal 'é child"
        subprocess.run([*common, ROOT / "tests/fixtures/test_supervisor_child.c", "-o", fixture],
                       check=True, env=env)
        for mode, flags in (("debug", ["-O0", "-g"]), ("release", ["-O2", "-DNDEBUG"]),
                            ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined"])):
            binary = directory / mode
            subprocess.run([*common, *flags, ROOT / "tools/test_supervisor.c", "-o", binary],
                           check=True, env=env)
            state = directory / (mode + "-state")
            subprocess.run([*common, *flags, ROOT / "tests/fixtures/test_supervisor_state.c",
                            "-o", state], check=True, env=env)
            live = subprocess.Popen([state], stdin=subprocess.PIPE, env=env)
            assert live.wait(timeout=5) == 0
            live.stdin.close()
            cases = directory / (mode + "-cases")
            cases.mkdir(mode=0o700)
            matrix(binary, fixture, cases)
            cancellation_cases(binary, fixture, cases)
            publication_cases(binary, fixture, cases)
            ownership_cases(binary, fixture, cases)
            concurrent_cases(binary, fixture, cases)
            print(mode + ": native protocol/lifecycle + cancellation/publication/ownership/concurrency passed", flush=True)


if __name__ == "__main__":
    main()
