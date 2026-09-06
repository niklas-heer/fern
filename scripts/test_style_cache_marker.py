#!/usr/bin/env python3
"""Cache marker publication is atomic and bounded, with no PID-lock stealing."""
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]


def initialize(cache):
    """Run only the authored cache initialization helpers, preserving all arguments literally."""
    command = '''source "$1/scripts/bootstrap/style_inputs.sh"
source "$1/scripts/bootstrap/style_cache.sh"
style_platform=$(/usr/bin/uname -s)
style_uid=$(/usr/bin/id -u)
style_stat_tool=$(command -v stat)
style_openssl=$(command -v openssl)
style_root=$1
style_cache_root "$2"
'''
    return subprocess.run(['/bin/bash', '-c', command, 'marker', ROOT, cache], capture_output=True, timeout=3)


def main():
    """A concurrent completed marker qualifies; unknown data is never adopted or deleted."""
    with tempfile.TemporaryDirectory(prefix='fern-style-marker-') as temporary:
        root = Path(temporary)
        cache = root / 'cache'
        cache.mkdir(mode=0o700)
        pending = cache / 'owner.ABCDef12'
        pending.write_text('FERN_STYLE_CACHE_V1\n')
        def publish():
            time.sleep(0.04)
            os.link(pending, cache / 'owner')
        writer = threading.Thread(target=publish)
        writer.start()
        result = initialize(cache)
        writer.join()
        assert result.returncode == 0, result
        assert pending.exists() and (cache / 'owner').read_text() == 'FERN_STYLE_CACHE_V1\n'
        foreign = root / 'foreign'
        foreign.mkdir(mode=0o700)
        (foreign / 'sentinel').write_text('keep')
        assert initialize(foreign).returncode == 125
        assert list(foreign.iterdir()) == [foreign / 'sentinel']
        unsafe = root / 'writable-parent'
        unsafe.mkdir()
        unsafe.chmod(0o777)
        assert initialize(unsafe / 'cache').returncode == 125
        assert not (unsafe / 'cache').exists()
        link = root / 'cache-link'
        link.symlink_to(cache, target_is_directory=True)
        assert initialize(str(link) + '/').returncode == 125
        print('four marker/ancestor ownership cases passed')


if __name__ == '__main__':
    main()
