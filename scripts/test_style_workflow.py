#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.14,<3.15"
# dependencies = ["rich>=13.0"]
# ///
"""Compare literal workflows with the pinned Python 3.14 argparse reference profile."""
import argparse
from collections import Counter
import check_style
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "scripts/check_style.py"
FAKE = '''#!{python}
import json, pathlib, sys
root=pathlib.Path.cwd()
args=[pathlib.Path(sys.argv[0]).name,*sys.argv[1:]]
with (root/'calls.jsonl').open('a') as stream: stream.write(json.dumps(args)+'\\n')
config=json.loads((root/'workflow.json').read_text())
reply=config.get(' '.join(args), config.get(args[0], [0,'','']))
sys.stdout.write(reply[1]);sys.stderr.write(reply[2]);sys.exit(reply[0])
'''


def setup(directory, case):
    """Create literal fake tools and fixtures, never invoking a real build or Git mutation."""
    for name in ['tools', 'bin', 'src', 'lib']:
        (directory / name).mkdir()
    for name in ['just', 'git']:
        tool = directory / 'tools' / name
        tool.write_text(FAKE.format(python=sys.executable)); tool.chmod(0o700)
    if case.get('compiler', True):
        tool = directory / 'bin/fern'
        tool.write_text(FAKE.format(python=sys.executable)); tool.chmod(0o700)
    (directory / 'workflow.json').write_text(json.dumps(case.get('replies', {})))
    for name, contents in case.get('files', {}).items():
        file = directory / name; file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(contents)
    if case.get('examples'):
        (directory / 'examples').mkdir(exist_ok=True)
    environment = dict(os.environ, PATH=str(directory / 'tools') + os.pathsep + os.environ['PATH'],
                       COLUMNS='1000', NO_COLOR='1', TERM='dumb')
    return environment


def execute(command, directory, environment):
    """Capture bounded test processes and normalized text without depending on table borders."""
    result = subprocess.run(command, cwd=directory, env=environment,
                            text=True, capture_output=True, timeout=30)
    text = re.sub(r'\x1b\[[0-9;]*m', '', result.stdout + result.stderr)
    log = directory / 'calls.jsonl'
    calls = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
    return result.returncode, text, calls, result.stdout, result.stderr


def compare(native, case, parent):
    """Pin expected semantic evidence on the reference before requiring native equality."""
    results = []
    for label, command in [('python', [sys.executable, str(REFERENCE)]), ('native', [str(native)])]:
        directory = parent / (case['name'] + '-' + label); directory.mkdir()
        environment = setup(directory, case)
        results.append(execute(command + case.get('args', []), directory, environment))
        code, output, calls = results[-1][:3]
        if code == 2:
            assert results[-1][3] == "" and "error:" in results[-1][4], (case["name"], label, "CLI stream", results[-1])
        elif case["name"].startswith("help") or case["name"] == "short_help":
            assert results[-1][3] and results[-1][4] == "", (case["name"], label, "help stream", results[-1])
        assert code == case.get('code', 0), (case['name'], label, code, output)
        for text in case.get('contains', []):
            assert text in output, (case['name'], label, 'missing', text, output)
        for text in case.get('absent', []):
            assert text not in output, (case['name'], label, 'unexpected', text, output)
        assert not (directory / 'escape').exists(), (case['name'], label, 'shell expansion')
        if case.get('no_calls'):
            assert not calls, (case['name'], label, calls)
    assert results[0][2] == results[1][2], (case['name'], 'literal argv/order', results[0][2], results[1][2])
    print('  PASS', case['name'])


