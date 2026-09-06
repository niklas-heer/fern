#!/usr/bin/env python3
"""Verify offline Unicode provenance, corruption rejection and table drift."""
import tempfile
import unittest
from pathlib import Path
import generate_decimal_tables as generator


class DecimalGenerator(unittest.TestCase):
    def test_primary_inventory_and_outputs(self):
        rendered = generator.render()
        self.assertEqual(len(rendered), 2)
        for path, text in rendered.items():
            self.assertEqual(path.read_text(), text)

    def test_corrupted_input_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "ucd"
            path.write_bytes(generator.SOURCE.read_bytes() + b"# changed\n")
            with self.assertRaisesRegex(ValueError, "checksum"):
                generator.render(path)

    def test_bad_ranges_rejected(self):
        for text in ["D800 ; Nd", "110000 ; Nd", "0031..0030 ; Nd",
                     "0030..0039 ; Nd\n0030..0039 ; Nd", "0030..0039 ; Nd"]:
            with self.assertRaises(ValueError):
                generator.ranges(text)


if __name__ == "__main__":
    unittest.main()
