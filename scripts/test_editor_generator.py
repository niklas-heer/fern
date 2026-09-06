#!/usr/bin/env python3
"""Check deterministic editor generation before any native tool is required."""
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch
import tempfile
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("generator", ROOT / "scripts/generate_editor_support.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)

class GeneratorTests(unittest.TestCase):
    def test_grammar_preserves_scanner_and_current_declaration_syntax(self):
        tokens = generator.parse_tokens(generator.TOKEN_HEADER)
        grammar = generator.generate_tree_sitter_grammar(tokens)
        self.assertIn("externals:", grammar)
        self.assertIn("newtype_definition:", grammar)
        self.assertIn("type_alias:", grammar)
        self.assertEqual(grammar, generator.generate_tree_sitter_grammar(tokens))

    def test_checked_outputs_are_reproducible_from_authored_inputs(self):
        tokens = generator.parse_tokens(generator.TOKEN_HEADER)
        self.assertEqual(generator.GRAMMAR_JS.read_text(), generator.generate_tree_sitter_grammar(tokens))
        self.assertEqual(generator.ZED_HIGHLIGHTS.read_text(), generator.generate_zed_highlights(tokens))
        outline = generator.ZED_HIGHLIGHTS.with_name("outline.scm")
        self.assertEqual(outline.read_text(), generator.editor_template("outline.scm.in"))

    def test_check_reports_drift_without_rewriting_it(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "grammar.js"
            path.write_text("stale generated content")
            with patch.object(generator, "GRAMMAR_JS", path), patch.object(
                generator, "ZED_HIGHLIGHTS", Path(temporary) / "highlights.scm"
            ):
                self.assertEqual(generator.main(["--check"]), 1)
                self.assertEqual(path.read_text(), "stale generated content")
                self.assertEqual(generator.main([]), 0)
                self.assertEqual(generator.main(["--check"]), 0)

if __name__ == "__main__":
    unittest.main()
