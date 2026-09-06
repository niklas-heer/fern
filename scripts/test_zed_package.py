#!/usr/bin/env python3
"""Offline regression tests for registered, isolated Zed extension packages."""
import pathlib
import tempfile
import sys
import os
import signal
import subprocess
import shutil
import tarfile
from types import SimpleNamespace
from unittest.mock import patch
import unittest
import tomllib

ROOT = pathlib.Path(__file__).resolve().parents[1]
EXTENSION = ROOT / "editor/zed-fern"

class Registration(unittest.TestCase):
    def test_language_registers_a_scalar_grammar(self):
        config = tomllib.loads((EXTENSION / "languages/fern/config.toml").read_text())
        self.assertEqual(config["grammar"], "fern")

    def test_manifest_pins_monorepo_grammar(self):
        config = tomllib.loads((EXTENSION / "extension.toml").read_text())
        grammar = config["grammars"]["fern"]
        self.assertEqual(grammar["repository"], "https://github.com/niklas-heer/fern")
        self.assertEqual(grammar["path"], "editor/tree-sitter-fern")
        self.assertRegex(grammar["rev"], r"^[0-9a-f]{40}$")

    def test_separate_component_toolchain_is_pinned(self):
        config = tomllib.loads((EXTENSION / "rust-toolchain.toml").read_text())
        self.assertEqual(config["toolchain"]["channel"], "1.97.1")
        self.assertEqual(config["toolchain"]["targets"], ["wasm32-wasip2"])

    def test_installer_does_not_mutate_a_user_profile(self):
        source = (ROOT / "editor/install-zed-extension.sh").read_text()
        self.assertNotIn("rm -rf", source)
        self.assertNotIn("$HOME", source)
        self.assertIn("package_zed.py", source)

class PackageBoundaries(unittest.TestCase):
    def setUp(self):
        import package_zed
        self.package = package_zed

    def test_revision_must_be_a_complete_commit(self):
        for revision in ["main", "abc123", "a" * 39, "-" + "a" * 39]:
            with self.assertRaises(ValueError):
                self.package.revision(revision)
        self.assertEqual(self.package.revision("a" * 40), "a" * 40)

    def test_output_refuses_existing_paths_without_changes(self):
        with tempfile.TemporaryDirectory() as temporary:
            target = pathlib.Path(temporary) / "output"
            target.write_text("preserved")
            with self.assertRaises(ValueError):
                self.package.output_path(target)
            self.assertEqual(target.read_text(), "preserved")

    def test_process_arguments_are_literal(self):
        result = self.package.command([sys.executable, "-c", "import sys; print(sys.argv[1])",
                                       "$(not a shell); two words"], ROOT)
        self.assertEqual(result, b"$(not a shell); two words\n")

    def test_process_output_and_deadlines_are_bounded(self):
        with self.assertRaisesRegex(ValueError, "output limit"):
            self.package.command([sys.executable, "-c", "print('x'*10000)"], ROOT, limit=100)
        with self.assertRaisesRegex(ValueError, "deadline"):
            self.package.command([sys.executable, "-c", "import time; time.sleep(5)"],
                                 ROOT, timeout=0.1)

    def test_timeout_cleans_descendant_when_leader_already_exited(self):
        with tempfile.TemporaryDirectory() as temporary:
            pidfile = pathlib.Path(temporary) / "pid"
            code = ("import subprocess,sys,pathlib; p=subprocess.Popen([sys.executable,'-c',"
                    "'import time; time.sleep(30)']); pathlib.Path(sys.argv[1]).write_text(str(p.pid))")
            try:
                with self.assertRaisesRegex(ValueError, "deadline"):
                    self.package.command([sys.executable, "-c", code, pidfile], ROOT, timeout=0.3)
                pid = int(pidfile.read_text())
                status = subprocess.run(["ps", "-o", "stat=", "-p", str(pid)],
                                        capture_output=True, text=True, timeout=5).stdout.strip()
                self.assertTrue(not status or status.startswith("Z"), status)
            finally:
                if pidfile.exists():
                    try:
                        os.kill(int(pidfile.read_text()), signal.SIGTERM)
                    except ProcessLookupError:
                        pass

    def test_runtime_resources_use_registered_layout_only(self):
        with tempfile.TemporaryDirectory() as temporary:
            stage = pathlib.Path(temporary)
            self.package.copy_resources(EXTENSION, stage)
            config = tomllib.loads((stage / "extension.toml").read_text())
            self.assertEqual(config["languages"], ["languages/fern"])
            self.assertEqual(config["lib"], {"kind": "Rust", "version": "0.7.0"})
            self.assertFalse((stage / "languages/fern/fern.wasm").exists())
            self.assertFalse((stage / "target").exists())

    def test_failed_build_removes_partial_stage(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = pathlib.Path(temporary) / "package"
            args = SimpleNamespace(output=output, source=EXTENSION, wasm_tools="unused",
                                   grammar_repository=ROOT)
            with patch.object(self.package, "command", return_value=b"wasm-tools 1.258.0"), \
                    patch.object(self.package, "grammar_source", side_effect=ValueError("bad commit")):
                with self.assertRaisesRegex(ValueError, "bad commit"):
                    self.package.package(args)
            self.assertEqual(list(pathlib.Path(temporary).iterdir()), [])

    def test_archive_is_reproducible_and_has_no_absolute_members(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            stage = root / "stage"
            stage.mkdir()
            (stage / "extension.wasm").write_bytes(b"artifact")
            self.package.write_archive(stage, root / "one.tar.gz")
            os.utime(stage / "extension.wasm", (100, 100))
            self.package.write_archive(stage, root / "two.tar.gz")
            self.assertEqual((root / "one.tar.gz").read_bytes(), (root / "two.tar.gz").read_bytes())
            with tarfile.open(root / "one.tar.gz") as archive:
                self.assertEqual(archive.getnames(), ["extension.wasm"])
                self.assertEqual(archive.extractfile("extension.wasm").read(), b"artifact")

    def test_malformed_registration_is_rejected_before_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = pathlib.Path(temporary) / "extension"
            shutil.copytree(EXTENSION, source, ignore=shutil.ignore_patterns("target", "grammars"))
            config = source / "languages/fern/config.toml"
            config.write_text('name = "Fern"\ngrammar = "wrong"\n')
            with self.assertRaisesRegex(ValueError, "register"):
                self.package.configuration(source)

    def test_component_marker_inside_core_module_is_preserved(self):
        marker = b"zed:api-version"
        payload = bytes([len(marker)]) + marker + b"\0\0\0\x07\0\0"
        module = b"\0asm\x01\0\0\0" + b"\0" + bytes([len(payload)]) + payload
        component = b"\0asm\x0d\0\x01\0" + b"\x01" + bytes([len(module)]) + module
        self.assertEqual(self.package.api_marker(component), "0.7.0")
        with self.assertRaises(ValueError):
            self.package.api_marker(component + b"\0\x7f")

    def test_component_metadata_rejects_truncation_and_wrong_api(self):
        for data in [b"", b"\x00asm", b"\x00asm\x01\x00\x00\x00"]:
            with self.assertRaises(ValueError):
                self.package.api_marker(data)

if __name__ == "__main__":
    unittest.main()