def build_cases():
    """Cover every build/test state, stderr inspection and continuation after failure."""
    common = ['Tests:', 'Examples:', 'FERN_STYLE Compliance']
    return [
        dict(name='clean_failure', replies={'just clean':[3,'clean detail','']}, code=1,
             contains=['Build: Clean failed', *common], absent=['clean detail']),
        dict(name='build_failure', replies={'just debug':[2,'build stdout','build stderr']}, code=1,
             contains=['Build: Build failed', 'build stdoutbuild stderr', *common]),
        dict(name='warning_case', replies={'just debug':[0,'','WARNING: native warning']}, code=1,
             contains=['Build: Build has warnings/errors', 'WARNING: native warning', *common]),
        dict(name='error_case', replies={'just debug':[0,'ERROR: native error','']}, code=1,
             contains=['Build: Build has warnings/errors', *common]),
        dict(name='tests_count', replies={'just test':[0,'Passed: 0042\n','']},
             contains=['Build: Build clean', 'Tests: All tests passing (0042 tests)', 'All checks passed!']),
        dict(name='tests_count_stderr', replies={'just test':[0,'Passed:', ' \n17\n']},
             contains=['Tests: All tests passing (17 tests)']),
        dict(name='tests_plain', replies={'just test':[0,'All tests passed\n','']},
             contains=['Tests: Tests passed', 'No .c files found to check']),
        dict(name='tests_failure', replies={'just test':[9,'test stdout','test stderr']}, code=1,
             contains=['Tests: Tests failed', 'test stdouttest stderr', 'Examples:']),
    ]


def example_cases():
    """Example names remain literal argv and sorted failures retain bounded diagnostic details."""
    names = ["examples/a space ' quote.fn", 'examples/b; literal.fn', 'examples/z.fn', 'examples/$(touch escape).fn']
    return [
        dict(name='examples_missing', contains=['Examples: No examples directory']),
        dict(name='examples_empty', examples=True, files={'examples/readme.txt':'ignored'},
             contains=['Examples: No .fn files in examples/']),
        dict(name='examples_regular_file', files={'examples':'not a directory'},
             contains=['Examples: No .fn files in examples/']),
        dict(name='example_unicode_detail', files={'examples/a.fn':''},
             replies={'fern':[1,'','🌿'*101]}, code=1,
             contains=['a.fn: ' + '🌿'*100], absent=['🌿'*101]),
        dict(name='compiler_missing', compiler=False, files={'examples/a.fn':''}, code=1,
             contains=['Examples: Fern compiler not built', 'Run just debug first']),
        dict(name='literal_examples', files={name:'' for name in reversed(names)},
             contains=['Examples: All 4 examples type-check']),
        dict(name='example_failure_details', files={f'examples/{i}.fn':'' for i in range(7)},
             replies={'fern':[1,'stdout hidden','stderr detail ' + 'x'*110]}, code=1,
             contains=['Examples: 7/7 examples failed type check', '0.fn: stderr detail', '4.fn: stderr detail'],
             absent=['5.fn: stderr detail', 'stdout hidden']),
    ]


def git_cases():
    """Git reminders are advisory and follow staged code, design and feature predicates exactly."""
    args = ['--style-only', '--pre-commit']
    return [
        dict(name='git_unavailable', args=args, replies={'git rev-parse --git-dir':[1,'','']},
             contains=['No issues detected', 'All checks passed!']),
        dict(name='git_diff_failure', args=args, replies={'git diff --cached --name-only --diff-filter=ACM':[1,'','']},
             contains=['No issues detected']),
        dict(name='git_reminders', args=args, replies={'git diff --cached --name-only --diff-filter=ACM':[0,'include/FEATURE.h\n','']},
             contains=['Reminder: Consider updating ROADMAP.md for feature changes',
                       'Reminder: Did you make a design decision? Consider /decision', 'All checks passed!']),
        dict(name='git_docs_present', args=args, replies={'git diff --cached --name-only --diff-filter=ACM':[0,'include/feature.h\nROADMAP.md\nDECISIONS.md\n','']},
             contains=['No issues detected'], absent=['Reminder:']),
        dict(name='git_noncode', args=args, replies={'git diff --cached --name-only --diff-filter=ACM':[0,'feature.md\nDESIGN.md\n','']},
             contains=['No issues detected'], absent=['Reminder:']),
    ]


