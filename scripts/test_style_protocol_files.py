#!/usr/bin/env python3
"""Private supervisor metadata rejects nonregular files without blocking on FIFO opens."""
import os
from pathlib import Path
import subprocess
import tempfile
from test_style_foreground import ROOT, command, invocation


def reject(argv):
    """Keep the regression bounded even when the old reader blocks before ownership validation."""
    process = subprocess.Popen(list(map(str, argv)), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        out, err = process.communicate(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()
        process.communicate(timeout=2)
        raise AssertionError('metadata FIFO blocked supervisor') from None
    assert process.returncode == 125 and out == b'' and err, (process.returncode, out, err)


def main():
    """Exercise both public ownership protocols with writerless FIFOs in three build modes."""
    with tempfile.TemporaryDirectory(prefix='fern-style-protocol-') as temporary:
        directory = Path(temporary)
        for name, flags in [('debug', ['-O0']), ('release', ['-O2', '-DNDEBUG']),
                            ('sanitized', ['-O1', '-fsanitize=address,undefined'])]:
            helper = directory / name
            subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
                            ROOT / 'scripts/bootstrap/style_supervisor.c', '-o', helper], check=True)
            run = invocation(directory, helper, name)
            (run / 'owner').unlink()
            os.mkfifo(run / 'owner', mode=0o600)
            reject(command(helper, run))
            control = directory / ('work.' + name)
            control.mkdir(mode=0o700)
            os.mkfifo(control / 'owner', mode=0o600)
            reject([helper, '--launch', control, '/bin/false', ROOT, directory / 'cache', '/usr/bin:/bin', '--'])
            print(name + ': two nonregular ownership records rejected without blocking')


if __name__ == '__main__':
    main()
