#!/usr/bin/env python3
"""Keep syntactic derive-list limits separate from checker-owned trait availability."""
import argparse
from pathlib import Path
import tempfile
from test_editor_parity import command, parse


def main():
    """Compare the exact32/33 trait syntax boundary with the frozen Rust formatter/parser."""
    options = argparse.ArgumentParser()
    options.add_argument('--tree-sitter', required=True)
    options.add_argument('--rust', required=True)
    options.add_argument('--wasm', action='store_true')
    args = options.parse_args()
    with tempfile.TemporaryDirectory(prefix='fern-editor-derive-limits-') as temporary:
        path = Path(temporary) / 'limits.fn'
        for count in (32, 33):
            traits = ','.join('Trait' + str(i) for i in range(count))
            source = 'newtype Id derive(' + traits + ') = Id(Int)\nfn main():()\n'
            path.write_text(source)
            native = parse(args.tree_sitter, path, args.wasm)
            rust = command([args.rust, 'fmt', str(path)], success=False)
            assert (native.returncode == 0) == (count == 32), (count, native.stdout)
            if count == 32:
                assert native.stdout.count('trait:') == 32, native.stdout
            assert (rust.returncode == 0) == (count == 32), (count, rust.stderr)
    print('Editor derive syntax:32 accepted and33 rejected by editor and Rust parser')


if __name__ == '__main__':
    main()