def cli_cases():
    """Argument validation happens before work, and summary retains warning/error classification."""
    warnings = (ROOT/'tests/style_fixtures/nested/warnings.c').read_text()
    return [
        dict(name='help', args=['--help'], no_calls=True, contains=['--lenient','--style-only','--pre-commit','--summary','src lib']),
        dict(name='short_help', args=['-h'], no_calls=True, contains=['--summary']),
        dict(name='help_cluster', args=['-hh'], no_calls=True, contains=['--summary']),
        dict(name='help_suffix', args=['-hfoo'], no_calls=True, contains=['--summary']),
        dict(name='help_suffix_value', args=['-hfoo=bar'], no_calls=True, contains=['--summary']),
        dict(name='short_help_value', args=['-h=yes'], code=2, no_calls=True, contains=["argument -h/--help: ignored explicit argument 'yes'"]),
        dict(name='cluster_help_value', args=['-hh=yes'], code=2, no_calls=True, contains=["argument -h/--help: ignored explicit argument 'yes'"]),
        dict(name='empty_help_value', args=['-h='], code=2, no_calls=True, contains=["ignored explicit argument ''"]),
        dict(name='ambiguous_flag', args=['--s'], code=2, no_calls=True, contains=['ambiguous option: --s could match --style-only, --summary']),
        dict(name='ambiguous_value', args=['--s=yes'], code=2, no_calls=True, contains=['ambiguous option: --s=yes could match --style-only, --summary']),
        dict(name='abbreviated_value', args=['--sty=yes'], code=2, no_calls=True, contains=["argument --style-only: ignored explicit argument 'yes'"]),
        dict(name='empty_flag_value', args=['--style-only='], code=2, no_calls=True, contains=["ignored explicit argument ''"]),
        dict(name='negative_integer_path', args=['--style-only','-1'], no_calls=True, contains=['No .c files found to check']),
        dict(name='negative_decimal_path', args=['--style-only','-.5'], no_calls=True, contains=['No .c files found to check']),
        dict(name='positional_terminator', args=['--style-only','src','--','lib'], no_calls=True, contains=['No .c files found to check']),
        dict(name='help_after_unknown', args=['--unknown','--help'], no_calls=True, contains=['--summary']),
        dict(name='abbreviated_flags', args=['--sty','--sum'], no_calls=True, contains=['No .c files found to check']),
        dict(name='positional_after_option', args=['--style-only','src','--lenient','lib'], code=2, no_calls=True, contains=['unrecognized arguments: lib']),
        dict(name='multiple_unknowns', args=['--bad','--other'], code=2, no_calls=True, contains=['unrecognized arguments: --bad --other']),
        dict(name='unknown_flag', args=['--unknown'], code=2, no_calls=True, contains=['unrecognized arguments: --unknown']),
        dict(name='flag_value', args=['--summary=yes'], code=2, no_calls=True, contains=['ignored explicit argument']),
        dict(name='summary_warnings', args=['--style-only','--summary','--lenient','src'],
             files={'src/warnings.c':warnings}, contains=['Checked 1 files','0 errors, 2 warnings','All checks passed!'],
             absent=['missing_documentation()', 'no-tagged-union']),
        dict(name='strict_warnings', args=['--style-only','--summary','src'], code=1,
             files={'src/warnings.c':warnings}, contains=['1 errors, 1 warnings','Checks failed - fix issues before committing']),
        dict(name='empty_style', args=['--style-only'], no_calls=True,
             contains=['No .c files found to check','All checks passed!']),
        dict(name='dash_path', args=['--style-only','--','--literal.c'], no_calls=True,
             files={'--literal.c':''}, contains=['Checked 1 files','All files pass FERN_STYLE checks']),
    ]



