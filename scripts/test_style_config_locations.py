#!/usr/bin/env python3
"""Real Clang default config searches cannot inject flags into the private compiler profile."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def compile_case(compiler, resource, user, system, source, output, protected):
    """Compare ordinary real-driver behavior with the authored compiler-only config isolation."""
    args = [str(compiler), '-resource-dir', resource, '--config-user-dir=' + str(user),
            '--config-system-dir=' + str(system), '-c', str(source), '-o', str(output)]
    env = dict(os.environ)
    env.pop('LIBRARY_PATH', None)
    env.pop('CLANG_NO_DEFAULT_CONFIG', None)
    if protected:
        script = ('source "$1/scripts/bootstrap/style_flags.sh"; shift; '
                  'export CLANG_NO_DEFAULT_CONFIG=caller; style_driver_run "$@"; status=$?; '
                  '[[ $CLANG_NO_DEFAULT_CONFIG == caller ]] || exit 126; exit "$status"')
        args = ['/bin/bash', '-c', script, 'config-profile', str(ROOT), str(output.parent / 'empty.cfg'), *args]
    return subprocess.run(args, env=env, capture_output=True, timeout=15)


def main():
    """Pin user, system and prefixed tool-directory defaults, then reject nonregular private config."""
    actual = Path(shutil.which('clang')).resolve()
    if os.uname().sysname == 'Darwin':
        actual = Path(subprocess.check_output(['/usr/bin/xcrun', '--find', 'clang'], text=True).strip())
    resource = subprocess.check_output([actual, '-print-resource-dir'], text=True).strip()
    triple = subprocess.check_output([actual, '-dumpmachine'], text=True).strip()
    with tempfile.TemporaryDirectory(prefix='fern-style-config-locations-') as temporary:
        root = Path(temporary)
        tool_dir, user, system = [root / name for name in ('tools', 'home', 'system')]
        for directory in (tool_dir, user, system): directory.mkdir(mode=0o700)
        compiler = tool_dir / (triple + '-clang')
        shutil.copyfile(actual, compiler)
        compiler.chmod(0o500)
        source = root / 'source.c'
        source.write_text('int value(void) { return 42; }\n')
        empty = root / 'empty.cfg'
        empty.write_bytes(b'')
        empty.chmod(0o400)
        for directory in (user, system, tool_dir):
            config = directory / (triple + '-clang.cfg')
            config.write_text('--fern-invalid-implicit-config\n')
            bad = compile_case(compiler, resource, user, system, source, root / 'out.o', False)
            assert bad.returncode != 0 and b'fern-invalid-implicit-config' in bad.stderr, bad
            good = compile_case(compiler, resource, user, system, source, root / 'out.o', True)
            assert good.returncode == 0 and good.stderr == b'', good
            config.unlink()
        empty.chmod(0o600)
        empty.write_text('-DUNEXPECTED\n')
        rejected = compile_case(compiler, resource, user, system, source, root / 'out.o', True)
        assert rejected.returncode == 125, rejected
        empty.unlink()
        os.mkfifo(empty, mode=0o600)
        rejected = compile_case(compiler, resource, user, system, source, root / 'out.o', True)
        assert rejected.returncode == 125, rejected
        print('three real config locations suppressed; nonempty/FIFO private configs rejected')


if __name__ == '__main__':
    main()
