#!/usr/bin/env python3
"""Exercise default Just recipes without Python/uv, preserving native arguments and exits."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RECIPES = {
    'style': ['--style-only', 'src', 'lib'],
    'style-lenient': ['--style-only', '--lenient', 'src', 'lib'],
    'pre-commit': ['--pre-commit', 'src', 'lib'],
    'check': ['src', 'lib'],
}


def main():
    """Use the real task runner and shell fixtures to observe dispatch and failure propagation."""
    root = Path(os.environ.get('FERN_RECIPE_ROOT', str(ROOT)))
    just = shutil.which('just')
    assert just, 'just is required'
    count = 0
    with tempfile.TemporaryDirectory(prefix='fern-native-recipes-') as temporary:
        work = Path(temporary)
        shutil.copyfile(root / 'Justfile', work / 'Justfile')
        (work / 'scripts').mkdir()
        (work / 'tools').mkdir()
        checker = work / 'scripts/check_style'
        checker.write_text('''#!/bin/sh
printf '%s\\n' "$@" > "$FERN_RECIPE_ARGS"
printf 'native stdout\\n'
printf 'native stderr\\n' >&2
exit "$FERN_RECIPE_STATUS"
''')
        checker.chmod(0o700)
        for name in ('uv', 'python', 'python3'):
            forbidden = work / 'tools' / name
            forbidden.write_text('#!/bin/sh\nprintf "unexpected Python tool\\n" >&2\nexit 99\n')
            forbidden.chmod(0o700)
        arguments = work / 'arguments'
        for recipe, expected in RECIPES.items():
            for code in (0, 7):
                if recipe == 'check' and code == 0:
                    continue  # Full check intentionally continues to explicit integration oracles.
                arguments.unlink(missing_ok=True)
                environment = dict(os.environ, PATH=str(work / 'tools') + os.pathsep + os.environ['PATH'],
                                   FERN_RECIPE_ARGS=str(arguments), FERN_RECIPE_STATUS=str(code))
                result = subprocess.run([just, '--justfile', str(work / 'Justfile'),
                                         '--working-directory', str(work), recipe], env=environment,
                                        text=True, capture_output=True, timeout=20)
                assert result.returncode == code, (recipe, code, result)
                assert arguments.read_text().splitlines() == expected, (recipe, result)
                assert result.stdout == 'native stdout\n', (recipe, result)
                assert 'native stderr\n' in result.stderr, (recipe, result)
                assert 'unexpected Python tool' not in result.stderr, (recipe, result)
                count += 1
    print(f'Native default recipes: {count} dispatch/stream/exit cases passed')


if __name__ == '__main__':
    main()
