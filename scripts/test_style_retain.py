#!/usr/bin/env python3
"""A disappearing cache candidate never abandons a partially retained invocation directory."""
from pathlib import Path
import subprocess
import tempfile
from test_style_cache_ownership import ROOT, entry


def main():
    """Force missing executable links at either stage and require exact owned cleanup before retry."""
    with tempfile.TemporaryDirectory(prefix='fern-style-retain-') as temporary:
        directory = Path(temporary)
        ready = entry(directory, 'entry.fixture') / 'ready'
        source = '''source "$1/scripts/bootstrap/style_inputs.sh"
source "$1/scripts/bootstrap/style_cache.sh"
source "$1/scripts/bootstrap/style_cleanup.sh"
style_cache=$2
style_platform=$(/usr/bin/uname -s)
style_uid=$(/usr/bin/id -u)
style_stat_tool=$(command -v stat)
style_retain "$3"
'''
        for name in ('supervisor', 'program'):
            (ready / name).unlink()
            result = subprocess.run(['/bin/bash', '-c', source, 'retain', ROOT, directory, ready],
                                    capture_output=True, timeout=5)
            assert result.returncode in (1, 125), result
            assert list(directory.glob('run.*')) == [], 'partial retain abandoned run directory'
            (ready / name).write_text('fixture\n')
        print('two partial retain failures cleaned before candidate retry')


if __name__ == '__main__':
    main()
