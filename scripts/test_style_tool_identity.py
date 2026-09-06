#!/usr/bin/env python3
"""Equal executable bytes do not erase literal symlink lookup identity from the cache key."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def record(directory, tool):
    """Call only the authored metadata helper on bounded test-owned paths."""
    script = '''source "$1/scripts/bootstrap/style_inputs.sh"
style_work=$2
style_tools=("$3")
: > "$style_work/environment"
style_tool_links
'''
    return subprocess.run(['/bin/bash', '-c', script, 'identity', ROOT, directory, tool], capture_output=True, timeout=3)


def main():
    """Pin equal-byte retargeting and bounded cyclic links without executing either tool."""
    with tempfile.TemporaryDirectory(prefix='fern-style-tool-identity-') as temporary:
        root = Path(temporary)
        first, second = root / 'first literal', root / 'second literal'
        first.write_bytes(b'#!/bin/sh\nexit 0\n')
        second.write_bytes(first.read_bytes())
        tool = root / 'selected'
        tool.symlink_to(first)
        result = record(root, tool)
        assert result.returncode == 0, result
        before = (root / 'environment').read_bytes()
        tool.unlink()
        tool.symlink_to(second)
        assert record(root, tool).returncode == 0
        assert (root / 'environment').read_bytes() != before
        tool.unlink()
        tool.symlink_to(tool.name)
        assert record(root, tool).returncode == 125
        print('two tool lookup identity cases passed')


if __name__ == '__main__':
    main()
