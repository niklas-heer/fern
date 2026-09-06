#!/usr/bin/env python3
"""The private Clang build disables implicit user configs and rejects opaque loading flags."""
import os
import shlex
from pathlib import Path
import subprocess
import tempfile
from test_style_cache import ROOT, execute, snapshot


def flags(arguments):
    """Run the authored finite driver profile against literal decoded flag words."""
    shell = 'source "$1/scripts/bootstrap/style_flags.sh" || exit 125; shift; style_driver_flags "$@"'
    return subprocess.run(['/bin/bash', '-c', shell, 'profile', ROOT, *arguments], capture_output=True, timeout=3)


def main():
    """Pin ordinary project flags and actual Clang default-config isolation under private HOME/XDG."""
    assert flags(['-std=c11', '-O2', '-g', '-Wall', '-DNAME=literal space', '-I', 'literal include', '-L/lib', '-lm']).returncode == 0
    for forbidden in ('--config=private.cfg', '--config-user-dir=/tmp', '-fplugin=plugin.so', '-Xclang',
                      '-fpass-plugin=plugin.so', '@response', '-Wl,-plugin,plugin.so', '-B/private/tools'):
        result = flags([forbidden])
        assert result.returncode == 125 and b'unsupported Clang bootstrap flag' in result.stderr, result
    with tempfile.TemporaryDirectory(prefix='fern-style-config-') as temporary:
        directory = Path(temporary)
        root = snapshot(directory)
        home = directory / 'home'
        config = home / '.config/clang'
        config.mkdir(parents=True)
        triple = subprocess.check_output(['/usr/bin/clang', '-dumpmachine'], text=True).strip()
        (config / 'clang.cfg').write_text('--fern-invalid-implicit-config\n')
        (config / (triple + '-clang.cfg')).write_text('--fern-invalid-implicit-config\n')
        compiler = directory / 'configured clang'
        compiler.write_text('#!/bin/bash\nexec -a ' + shlex.quote(triple + '-clang') + ' /usr/bin/clang --config-user-dir="$HOME/.config/clang" "$@"\n')
        compiler.chmod(0o700)
        env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
                   FERN_STYLE_CC=str(compiler), FERN_STYLE_CACHE=str(directory / 'cache'))
        env.pop('LIBRARY_PATH', None)
        probe = subprocess.run([compiler, '-c', '-x', 'c', '/dev/null', '-o', directory / 'probe.o'],
                               env=env, capture_output=True, timeout=5)
        assert probe.returncode != 0 and b'fern-invalid-implicit-config' in probe.stderr, probe
        override = dict(env, CCC_OVERRIDE_OPTIONS='+-DUNTRACKED_OVERRIDE')
        rejected = execute(root, override)
        assert rejected[0] == 125 and b'unsupported Clang bootstrap environment' in rejected[2], rejected
        expected = subprocess.run([ROOT / 'bin/check_style', '--help'], capture_output=True, check=True).stdout
        result = execute(root, env)
        assert result == (0, expected, b''), result
        justfile = root / 'Justfile'
        justfile.write_text(justfile.read_text().replace('base_cflags := "', 'base_cflags := "-fplugin=forbidden.so '))
        result = execute(root, env)
        assert result[0] == 125 and result[1] == b'' and b'unsupported Clang bootstrap flag' in result[2], result
        print('nine driver-profile checks plus implicit-config and explicit-plugin native cases passed')


if __name__ == '__main__':
    main()
