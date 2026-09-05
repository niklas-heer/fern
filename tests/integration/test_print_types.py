"""Native-output regressions for printing expression values with their Fern types."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PrintTypes(unittest.TestCase):
    def test_function_and_branch_results(self):
        source = '''fn greeting() -> String:
    "hello"
fn ready() -> Bool:
    true
fn main():
    println(greeting())
    println(ready())
    println(if true: "yes" else: "no")
    println(2 < 3)
    println(Regex.is_match("(", "[(]"))
'''
        with tempfile.TemporaryDirectory(prefix="fern-print-") as directory:
            path = Path(directory) / "print.fn"
            path.write_text(source)
            result = subprocess.run([str(ROOT / "bin/fern"), "run", str(path)],
                                    text=True, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stdout, "hello\ntrue\nyes\ntrue\ntrue\n")


if __name__ == "__main__":
    unittest.main()
