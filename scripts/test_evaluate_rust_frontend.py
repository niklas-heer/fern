#!/usr/bin/env python3
"""Regression tests for trustworthy Rust migration evaluation reports."""
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from evaluate_rust_frontend import verify_emission

SCRIPT = Path(__file__).with_name("evaluate_rust_frontend.py")


class EvaluationFailures(unittest.TestCase):
    """A failed compiler or wrong executable must never yield successful metrics."""

    def evaluate_fake(self, body: str) -> tuple[subprocess.CompletedProcess[str], dict]:
        """Run the CLI against isolated fake compilers and a preexisting report."""
        with tempfile.TemporaryDirectory(prefix="fern-evaluation-test-") as directory:
            root = Path(directory)
            compiler = root / "compiler"
            compiler.write_text(f"#!{sys.executable}\n" + body)
            compiler.chmod(0o700)
            output = root / "report.json"
            output.write_text('{"status": "success", "metrics": {"stale": true}}')
            result = subprocess.run(
                [sys.executable, str(SCRIPT), "--c-compiler", str(compiler),
                 "--rust-compiler", str(compiler), "--qbe", str(compiler),
                 "--samples", "1", "--build-samples", "1", "--output", str(output)],
                capture_output=True, text=True, timeout=15, check=False,
            )
            return result, json.loads(output.read_text())

    def test_compiler_failure_invalidates_stale_success_report(self) -> None:
        """A nonzero compiler exit is reported as failure, with no timing metrics."""
        result, report = self.evaluate_fake("import sys\nprint('compiler broke', file=sys.stderr)\nsys.exit(7)\n")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(report["status"], "failed")
        self.assertNotIn("metrics", report)
        self.assertIn("compiler broke", report["error"])

    def test_successful_compile_with_wrong_output_is_failure(self) -> None:
        """Native output, not merely compiler exit status, must match the fixture."""
        body = (
            "import pathlib, sys\n"
            "if len(sys.argv) > 1 and sys.argv[1] == 'emit':\n"
            "    print('export function w $main() {\\n@start\\nret 0\\n}')\n"
            "elif '-o' in sys.argv:\n"
            "    output = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])\n"
            "    output.write_text('#!' + sys.executable + '\\nprint(\\\"wrong\\\")\\n')\n"
            "    output.chmod(0o700)\n"
        )
        result, report = self.evaluate_fake(body)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(report["status"], "failed")
        self.assertNotIn("metrics", report)
        self.assertIn("stdout", report["error"])


    def test_complete_success_records_each_verified_sample(self) -> None:
        """Only verified samples appear in successful reports, with separate QBE size."""
        body = (
            "import pathlib, sys\n"
            "if len(sys.argv) > 1 and sys.argv[1] == 'emit':\n"
            "    print('export function w $main() {\\n@start\\nret 0\\n}')\n"
            "elif '-o' in sys.argv:\n"
            "    output = pathlib.Path(sys.argv[sys.argv.index('-o') + 1])\n"
            "    output.write_text('#!' + sys.executable + '\\nprint(100)\\n')\n"
            "    output.chmod(0o700)\n"
        )
        result, report = self.evaluate_fake(body)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(report["status"], "success")
        self.assertGreater(report["artifacts"]["rust_qbe_sidecar"]["bytes"], 0)
        self.assertIsInstance(report["metadata"]["git_dirty"], bool)
        for label in ["c", "rust"]:
            metrics = report["metrics"][label]
            self.assertEqual(metrics["native_verified"], 1)
            for action in ["check", "emit", "native_build"]:
                self.assertEqual(len(metrics[action]["samples_ms"]), 1)
                self.assertGreater(metrics[action]["median_ms"], 0)


    def test_accepts_the_real_fern_runtime_entrypoint(self) -> None:
        """Both actual emitters provide fern_main for the C runtime entry wrapper."""
        verify_emission("export function w $fern_main() {\n@start\nret 0\n}", "rust")


if __name__ == "__main__":
    unittest.main()
