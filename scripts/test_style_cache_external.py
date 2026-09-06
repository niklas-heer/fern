#!/usr/bin/env python3
"""External dependency contents and hostile literal paths participate in native cache validation."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from test_style_cache import ROOT, entries, execute, snapshot


def tool_path(directory):
    """Provide only the finite native toolchain; neither Python nor uv exists on this PATH."""
    path = directory / 'native-tools'
    path.mkdir()
    names = ('bash', 'clang', 'just', 'pkg-config', 'ar', 'openssl', 'find', 'sort', 'stat',
             'mkdir', 'chmod', 'mktemp', 'cat', 'cmp', 'rmdir', 'cp', 'mv', 'rm', 'ln', 'uname', 'head', 'tail', 'ld', 'ldd')
    for name in names:
        selected = shutil.which(name)
        if selected:
            (path / name).symlink_to(selected)
    assert not (path / 'python3').exists() and not (path / 'uv').exists()
    return path


def main():
    """Keep pkg-config output stable while observed header and archive bytes change underneath it."""
    with tempfile.TemporaryDirectory(prefix='fern-style-external-') as temporary:
        directory = Path(temporary)
        root = snapshot(directory)
        dependencies = directory / "external ' space 🌿"
        dependencies.mkdir()
        header = dependencies / 'header #$colon:back\\slash.h'
        header.write_text('#define CACHE_LITERAL_HEADER 1\n')
        runtime = root / 'runtime/fern_runtime.c'
        # Quoted C header names allow literal backslashes; Clang -H keeps the real identity.
        runtime.write_bytes(('#include "' + header.name + '"\n').encode() + runtime.read_bytes())
        pkg = shutil.which('pkg-config')
        library = Path(subprocess.check_output([pkg, '--variable=libdir', 'bdw-gc']).decode().strip())
        include = subprocess.check_output([pkg, '--variable=includedir', 'bdw-gc']).decode().strip()
        archive = dependencies / 'libgc.a'
        shutil.copy2(library / 'libgc.a', archive)
        archive.chmod(0o600)
        pc = directory / 'pkg config'
        pc.mkdir()
        (pc / 'bdw-gc.pc').write_text(f'''libdir={dependencies}
includedir={include}
Name: bdw-gc
Description: literal external cache test
Version: 1.0
Cflags: -I${{includedir}}
Libs: -L${{libdir}} -lgc
''')
        tools = tool_path(directory)
        env = dict(os.environ, FERN_STYLE_CACHE=str(directory / 'cache'), FERN_STYLE_TOOL_PATH=str(tools),
                   PATH=str(tools), PKG_CONFIG_PATH=str(pc), CPATH=str(dependencies))
        env.pop('LIBRARY_PATH', None)
        expected = subprocess.run([ROOT / 'bin/check_style', '--help'], capture_output=True, check=True).stdout
        assert execute(root, env) == (0, expected, b'')
        stamp = header.stat()
        header.write_text('#define CACHE_LITERAL_HEADER 2\n')
        os.utime(header, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        assert execute(root, env) == (0, expected, b'')
        assert len(entries(Path(env['FERN_STYLE_CACHE']))) == 2
        obj = directory / 'unused.o'
        subprocess.run(['/usr/bin/clang', '-x', 'c', '-c', '-o', obj, '-'],
                       input=b'int cache_unused_symbol(void) { return 42; }\n', check=True)
        stamp = archive.stat()
        subprocess.run(['ar', 'r', archive, obj], check=True)
        os.utime(archive, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        assert execute(root, env) == (0, expected, b'')
        assert len(entries(Path(env['FERN_STYLE_CACHE']))) == 3
        source = directory / "literal ' 🌿.c"
        source.write_text('/* literal path */\n')
        args = ['--style-only', '--', source.name]
        oracle = subprocess.run([ROOT / 'bin/check_style', *args], cwd=directory, capture_output=True)
        assert execute(root, env, args, cwd=directory) == (oracle.returncode, oracle.stdout, oracle.stderr)
        print('four external/native-only/literal-cwd cache cases passed')


if __name__ == '__main__':
    main()
