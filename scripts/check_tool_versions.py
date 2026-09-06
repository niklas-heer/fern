#!/usr/bin/env python3
"""Verify managed compiler/tool pins and report the host-native dependency versions."""
import json
from pathlib import Path
import re
import subprocess
import sys
import tomllib

ROOT = Path(__file__).resolve().parents[1]


def output(argv):
    """Bound every version probe and retain actionable diagnostics without invoking a shell."""
    result = subprocess.run(argv, text=True, capture_output=True, timeout=15)
    if result.returncode:
        raise RuntimeError(f'{argv[0]} version probe failed: {result.stderr.strip()}')
    return result.stdout.strip()


def verify_rust(toolchain):
    """Require active component identities to match the installed dated pin, including Cargo."""
    report = {}
    for tool in ('rustc', 'cargo', 'rustfmt', 'cargo-clippy'):
        arguments = ['--version', '--verbose'] if tool in ('rustc', 'cargo') else ['--version']
        actual = output([tool, *arguments])
        expected = output(['rustup', 'run', toolchain, tool, *arguments])
        if actual != expected:
            raise RuntimeError(f'{tool}: expected {toolchain} identity {expected!r}, got {actual!r}; run mise install')
        report[tool] = actual
    components = output(['rustup', 'component', 'list', '--toolchain',
                         toolchain, '--installed']).splitlines()
    if 'rust-src' not in components:
        raise RuntimeError(f'rust-src is required for {toolchain}; run mise install')
    report['rust-src-installed'] = True
    return report


def verify():
    """Managed pins are exact; native packages are explicitly reported as host-owned inputs."""
    config = tomllib.loads((ROOT / 'mise.toml').read_text())
    tools = config['tools']
    probes = [('python', ['python3', '--version'], tools['python']),
              ('uv', ['uv', '--version'], tools['uv'])]
    report = verify_rust(tools['rust']['version'])
    for name, command, expected in probes:
        actual = output(command)
        if not re.search(r'(?<![\d.])' + re.escape(expected) + r'(?![\d.])', actual):
            raise RuntimeError(f'{name}: expected {expected}, got {actual}; run mise install')
        report[name] = actual
    version = output(['mise', '--version'])
    match = re.match(r'(\d+)\.(\d+)\.(\d+)', version)
    minimum = tuple(map(int, config['min_version'].split('.')))
    if not match or tuple(map(int, match.groups())) < minimum:
        raise RuntimeError(f'mise {config["min_version"]} or newer is required; got {version}')
    report['mise'] = version
    for tool in ('clang', 'pkg-config'):
        report[tool] = output([tool, '--version'])
    for package in ('bdw-gc', 'sqlite3', 'openssl'):
        report[package] = output(['pkg-config', '--modversion', package])
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    try:
        verify()
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f'tool verification failed: {error}', file=sys.stderr)
        raise SystemExit(1)
