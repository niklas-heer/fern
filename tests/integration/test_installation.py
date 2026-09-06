"""Exercise installed compiler workflows from outside the checkout."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class InstallationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fern-install-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.bundle = self.directory / "Fern's tools $bundle"
        self.bundle.mkdir()
        for name in ("fern", "libfern_runtime.a"):
            shutil.copy2(ROOT / "bin" / name, self.bundle / name)
        self.source = self.directory / "hello world.fn"
        self.source.write_text('fn main():\n    println("Hello, Fern!")\n')
        self.env = dict(os.environ, PATH=str(self.bundle) + os.pathsep + os.environ["PATH"])
        self.env.pop("LIBRARY_PATH", None)

    def run_fern(self, executable, *args):
        return subprocess.run([str(executable), *map(str, args)], cwd=self.directory,
                              env=self.env, text=True, capture_output=True, timeout=30)

    def assert_success(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_bundle_on_path_builds_output_with_shell_characters(self):
        output = self.directory / "my 'program' $output"
        self.assert_success(self.run_fern("fern", "build", self.source, "-o", output))
        result = self.run_fern(output)
        self.assert_success(result)
        self.assertEqual(result.stdout, "Hello, Fern!\n")

    def test_leading_dash_output_is_a_filename(self):
        self.assert_success(self.run_fern("fern", "build", self.source, "-o", "-program"))
        result = self.run_fern(self.directory / "-program")
        self.assert_success(result)
        self.assertEqual(result.stdout, "Hello, Fern!\n")

    def test_symlinked_compiler_finds_bundle_runtime(self):
        link = self.directory / "fern-link"
        link.symlink_to(self.bundle / "fern")
        self.assert_success(self.run_fern(link, "run", self.source))

    def test_run_uses_private_temporary_paths(self):
        # The old basename-based path must never be overwritten or removed.
        name = "fern-protected-" + self.directory.name
        source = self.directory / (name + ".fn")
        source.write_text(self.source.read_text())
        protected = Path("/tmp") / ("fern_" + name)
        protected.write_text("user-owned sentinel")
        self.addCleanup(lambda: protected.unlink(missing_ok=True))
        self.assert_success(self.run_fern(self.bundle / "fern", "run", source))
        self.assertTrue(protected.exists(), "fern run deleted an unrelated file")
        self.assertEqual(protected.read_text(), "user-owned sentinel")

    def test_install_and_uninstall_custom_prefix(self):
        prefix = self.directory / "installed 'tools' $literal"
        env = dict(self.env, PREFIX=str(prefix))
        # Inspect the literal environment-based command; actual argv checks below pin the destination.
        dry = subprocess.run(["mise", "run", "--dry-run", "--skip-deps", "install"], cwd=ROOT,
                             env=env, text=True, capture_output=True, timeout=10)
        self.assertIn("${DESTDIR-}${PREFIX-/usr/local}/bin", dry.stdout + dry.stderr)
        subprocess.run(["mise", "run", "--skip-deps", "install"], cwd=ROOT, env=env,
                       check=True, capture_output=True, timeout=10)
        self.assertTrue((prefix / "bin/libfern_runtime.a").is_file())
        self.assert_success(self.run_fern(prefix / "bin/fern", "run", self.source))
        subprocess.run(["mise", "run", "uninstall"], cwd=ROOT, env=env,
                       check=True, capture_output=True, timeout=10)
        self.assertFalse((prefix / "bin/fern").exists())
        self.assertFalse((prefix / "bin/libfern_runtime.a").exists())


if __name__ == "__main__":
    unittest.main()
