#!/usr/bin/env python3
"""Task migration preserves every gate, sequencing and literal native invocation."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import tomllib
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class MiseWorkflow(unittest.TestCase):
    def config(self):
        return tomllib.loads((ROOT / 'mise.toml').read_text())

    def test_existing_task_and_gate_inventory_is_preserved(self):
        config = self.config()
        contract = json.loads((ROOT / 'tests/fixtures/mise_task_contract.json').read_text())
        self.assertTrue(set(contract['tasks']).issubset(config['tasks']))
        for name, commands in contract['ordered_gates'].items():
            body = config['tasks'][name]['run']
            if isinstance(body, list):
                body = '\n'.join(body)
            position = 0
            for command in commands:
                command = command.replace('just ', 'mise run ').replace('uv run scripts/', 'uv run --locked scripts/')
                if command.startswith('cargo fmt '):
                    continue  # Baseline Rust formatting is now an explicit reusable task.
                index = body.find(command, position)
                self.assertGreaterEqual(index, position, (name, command))
                position = index + len(command)

    def test_release_prerequisites_are_serial_and_native_config_is_shared(self):
        config = self.config()
        release = config['tasks']['release']
        self.assertEqual(release.get('depends', []), [])
        self.assertLess(release['run'].index('mise run clean'),
                        release['run'].index('_build-fern release'))
        self.assertIn('scripts/build_config', (ROOT / 'scripts/bootstrap/style_build').read_text())
        self.assertNotIn('Justfile', (ROOT / 'scripts/bootstrap/style_inputs.sh').read_text())

    def test_mise_versions_and_msrv_are_exact(self):
        config = self.config()
        self.assertEqual(config['tools']['rust']['version'], '1.75.0')
        self.assertEqual(set(config['tools']['rust']['components']), {'rustfmt', 'clippy', 'rust-src'})
        self.assertEqual(config['tools']['python'], '3.14.7')
        self.assertEqual(config['tools']['uv'], '0.12.5')
        for name in ('rust-fmt', 'rust-compile-check', 'rust-clippy', 'rust-test', 'rust-doc-test'):
            self.assertIn(name, config['tasks'])

    def test_bacon_build_toolchain_is_separate_from_checked_project(self):
        config = self.config()
        install = config['tasks']['rust-bacon-install']
        self.assertEqual(install['tools']['rust'], '1.98.1')
        self.assertIn('cargo install --locked --version 3.25.0', install['run'])
        self.assertIn('--root compiler-rs/target/dev-tools/bacon-3.25.0', install['run'])
        self.assertNotIn('depends', config['tasks']['rust-bacon'])
        jobs = tomllib.loads((ROOT / 'compiler-rs/bacon.toml').read_text())['jobs']
        for job in jobs.values():
            self.assertEqual(job['env']['RUSTUP_TOOLCHAIN'], '1.75.0')
            self.assertIn('--locked', job['command'])
        self.assertEqual(config['tools']['rust']['version'], '1.75.0')

    def test_zed_component_tasks_keep_their_independent_compiler(self):
        config = self.config()
        for task in ('zed-extension-check', 'zed-package'):
            self.assertIn('RUSTUP_TOOLCHAIN=1.97.1 python3 ', config['tasks'][task]['run'])

    def test_dependency_jobs_are_serial_in_the_real_runner(self):
        with tempfile.TemporaryDirectory(prefix='fern-mise-order-') as temp:
            work = Path(temp)
            work.joinpath('mise.toml').write_text(
                '[settings]\njobs=1\n[tasks.a]\nrun="sh step a"\n'
                '[tasks.b]\nrun="sh step b"\n[tasks.all]\ndepends=["a","b"]\n')
            work.joinpath('step').write_text(
                'echo "start $1" >> events\nsleep .1\necho "end $1" >> events\n')
            env = dict(os.environ, MISE_TRUSTED_CONFIG_PATHS=temp, MISE_AUTO_INSTALL='0')
            run = subprocess.run(['mise', '-C', temp, 'run', 'all'], env=env,
                                 text=True, capture_output=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stderr)
            events = work.joinpath('events').read_text().splitlines()
            self.assertIn(events, [ ['start a','end a','start b','end b'],
                                   ['start b','end b','start a','end a'] ])

    def test_binary_tool_locks_cover_both_supported_os_architectures(self):
        lock = tomllib.loads((ROOT / 'mise.lock').read_text())
        self.assertTrue(self.config()['tool_config']['locked'])
        platforms = {'linux-x64', 'linux-arm64', 'macos-x64', 'macos-arm64'}
        for tool in ('python', 'uv', 'watchexec', 'aqua:nextest-rs/nextest/cargo-nextest'):
            entries = lock['tools'][tool][0]
            for platform in platforms:
                record = entries['platforms.' + platform]
                self.assertTrue(record['url'].startswith('https://'))
                self.assertRegex(record['checksum'], r'^sha256:[a-f0-9]{64}$')

    def test_build_scripts_consume_shared_source_configuration_literally(self):
        with tempfile.TemporaryDirectory(prefix='fern-mise-sources-') as temp:
            work = Path(temp)
            (work / 'scripts').mkdir()
            for directory in ('chosen', 'support', 'backend'):
                (work / directory).mkdir()
                (work / directory / 'space name.c').touch()
            compiler = work / 'cc'
            compiler.write_text('#!/bin/sh\nprintf "%s\\n" "$@" >> "$BUILD_ARGS"\n')
            compiler.chmod(0o700)
            (work / 'scripts/build_config').write_text(
                '#!/bin/sh\ncase "$1" in\n'
                f'cc) echo "{compiler}";;\n'
                "src_sources) echo 'chosen/*.c';;\nlib_sources) echo 'support/*.c';;\n"
                "qbe_sources) echo 'backend/*.c';;\n*) echo '-DTEST';;\nesac\n")
            env = dict(os.environ, BUILD_ARGS=str(work / 'arguments'))
            run = subprocess.run(['bash', str(ROOT / 'scripts/tasks/build-fern'), 'debug'],
                                 cwd=work, env=env, capture_output=True, text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stderr)
            args = (work / 'arguments').read_text().splitlines()
            self.assertIn('chosen/space name.c', args)
            self.assertIn('support/space name.c', args)
            self.assertIn('backend/space name.c', args)

    def test_version_verification_rejects_a_mismatched_cargo(self):
        import check_tool_versions
        versions = {'rustc': 'rustc 1.75.0', 'cargo': 'cargo 1.98.1',
                    'python3': 'Python 3.14.7', 'uv': 'uv 0.12.5',
                    'mise': '2026.9.1'}
        with patch.object(check_tool_versions, 'output',
                          side_effect=lambda argv: versions.get(argv[0], 'native tool')):
            with self.assertRaisesRegex(RuntimeError, 'cargo: expected 1.75.0'):
                check_tool_versions.verify()

    def test_native_recipe_failure_stops_before_python_or_later_gates(self):
        self.config()
        with tempfile.TemporaryDirectory(prefix='fern-mise-dispatch-') as temp:
            work = Path(temp)
            shutil.copy2(ROOT / 'mise.toml', work / 'mise.toml')
            (work / 'scripts').mkdir()
            script = work / 'scripts/check_style'
            script.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$ARGS_FILE"\nexit 7\n')
            script.chmod(0o700)
            env = dict(os.environ, MISE_TRUSTED_CONFIG_PATHS=temp, MISE_TASK_OUTPUT='interleave',
                       MISE_AUTO_INSTALL='0', ARGS_FILE=str(work / 'args'))
            run = subprocess.run(['mise', '-C', temp, 'run', 'check'], env=env,
                                 text=True, capture_output=True, timeout=30)
            self.assertEqual(run.returncode, 7, run.stdout + run.stderr)
            self.assertEqual((work / 'args').read_text().splitlines(), ['src', 'lib'])


if __name__ == '__main__':
    unittest.main()
