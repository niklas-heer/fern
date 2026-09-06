#!/usr/bin/env python3
"""Concurrent cold builds publish independently; launcher-PID interruption cannot poison a peer."""
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
from test_style_cache import ROOT, snapshot


def start(root, env):
    """Start one bounded test-owned launcher group; production owns its own worker identity."""
    return subprocess.Popen([root / 'scripts/check_style', '--help'], env=env, cwd=root,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)


def finish(process):
    """Outer test deadline cleans only its own known live process group on an assertion failure."""
    try:
        out, err = process.communicate(timeout=180)
        return process.returncode, out, err
    except BaseException:
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate()
        raise


def main():
    """Exercise one simultaneous cold cache and one cancellation during a real compiler call."""
    with tempfile.TemporaryDirectory(prefix='fern-style-concurrent-') as temporary:
        directory = Path(temporary)
        root = snapshot(directory)
        env = dict(os.environ, FERN_STYLE_CACHE=str(directory / 'cache'))
        env.pop('LIBRARY_PATH', None)
        expected = subprocess.run([ROOT / 'bin/check_style', '--help'], capture_output=True, check=True).stdout
        first, second = start(root, env), start(root, env)
        assert finish(first) == (0, expected, b'')
        assert finish(second) == (0, expected, b'')
        compiler = directory / 'waiting cc'
        compiler.write_text('''#!/bin/bash
for arg in "$@"; do
    if [[ $arg == src/main.c && -n ${FERN_STYLE_TEST_READY-} ]]; then
        printf 'ready\\n' > "$FERN_STYLE_TEST_READY"
        /bin/sleep 2
    fi
done
exec /usr/bin/clang "$@"
''')
        compiler.chmod(0o700)
        env.update(FERN_STYLE_CACHE=str(directory / 'interrupted-cache'), FERN_STYLE_CC=str(compiler))
        ready = directory / 'ready'
        cancelled = start(root, dict(env, FERN_STYLE_TEST_READY=str(ready)))
        survivor = start(root, env)
        for _ in range(1000):
            if ready.exists():
                break
            time.sleep(0.01)
        else:
            raise AssertionError('cold native worker did not reach the compiler')
        cancelled.send_signal(signal.SIGTERM)  # Only the launcher PID, not the shell's child PID/group.
        result = finish(cancelled)
        assert result[0] == 143 and result[1] == b'' and b'interrupted' in result[2], result
        assert finish(survivor) == (0, expected, b'')
        print('four concurrent/cold-interruption native cases passed')


if __name__ == '__main__':
    main()
