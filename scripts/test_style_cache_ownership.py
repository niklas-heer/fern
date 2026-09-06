#!/usr/bin/env python3
"""Pruning touches only complete owned fixed-shape bundles and never retained run links."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FILES = ('bundle', 'artifacts', 'base', 'dependencies', 'search-roots', 'search', 'owner',
         'program', 'program.sha256', 'supervisor', 'supervisor.sha256', 'metadata', 'metadata.sha256')


def entry(root, name):
    """Construct exactly the private complete-bundle protocol for a deletion-only test."""
    path = root / name
    path.mkdir(mode=0o700)
    ready = path / 'ready'
    ready.mkdir(mode=0o700)
    for name in FILES:
        (ready / name).write_text('FERN_STYLE_ENTRY_V1\n' if name == 'owner' else 'fixture\n')
    return path


def prune(root, keep, clear=False):
    """Source authored helpers only; all cache data remains ordinary arguments/files."""
    script = '''source "$1/scripts/bootstrap/style_inputs.sh"
source "$1/scripts/bootstrap/style_cache.sh"
style_cache=$2
style_current_entry=$3
style_platform=$(/usr/bin/uname -s)
style_uid=$(/usr/bin/id -u)
style_stat_tool=$(command -v stat)
if [[ $4 == clear ]]; then style_clear; else style_prune; fi
'''
    return subprocess.run(['/bin/bash', '-c', script, 'prune', ROOT, root, keep, 'clear' if clear else 'prune'], capture_output=True, timeout=5)


def main():
    """Pin retention, open inode survival, unexpected children and marker/permission rejection."""
    with tempfile.TemporaryDirectory(prefix='fern-style-prune-') as temporary:
        root = Path(temporary)
        for number in range(9):
            entry(root, 'entry.' + str(number))
        keep = root / 'entry.8'
        retained = root / 'retained'
        os.link(root / 'entry.0/ready/program', retained)
        result = prune(root, keep)
        assert result.returncode == 0, result
        assert len(list(root.glob('entry.*'))) == 8 and keep.exists()
        assert retained.read_text() == 'fixture\n'
        unexpected = entry(root, 'entry.0')
        (unexpected / 'ready/sentinel').write_text('keep')
        result = prune(root, keep)
        assert result.returncode == 125 and (unexpected / 'ready/sentinel').read_text() == 'keep', result
        assert (unexpected / 'ready/owner').exists()
        (unexpected / 'ready/sentinel').unlink()
        (unexpected / 'ready/owner').write_text('WRONG\n')
        assert prune(root, keep).returncode == 125
        assert (unexpected / 'ready/program').exists()
        (unexpected / 'ready/owner').write_text('FERN_STYLE_ENTRY_V1\n')
        assert prune(root, keep, clear=True).returncode == 0
        assert list(root.glob('entry.*')) == [] and retained.read_text() == 'fixture\n'
        print('five owned pruning/clear checks passed')


if __name__ == '__main__':
    main()
