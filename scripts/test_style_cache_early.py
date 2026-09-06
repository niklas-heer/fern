#!/usr/bin/env python3
"""Pre-supervisor failures clean only the current invocation's identified private staging."""
import os
from pathlib import Path
import tempfile
from test_style_cache import execute, snapshot


def main():
    """Repeat initial compiler and invalid-input failures while another invocation remains intact."""
    with tempfile.TemporaryDirectory(prefix='fern-style-early-') as temporary:
        directory = Path(temporary)
        root = snapshot(directory)
        cache = directory / 'cache'
        compiler = directory / 'failing cc'
        compiler.write_text('#!/bin/bash\nprintf "initial compiler failure\\n" >&2\nexit 7\n')
        compiler.chmod(0o700)
        env = dict(os.environ, FERN_STYLE_CACHE=str(cache), FERN_STYLE_CC=str(compiler))
        env.pop('LIBRARY_PATH', None)
        for number in range(4):
            result = execute(root, env)
            assert result[0] == 125 and result[1] == b'' and b'initial compiler failure' in result[2], result
            work = list(cache.glob('*/work.*'))
            assert len(work) == (0 if number == 0 else 1), work
            if number == 0:
                namespace = next(path for path in cache.iterdir() if path.is_dir())
                unrelated = namespace / 'work.other-invocation'
                unrelated.mkdir(mode=0o700)
                (unrelated / 'owner').write_text('FERN_STYLE_WORK_V1\n')
                (unrelated / 'sentinel').write_text('keep')
            assert (unrelated / 'sentinel').read_text() == 'keep'
        source = root / 'runtime/fern_runtime.c'
        source.chmod(0o666)
        result = execute(root, env)
        assert result[0] == 125 and result[1] == b''
        assert list(cache.glob('*/work.*')) == [unrelated]
        print('five early-failure cleanup cases passed')


if __name__ == '__main__':
    main()