def default_diagnostics(native, parent):
    """Retain the internal diagnostic hook and Python's src-then-lib default scan order."""
    directory = parent / 'default-diagnostics'; directory.mkdir()
    source = (ROOT/'tests/style_fixtures/nested/warnings.c').read_text()
    environment = setup(directory, {'files':{'src/first.c':source, 'lib/second.c':source}})
    code, output, calls = execute([str(native),'--style-only','--diagnostics'], directory, environment)[:3]
    records = [tuple(line.split('\t')[1:]) for line in output.splitlines() if line.startswith('DIAG\t')]
    previous = Path.cwd()
    try:
        os.chdir(directory)
        expected = [(str(v.file),str(v.line),v.function,v.rule,v.message,v.severity)
                    for file in check_style.find_c_files(['src','lib'])
                    for v in check_style.check_file(file)]
    finally:
        os.chdir(previous)
    assert code == 1 and not calls and Counter(records) == Counter(expected), (code, output)
    assert list(dict.fromkeys(record[0] for record in records)) == ['src/first.c','lib/second.c'], records
    print('  PASS default diagnostics and path order')



def closed_diagnostic(native):
    """A failed stderr write preserves the actual CLI error and produces no stdout."""
    launch = 'import os,sys; os.close(2); os.execv(sys.argv[1], sys.argv[1:])'
    result = subprocess.run([sys.executable, '-c', launch, str(native), '--unknown'],
                            text=True, capture_output=True, timeout=5)
    assert (result.returncode, result.stdout, result.stderr) == (2, '', ''), result
    print('  PASS closed stderr preserves CLI exit 2')


def unicode_numeric_gap(native, parent):
    """Keep the remaining Python Unicode-decimal classification gap visible until fixed."""
    for label, command, expected in [('python', [sys.executable, str(REFERENCE)], 0),
                                     ('native', [str(native)], 2)]:
        directory = parent / ('unicode-numeric-' + label); directory.mkdir()
        environment = setup(directory, {})
        result = execute(command + ['--style-only', '-١'], directory, environment)
        assert result[0] == expected, ('update the documented Unicode CLI gap', label, result)
        terminated = execute(command + ['--style-only', '--', '-١'], directory, environment)
        assert terminated[0] == 0 and not terminated[2], (label, terminated)
    print('  KNOWN GAP Unicode numeric paths require -- in native CLI')


def main():
    """Build an isolated checker or verify a supplied binary without switching defaults."""
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--native', type=Path)
    group.add_argument('--compiler', type=Path, default=ROOT/'bin/fern')
    parser.add_argument('--case', help='Run one named regression')
    args = parser.parse_args()
    native = args.native.resolve() if args.native else None
    cases = build_cases()+example_cases()+git_cases()+cli_cases()
    failures=[]
    with tempfile.TemporaryDirectory(prefix='fern-style-workflow-') as temporary:
        if native is None:
            native = Path(temporary)/'checker'
            built = subprocess.run([str(args.compiler.resolve()), 'build', '-o', str(native),
                                    str(ROOT/'scripts/check_style.fn')], cwd=ROOT,
                                   text=True, capture_output=True, timeout=120)
            assert built.returncode == 0, built
        for case in cases:
            if args.case and case['name'] != args.case: continue
            try: compare(native, case, Path(temporary))
            except AssertionError as error:
                failures.append(case['name']); print('  FAIL', error)
        if not args.case:
            try: default_diagnostics(native, Path(temporary))
            except AssertionError as error:
                failures.append('default_diagnostics'); print('  FAIL', error)
            try:
                closed_diagnostic(native)
                unicode_numeric_gap(native, Path(temporary))
            except AssertionError as error:
                failures.append('CLI stream/classification'); print('  FAIL', error)
    assert not failures, f'Workflow parity failed: {failures}'
    print(f'Style workflow parity: {len(cases)+2 if not args.case else 1} cases passed')

if __name__ == '__main__':
    main()
