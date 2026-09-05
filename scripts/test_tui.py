#!/usr/bin/env python3
"""Deterministic TUI contracts, including real terminal editing through a PTY."""
import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import shlex
import struct
import subprocess
import tempfile
import termios
import time
import unittest

ROOT = Path(__file__).resolve().parent.parent


class TuiContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="fern-tui-")
        cls.binary = str(Path(cls.tmp.name) / "tui")
        flags = subprocess.check_output(
            ["pkg-config", "--libs", "bdw-gc", "sqlite3", "openssl"], text=True
        )
        subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Iruntime",
            "tests/fixtures/tui_runtime.c", "bin/libfern_runtime.a",
            *shlex.split(flags), "-pthread", "-o", cls.binary,
        ], cwd=ROOT, check=True)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_pipe(self, mode, text=""):
        return subprocess.check_output([self.binary, mode], input=text, text=True, timeout=5)

    def run_terminal(self, mode, keys=b"", prompt=b"", terminal="xterm"):
        master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 120, 0, 0))
        original = termios.tcgetattr(slave)
        process = subprocess.Popen([self.binary, mode], stdin=slave, stdout=slave,
                                   stderr=slave, env={**os.environ, "TERM": terminal})
        output = bytearray()
        sent = not prompt
        deadline = time.monotonic() + 5
        try:
            while time.monotonic() < deadline:
                if not select.select([master], [], [], 0.05)[0]:
                    if process.poll() is not None:
                        break
                    continue
                try:
                    chunk = os.read(master, 65536)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                if not chunk:
                    break
                output.extend(chunk)
                if not sent and prompt in output:
                    os.write(master, keys)
                    sent = True
            self.assertEqual(process.wait(timeout=1), 0, bytes(output))
            self.assertEqual(termios.tcgetattr(slave), original, "terminal mode leaked")
            return bytes(output)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            os.close(master)
            os.close(slave)

    def test_fern_example(self):
        binary = str(Path(self.tmp.name) / "project")
        subprocess.run([str(ROOT / "bin/fern"), "build", "examples/tui_project.fn",
                        "-o", binary], cwd=ROOT, check=True, capture_output=True)
        self.assertEqual(subprocess.check_output([binary], text=True, timeout=5),
            "[INFO] Project ready\nfern-app\n├── src\n│   └── main.fn\n└── README.md\n"
            "[DEBUG] 2 source entries\n[WARN] Remember to add tests\n"
            "[ERROR] Example error message\n")

    def test_fern_rejects_invalid_tui_arguments(self):
        source = Path(self.tmp.name) / "invalid.fn"
        for expression in ['Tui.Tree.add(Tui.Tree.new("x"), "oops")',
                           'Tui.Log.info(42)', 'Tui.Term.move_to("row", 1)']:
            source.write_text(f"fn main():\n    {expression}\n    0\n")
            result = subprocess.run([str(ROOT / "bin/fern"), "check", str(source)],
                                    cwd=ROOT, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0, expression)
            self.assertNotIn("Undefined variable: Tui", result.stderr)

    def test_tree_rendering_and_immutability(self):
        self.assertEqual(self.run_pipe("tree"),
            "project\n├── src\n│   └── main.fn\n└── README.md\n"
            "project\nsrc\n└── main.fn\n\n└── a\n    b\n")

    def test_logs_are_deterministic_and_escape_terminal_controls(self):
        self.assertEqual(self.run_pipe("log"),
            "[DEBUG] details\n[INFO] ready\n[WARN] careful\\nsecond line\n"
            "[ERROR] bad\\x1b[2J\\rspoof\n")

    def test_piped_input_and_password(self):
        self.assertEqual(self.run_pipe("input", "hello\n"), "name> RESULT:hello\n")
        self.assertEqual(self.run_pipe("input", ""), "name> RESULT:\n")
        self.assertEqual(self.run_pipe("password", "secret\n"), "secret> RESULT:secret\n")

    def test_cursor_operations_are_silent_in_pipes(self):
        self.assertEqual(self.run_pipe("cursor"), "DONE\n")

    def test_terminal_cursor_sequences(self):
        self.assertEqual(self.run_terminal("cursor"),
            b"\x1b[2;3H\x1b[1A\x1b[2B\x1b[3D\x1b[4C"
            b"\x1b[?25l\x1b[?25h\x1b[s\x1b[u\x1b[2J\x1b[HDONE\r\n")

    def test_prompt_cursor_editing(self):
        out = self.run_terminal("input", b"ac\x1b[Db\x01X\x05!\r", b"name> ")
        self.assertIn(b"RESULT:Xabc!\r\n", out)

    def test_prompt_delete_and_unicode(self):
        out = self.run_terminal("input", "aéZ".encode() + b"\x1b[D\x7f\x1b[3~b\r", b"name> ")
        self.assertIn(b"RESULT:ab\r\n", out)

    def test_password_does_not_echo_and_restores_terminal(self):
        out = self.run_terminal("password", b"never-echo-this\r", b"secret> ")
        self.assertEqual(out.count(b"never-echo-this"), 1)
        self.assertIn(b"RESULT:never-echo-this\r\n", out)

    def test_password_dumb_terminal_does_not_echo(self):
        out = self.run_terminal("password", b"private-secret\r", b"secret> ", "dumb")
        self.assertEqual(out.count(b"private-secret"), 1)
        self.assertIn(b"RESULT:private-secret\r\n", out)

    def test_prompt_cancel_restores_terminal(self):
        out = self.run_terminal("input", b"discard\x03", b"name> ")
        self.assertIn(b"RESULT:\r\n", out)

    def test_prompt_eof_restores_terminal(self):
        out = self.run_terminal("input", b"\x04", b"name> ")
        self.assertIn(b"RESULT:\r\n", out)


if __name__ == "__main__":
    unittest.main(verbosity=2)
