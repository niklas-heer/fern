#!/usr/bin/env python3
"""Literal build arguments, using temporary mock tools and no native compilation."""
import os
from pathlib import Path
import random
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BuildFlags(unittest.TestCase):
    def decode(self, data):
        with tempfile.TemporaryDirectory(prefix="fern-flag-decode-") as temp:
            source = Path(temp) / "record"
            source.write_bytes(data)
            script = 'source "$1/scripts/tasks/build-arguments"; build_words_file "$2" || exit $?; if ((${#build_words[@]})); then printf "%s\\0" "${build_words[@]}"; fi'
            run = subprocess.run(["/bin/bash", "-euo", "pipefail", "-c", script, "test", str(ROOT), str(source)], capture_output=True, timeout=10)
            return run, run.stdout.split(b"\0")[:-1]

    def test_quotes_escapes_empty_and_literal_expansions(self):
        run, words = self.decode(b"-I\"include dir\" -DNAME='hello world' \"\" a\\ b '$HOME' '$(touch marker)' '*'\n")
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(words, [b"-Iinclude dir", b"-DNAME=hello world", b"", b"a b", b"$HOME", b"$(touch marker)", b"*"])

    def test_empty_record_has_no_arguments(self):
        run, words = self.decode(b"\n")
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(words, [])

    def test_exact_bounds_and_unicode_bytes(self):
        for data, count in [(b"x" * 16384, 1), (b"x " * 4096, 4096), (b" ".join([b"x" * 16383] * 4) + b"\n", 4), ('-I"données λ"'.encode(), 1)]:
            run, words = self.decode(data)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(len(words), count)
        self.assertEqual(words, ['-Idonnées λ'.encode()])

    def test_library_flags_are_literal_too(self):
        run, words = self.decode(b'-L"library dir" -Wl,-rpath,/literal\\ path -lssl\n')
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(words, [b'-Llibrary dir', b'-Wl,-rpath,/literal path', b'-lssl'])

    def test_backticks_and_metacharacters_do_not_expand(self):
        text = b"`touch${IFS}marker` ; & | > < $HOME ${PATH} * ? [a] ~ #"
        run, words = self.decode(text)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(words, text.split())

    def test_seeded_literal_word_roundtrips(self):
        rng = random.Random(106)
        alphabet = "ab ' \t\"\\$();*?[]`λ"
        for _ in range(128):
            expected = [''.join(rng.choice(alphabet) for _ in range(rng.randrange(30))) for _ in range(rng.randrange(8))]
            run, words = self.decode(' '.join(shlex.quote(word) for word in expected).encode())
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(words, [word.encode() for word in expected])

    def test_malformed_and_bounded_records_have_no_partial_output(self):
        for data in [b"good 'bad", b'good "bad', b"good bad\\", b"a\0b", b"a\rb", b"a\nb", b"x" * 65537, b"x" * 16385, b"x " * 4097]:
            with self.subTest(data=data[:20]):
                run, _ = self.decode(data)
                self.assertNotEqual(run.returncode, 0)
                self.assertEqual(run.stdout, b"")

    def test_all_native_helpers_preserve_authored_and_pkg_config_flags(self):
        for helper in ["build-fern", "build-runtime", "build-test-runner", "build-fuzz-runner", "build-rust-backend"]:
            for mode in ["debug", "release"]:
                with self.subTest(helper=helper, mode=mode):
                    self.helper(helper, mode)

    def test_every_helper_rejects_malformed_flags_before_compiling(self):
        for helper in ["build-fern", "build-runtime", "build-test-runner", "build-fuzz-runner", "build-rust-backend"]:
            self.helper(helper, "debug", malformed=True)

    def test_every_helper_accepts_empty_flag_records(self):
        for helper in ["build-fern", "build-runtime", "build-test-runner", "build-fuzz-runner", "build-rust-backend"]:
            self.helper(helper, "debug", empty=True)

    def helper(self, helper, mode, malformed=False, empty=False):
        with tempfile.TemporaryDirectory(prefix="fern-flag-helper-") as temp:
            work = Path(temp)
            shutil.copytree(ROOT / "scripts/tasks", work / "scripts/tasks")
            for directory in ["src", "lib", "runtime", "tests", "deps/qbe", "build"]:
                (work / directory).mkdir(parents=True, exist_ok=True)
                (work / directory / "space name.c").touch()
            (work / "build/qbe_mock.o").touch()
            tools = work / "mock-tools"
            tools.mkdir()
            (work / "temporary").mkdir()
            logger = '#!/bin/bash\nprintf "CALL\\0" >> "$BUILD_ARGS"\nprintf "%s\\0" "$@" >> "$BUILD_ARGS"\n'
            for tool in ["clang", "ar"]:
                (tools / tool).write_text(logger)
                (tools / tool).chmod(0o700)
            (tools / "pkg-config").write_text('#!/bin/sh\n[ "$1" = --exists ] && exit 0\nprintf \'%s\\n\' \'-Iexternal\\ path\'\n')
            (tools / "pkg-config").chmod(0o700)
            (work / "scripts/build_config").write_text('#!/bin/sh\ncase "$1" in\ncc) echo clang;;\nsrc_sources) echo "src/*.c";;\nlib_sources) echo "lib/*.c";;\nruntime_sources) echo "runtime/*.c";;\ntest_sources) echo "tests/*.c";;\nqbe_sources) echo "deps/qbe/*.c";;\n*) printf \'%s\\n\' \'-I"include dir" -DNAME="hello world" -DLITERAL=$(touch${IFS}marker)\';;\nesac\n')
            if malformed or empty:
                config = work / "scripts/build_config"
                flags = '-I"include dir" -DNAME="hello world" -DLITERAL=$(touch${IFS}marker)'
                config.write_text(config.read_text().replace(flags, '"bad' if malformed else ''))
            env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"], BUILD_ARGS=str(work / "args"), TMPDIR=str(work / "temporary"))
            args = ["bash", str(work / "scripts/tasks" / helper)] + ([] if helper == "build-rust-backend" else [mode])
            run = subprocess.run(args, cwd=work, env=env, capture_output=True, timeout=10)
            self.assertEqual(list((work / "temporary").iterdir()), [])
            if malformed:
                self.assertNotEqual(run.returncode, 0)
                self.assertFalse((work / "args").exists())
                return
            self.assertEqual(run.returncode, 0, run.stderr)
            words = (work / "args").read_bytes().split(b"\0")
            if not empty:
                self.assertIn(b"-Iinclude dir", words)
                self.assertIn(b"-DNAME=hello world", words)
                self.assertIn(b"-DLITERAL=$(touch${IFS}marker)", words)
            if helper == "build-runtime":
                self.assertIn(b"-Iexternal path", words)
            self.assertFalse((work / "marker").exists())


if __name__ == "__main__":
    unittest.main()
