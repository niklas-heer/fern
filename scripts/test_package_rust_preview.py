#!/usr/bin/env python3
"""Bounded preview packaging regressions; no compiler/native builds occur here."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("package_rust_preview.py")

class PreviewTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="fern preview ü $(literal) ")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        spec = importlib.util.spec_from_file_location("preview", SCRIPT)
        self.api = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.api)
        self.inputs = {}
        for name in self.api.PAYLOAD_NAMES:
            path = self.root / ("input " + name)
            path.write_bytes(("literal " + name + "\n").encode())
            path.chmod(0o755 if name in self.api.EXECUTABLES else 0o644)
            self.inputs[name] = path
        self.stage = self.root / "stage"

    def stage_it(self):
        return self.api.stage(self.inputs, self.stage, "0.1.0-preview.1", "macos", "arm64")

    def test_stage_exact_manifest_modes_and_determinism(self):
        self.stage_it()
        expected = set(self.api.PAYLOAD_NAMES) | {self.api.MANIFEST}
        self.assertEqual({p.name for p in self.stage.iterdir()}, expected)
        manifest = self.api.validate_stage(self.stage)
        for name, info in manifest["files"].items():
            data = (self.stage / name).read_bytes()
            self.assertEqual(info["sha256"], hashlib.sha256(data).hexdigest())
            self.assertEqual(info["size"], len(data))
        left, right = self.root / "left.tar.gz", self.root / "right.tar.gz"
        self.api.package(self.stage, left)
        self.api.package(self.stage, right)
        self.assertEqual(left.read_bytes(), right.read_bytes())
        self.assertEqual(self.api.verify(left), manifest)

    def test_invalid_stage_inputs_preserve_prior_output(self):
        self.stage_it()
        before = (self.stage / self.api.MANIFEST).read_bytes()
        for mutation in ["missing", "nonexec", "symlink", "empty", "oversized"]:
            with self.subTest(mutation=mutation):
                source = self.inputs["fern-qbe"]
                source.unlink(missing_ok=True)
                if mutation == "symlink": source.symlink_to(self.inputs["fern-rs"])
                elif mutation != "missing":
                    source.write_bytes(b"" if mutation == "empty" else b"qbe")
                    source.chmod(0o644 if mutation == "nonexec" else 0o755)
                    if mutation == "oversized":
                        with source.open("r+b") as stream: stream.truncate(self.api.FILE_LIMITS["fern-qbe"] + 1)
                with self.assertRaises((ValueError, OSError)): self.stage_it()
                self.assertEqual((self.stage / self.api.MANIFEST).read_bytes(), before)

    def test_new_stage_and_overlap_rejected(self):
        self.stage_it()
        with self.assertRaises(ValueError): self.stage_it()
        with self.assertRaises(ValueError):
            self.api.stage(self.inputs, self.root, "0.1.0", "macos", "arm64")
        alias = self.root / "alias"
        alias.symlink_to(self.stage, target_is_directory=True)
        with self.assertRaises(ValueError): self.api.stage(self.inputs, alias, "0.1.0", "macos", "arm64")

    def test_metadata_rejected_before_publication(self):
        for value in ["", "../x", "1.0.0/../../x", "x" * 100, "1.0.0\n"]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.api.stage(self.inputs, self.stage, value, "macos", "arm64")
            self.assertFalse(self.stage.exists())
        with self.assertRaises(ValueError): self.api.stage(self.inputs, self.stage, "0.1.0", "windows", "arm64")

    def test_archive_failure_preserves_prior_output(self):
        self.stage_it()
        output = self.root / "artifact.tar.gz"
        self.api.package(self.stage, output)
        before = output.read_bytes(), output.stat().st_mtime_ns
        (self.stage / "fern-rs").write_bytes(b"changed")
        with self.assertRaises(ValueError): self.api.package(self.stage, output)
        self.assertEqual((output.read_bytes(), output.stat().st_mtime_ns), before)

    def test_stage_extra_symlink_and_manifest_duplicate_rejected(self):
        self.stage_it()
        extra = self.stage / "extra"; extra.write_bytes(b"x")
        with self.assertRaises(ValueError): self.api.validate_stage(self.stage)
        extra.unlink()
        target = self.stage / "LICENSE"; target.unlink(); target.symlink_to(self.inputs["LICENSE"])
        with self.assertRaises(ValueError): self.api.validate_stage(self.stage)
        target.unlink(); target.write_bytes(self.inputs["LICENSE"].read_bytes())
        manifest = self.stage / self.api.MANIFEST
        manifest.write_text('{"format":1,"format":1}')
        with self.assertRaises(ValueError): self.api.validate_stage(self.stage)

    def raw_archive(self):
        import io
        self.stage_it()
        path = self.root / "canonical.tar.gz"
        self.api.package(self.stage, path)
        raw = io.BytesIO()
        with path.open("rb") as stream:
            self.api.archive.inflate(stream, raw, self.api.TAR_LIMIT)
        return raw.getvalue()

    def verify_raw(self, raw):
        import io
        path = self.root / "hostile.tar.gz"
        with path.open("wb") as output:
            self.api.archive.write_gzip(io.BytesIO(raw), output)
        return self.api.verify(path)

    def test_hostile_header_variants_and_membership(self):
        raw = self.raw_archive()
        first_size = int(raw[124:135], 8)
        first_end = 512 + first_size + self.api.archive.pad(first_size)
        cases = {}
        for label, start, value in [("link", 156, b"2"), ("hardlink", 156, b"1"),
                                     ("device", 156, b"3"), ("pax", 156, b"x"),
                                     ("longname", 156, b"L"), ("checksum", 148, b"999999"),
                                     ("owner", 108, b"0000001"), ("absolute", 0, b"/"),
                                     ("traversal", 0, b"../"), ("wrongroot", 0, b"other")]:
            changed = bytearray(raw); changed[start:start + len(value)] = value
            cases[label] = bytes(changed)
        cases["duplicate"] = raw[:first_end] + raw
        cases["missing"] = raw[:first_end] + bytes(1024)
        cases["extra"] = raw[:-1024] + raw[:first_end] + bytes(1024)
        cases["truncated"] = raw[:-1]
        cases["trailing"] = raw + b"x"
        cases["bad_mode"] = self.api.archive.header(raw[:100].split(b"\0")[0].decode(), first_size, 0o777) + raw[512:]
        cases["oversized_manifest"] = self.api.archive.header(raw[:100].split(b"\0")[0].decode(), 65537, 0o644) + raw[512:]
        for bad_name in ("/absolute/" + self.api.MANIFEST, "../" + self.api.MANIFEST,
                         "other/" + self.api.MANIFEST):
            cases[bad_name] = self.api.archive.header(bad_name, first_size, 0o644) + raw[512:]
        for label, changed in cases.items():
            with self.subTest(label=label), self.assertRaises(ValueError): self.verify_raw(changed)

    def test_digest_padding_and_manifest_shape(self):
        raw = self.raw_archive()
        size = int(raw[124:135], 8)
        end = 512 + size + self.api.archive.pad(size)
        changed = bytearray(raw); changed[end + 512] ^= 1
        with self.assertRaisesRegex(ValueError, "digest"): self.verify_raw(changed)
        changed = bytearray(raw); changed[512 + size] = 1
        with self.assertRaisesRegex(ValueError, "padding"): self.verify_raw(changed)
        manifest = self.api.validate_stage(self.stage)
        for value in [[], {}, {**manifest, "extra": 0}, {**manifest, "format": True},
                      {**manifest, "files": []}, {**manifest, "os": []}]:
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.api.parse_manifest(self.api.canonical(value))
        with self.assertRaises(ValueError): self.api.parse_manifest(b"{" * 65537)
        with self.assertRaises(ValueError): self.api.parse_manifest(b"[" * 2048 + b"]" * 2048)

    def test_gzip_member_tail_crc_and_shared_limit(self):
        import io
        from unittest import mock
        raw = self.raw_archive()
        valid = self.root / "canonical.tar.gz"
        data = valid.read_bytes()
        for changed in [data[:-1], data + b"x", data + data, data[:-8] + bytes(8), b"x" + data[1:]]:
            path = self.root / "gzip-bad"; path.write_bytes(changed)
            with self.assertRaises(ValueError): self.api.verify(path)
        with mock.patch.object(self.api, "TAR_LIMIT", len(raw) - 1):
            with self.assertRaisesRegex(ValueError, "decompressed"): self.api.verify(valid)
        with mock.patch.object(self.api, "TAR_LIMIT", len(raw)):
            self.api.verify(valid)
        with mock.patch.object(self.api, "ARCHIVE_LIMIT", len(data) - 1):
            with self.assertRaises(ValueError): self.api.verify(valid)

    def test_exact_file_and_aggregate_caps(self):
        from unittest import mock
        name = "LICENSE"; length = self.inputs[name].stat().st_size
        with mock.patch.dict(self.api.FILE_LIMITS, {name: length - 1}):
            with self.assertRaises(ValueError): self.stage_it()
        self.assertFalse(self.stage.exists())
        total = sum(path.stat().st_size for path in self.inputs.values())
        with mock.patch.object(self.api, "TOTAL_LIMIT", total - 1):
            with self.assertRaises(ValueError): self.stage_it()
        self.assertFalse(self.stage.exists())
        with mock.patch.dict(self.api.FILE_LIMITS, {name: length}): self.stage_it()

    def test_extract_new_directory_and_failed_publish(self):
        from unittest import mock
        self.stage_it()
        path = self.root / "archive.tar.gz"; self.api.package(self.stage, path)
        moved = self.root / "moved ü $([literal])"
        manifest = self.api.extract(path, moved)
        self.assertEqual(self.api.validate_stage(moved), manifest)
        with self.assertRaises(ValueError): self.api.extract(path, moved)
        before = path.read_bytes(), path.stat().st_mtime_ns
        with mock.patch.object(self.api.os, "replace", side_effect=OSError("injected publish failure")):
            with self.assertRaises(OSError): self.api.package(self.stage, path)
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)

    def test_each_input_failure_on_new_destination(self):
        for mutation in ("missing", "nonexec", "symlink", "empty", "oversized"):
            source = self.inputs["fern-qbe"]; source.unlink(missing_ok=True)
            if mutation == "symlink": source.symlink_to(self.inputs["fern-rs"])
            elif mutation != "missing":
                source.write_bytes(b"" if mutation == "empty" else b"qbe")
                source.chmod(0o644 if mutation == "nonexec" else 0o755)
                if mutation == "oversized":
                    with source.open("r+b") as stream: stream.truncate(self.api.FILE_LIMITS["fern-qbe"] + 1)
            with self.subTest(mutation=mutation), self.assertRaises((ValueError, OSError)): self.stage_it()
            self.assertFalse(self.stage.exists())

    def test_opened_archive_growth_remains_bounded(self):
        import io
        reader = self.api.archive.BoundedReader(io.BytesIO(b"12345"), 4)
        self.assertEqual(reader.read(3), b"123")
        with self.assertRaisesRegex(ValueError, "compressed.*limit"):
            reader.read(65536)

    def test_cli_literal_paths_and_invalid_arguments(self):
        import subprocess
        import sys
        command = [sys.executable, str(SCRIPT), "stage", "--compiler", str(self.inputs["fern-rs"]),
                   "--qbe", str(self.inputs["fern-qbe"]), "--supervisor", str(self.inputs["fern-test-supervisor"]),
                   "--runtime", str(self.inputs["libfern_runtime.a"]), "--license", str(self.inputs["LICENSE"]),
                   "--output", str(self.stage), "--version", "0.1.0-preview.1", "--os", "macos", "--arch", "arm64"]
        result = subprocess.run(command, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), self.api.validate_stage(self.stage))
        self.assertFalse((self.root / "literal").exists())
        result = subprocess.run([sys.executable, str(SCRIPT), "verify", "--unknown"], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, b"")

    def test_precharged_copy_size_cannot_grow_between_validation_and_copy(self):
        import io
        output = io.BytesIO()
        path = self.inputs["LICENSE"]
        with self.assertRaises(ValueError):
            self.api.transfer(path, "LICENSE", output, expected_size=path.stat().st_size - 1)
        self.assertEqual(output.getvalue(), b"")

    def test_reproducibility_ignores_source_metadata(self):
        import tarfile
        self.stage_it()
        one = self.root / "one.tar.gz"; self.api.package(self.stage, one)
        for name, path in self.inputs.items():
            path.chmod(0o700 if name in self.api.EXECUTABLES else 0o600)
            os.utime(path, (123, 456))
        self.stage = self.root / "second stage"
        self.stage_it()
        two = self.root / "two.tar.gz"; self.api.package(self.stage, two)
        self.assertEqual(one.read_bytes(), two.read_bytes())
        with tarfile.open(one, "r:gz") as tar:
            members = tar.getmembers()
            self.assertEqual(len(members), 7)
            self.assertTrue(all(member.isfile() and member.uid == member.gid == member.mtime == 0 for member in members))

    def test_mise_tasks_have_literal_argv_and_no_build_dependencies(self):
        import subprocess
        import tomllib
        config = tomllib.loads(SCRIPT.parents[1].joinpath("mise.toml").read_text())
        tasks = config["tasks"]
        environment = dict(os.environ)
        environment.update({"FERN_PREVIEW_COMPILER": str(self.inputs["fern-rs"]),
            "FERN_PREVIEW_QBE": str(self.inputs["fern-qbe"]),
            "FERN_PREVIEW_SUPERVISOR": str(self.inputs["fern-test-supervisor"]),
            "FERN_PREVIEW_RUNTIME": str(self.inputs["libfern_runtime.a"]),
            "FERN_PREVIEW_VERSION": "0.1.0-preview.1", "FERN_PREVIEW_OS": "macos",
            "FERN_PREVIEW_ARCH": "arm64", "FERN_PREVIEW_STAGE": str(self.stage),
            "FERN_PREVIEW_ARCHIVE": str(self.root / "bundle ü $(literal).tar.gz")})
        for name in ("stage", "package", "verify"):
            task = tasks["rust-preview-" + name]
            self.assertNotIn("depends", task)
            result = subprocess.run(["/bin/sh", "-eu", "-c", task["run"]], env=environment,
                                    cwd=SCRIPT.parents[1], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.api.verify(Path(environment["FERN_PREVIEW_ARCHIVE"])), self.api.validate_stage(self.stage))

if __name__ == "__main__": unittest.main()
