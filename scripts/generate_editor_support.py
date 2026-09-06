#!/usr/bin/env python3
"""Render editor artifacts deterministically from explicitly authored templates.

The Rust parser and checked parity corpus define structural syntax. C token
metadata remains available for legacy token inventory; it cannot derive grammar
structure. Edit scripts/editor/*.in, then run this script (or --check for drift).
"""

import re
import sys
from pathlib import Path

# Project root (parent of scripts/)
PROJECT_ROOT = Path(__file__).parent.parent

# Source files
TOKEN_HEADER = PROJECT_ROOT / "include" / "token.h"

# Output files
TREE_SITTER_DIR = PROJECT_ROOT / "editor" / "tree-sitter-fern"
GRAMMAR_JS = TREE_SITTER_DIR / "grammar.js"
ZED_HIGHLIGHTS = (
    PROJECT_ROOT / "editor" / "zed-fern" / "languages" / "fern" / "highlights.scm"
)


def parse_tokens(header_path: Path) -> dict:
    """
    Parse token definitions from token.h.

    Returns a dict with categories:
    - keywords: list of keyword strings
    - operators: list of operator strings
    - literals: list of literal token names
    - delimiters: list of delimiter strings
    """
    content = header_path.read_text()

    tokens = {
        "keywords": [],
        "control_flow": [],  # if, else, match, for, return
        "storage": [],  # let, fn, type, newtype, trait, impl
        "visibility": [],  # pub
        "operators": [],
        "delimiters": [],
        "brackets": [],
        "builtin_types": [
            "Int",
            "Float",
            "String",
            "Bool",
            "List",
            "Map",
            "Result",
            "Option",
        ],
        "builtin_constants": ["true", "false"],
        "constructors": ["Ok", "Err", "Some", "None"],
    }

    # Map TOKEN_* to actual keyword/operator strings
    token_to_string = {
        # Keywords
        "TOKEN_LET": "let",
        "TOKEN_FN": "fn",
        "TOKEN_RETURN": "return",
        "TOKEN_IF": "if",
        "TOKEN_ELSE": "else",
        "TOKEN_MATCH": "match",
        "TOKEN_WITH": "with",
        "TOKEN_DO": "do",
        "TOKEN_DEFER": "defer",
        "TOKEN_PUB": "pub",
        "TOKEN_IMPORT": "import",
        "TOKEN_TYPE": "type",
        "TOKEN_TRAIT": "trait",
        "TOKEN_IMPL": "impl",
        "TOKEN_AND": "and",
        "TOKEN_OR": "or",
        "TOKEN_NOT": "not",
        "TOKEN_AS": "as",
        "TOKEN_MODULE": "module",
        "TOKEN_FOR": "for",
        "TOKEN_BREAK": "break",
        "TOKEN_CONTINUE": "continue",
        "TOKEN_IN": "in",
        "TOKEN_DERIVE": "derive",
        "TOKEN_WHERE": "where",
        "TOKEN_NEWTYPE": "newtype",
        "TOKEN_SPAWN": "spawn",
        "TOKEN_SEND": "send",
        "TOKEN_RECEIVE": "receive",
        "TOKEN_AFTER": "after",
        # Operators
        "TOKEN_PLUS": "+",
        "TOKEN_MINUS": "-",
        "TOKEN_STAR": "*",
        "TOKEN_SLASH": "/",
        "TOKEN_PERCENT": "%",
        "TOKEN_POWER": "**",
        "TOKEN_EQ": "==",
        "TOKEN_NE": "!=",
        "TOKEN_LT": "<",
        "TOKEN_LE": "<=",
        "TOKEN_GT": ">",
        "TOKEN_GE": ">=",
        "TOKEN_ASSIGN": "=",
        "TOKEN_BIND": "<-",
        "TOKEN_PIPE": "|>",
        "TOKEN_BAR": "|",
        "TOKEN_ARROW": "->",
        "TOKEN_FAT_ARROW": "=>",
        "TOKEN_QUESTION": "?",
        # Delimiters
        "TOKEN_COMMA": ",",
        "TOKEN_COLON": ":",
        "TOKEN_DOT": ".",
        "TOKEN_DOTDOT": "..",
        "TOKEN_DOTDOTEQ": "..=",
        "TOKEN_ELLIPSIS": "...",
        "TOKEN_UNDERSCORE": "_",
        "TOKEN_AT": "@",
        # Brackets
        "TOKEN_LPAREN": "(",
        "TOKEN_RPAREN": ")",
        "TOKEN_LBRACKET": "[",
        "TOKEN_RBRACKET": "]",
        "TOKEN_LBRACE": "{",
        "TOKEN_RBRACE": "}",
    }

    # Categorize keywords
    control_flow_tokens = {
        "TOKEN_IF",
        "TOKEN_ELSE",
        "TOKEN_MATCH",
        "TOKEN_FOR",
        "TOKEN_RETURN",
        "TOKEN_BREAK",
        "TOKEN_CONTINUE",
        "TOKEN_WITH",
    }
    storage_tokens = {
        "TOKEN_LET",
        "TOKEN_FN",
        "TOKEN_TYPE",
        "TOKEN_NEWTYPE",
        "TOKEN_TRAIT",
        "TOKEN_IMPL",
    }
    visibility_tokens = {"TOKEN_PUB"}

    # All keyword tokens (for categorization)
    keyword_tokens = {
        "TOKEN_LET",
        "TOKEN_FN",
        "TOKEN_RETURN",
        "TOKEN_IF",
        "TOKEN_ELSE",
        "TOKEN_MATCH",
        "TOKEN_WITH",
        "TOKEN_DO",
        "TOKEN_DEFER",
        "TOKEN_PUB",
        "TOKEN_IMPORT",
        "TOKEN_TYPE",
        "TOKEN_TRAIT",
        "TOKEN_IMPL",
        "TOKEN_AND",
        "TOKEN_OR",
        "TOKEN_NOT",
        "TOKEN_AS",
        "TOKEN_MODULE",
        "TOKEN_FOR",
        "TOKEN_BREAK",
        "TOKEN_CONTINUE",
        "TOKEN_IN",
        "TOKEN_DERIVE",
        "TOKEN_WHERE",
        "TOKEN_NEWTYPE",
        "TOKEN_SPAWN",
        "TOKEN_SEND",
        "TOKEN_RECEIVE",
        "TOKEN_AFTER",
    }
    bracket_tokens = {
        "TOKEN_LPAREN",
        "TOKEN_RPAREN",
        "TOKEN_LBRACKET",
        "TOKEN_RBRACKET",
        "TOKEN_LBRACE",
        "TOKEN_RBRACE",
    }
    delimiter_tokens = {
        "TOKEN_COMMA",
        "TOKEN_COLON",
        "TOKEN_DOT",
        "TOKEN_DOTDOT",
        "TOKEN_DOTDOTEQ",
        "TOKEN_ELLIPSIS",
        "TOKEN_AT",
    }
    operator_tokens = {
        "TOKEN_PLUS",
        "TOKEN_MINUS",
        "TOKEN_STAR",
        "TOKEN_SLASH",
        "TOKEN_PERCENT",
        "TOKEN_POWER",
        "TOKEN_EQ",
        "TOKEN_NE",
        "TOKEN_LT",
        "TOKEN_LE",
        "TOKEN_GT",
        "TOKEN_GE",
        "TOKEN_ASSIGN",
        "TOKEN_BIND",
        "TOKEN_PIPE",
        "TOKEN_BAR",
        "TOKEN_ARROW",
        "TOKEN_FAT_ARROW",
        "TOKEN_QUESTION",
    }

    # Find all TOKEN_* in the file and use the mapping
    token_names = re.findall(r"(TOKEN_\w+)", content)

    for token in set(token_names):  # Use set to avoid duplicates
        if token in token_to_string:
            string_val = token_to_string[token]

            # Categorize based on token sets
            if token in control_flow_tokens:
                tokens["control_flow"].append(string_val)
            if token in storage_tokens:
                tokens["storage"].append(string_val)
            if token in visibility_tokens:
                tokens["visibility"].append(string_val)
            if token in keyword_tokens:
                tokens["keywords"].append(string_val)
            elif token in bracket_tokens:
                tokens["brackets"].append(string_val)
            elif token in delimiter_tokens:
                tokens["delimiters"].append(string_val)
            elif token in operator_tokens:
                tokens["operators"].append(string_val)

    return tokens


def generate_zed_highlights(tokens: dict) -> str:
    """Render only query nodes/tokens that the authored grammar actually declares."""
    assert "keywords" in tokens
    return editor_template("highlights.scm.in")


def editor_template(name: str) -> str:
    """Load one fixed, bounded authored editor input without inspecting generated output."""
    source = PROJECT_ROOT / "scripts/editor" / name
    content = source.read_text()
    if len(content.encode()) > 256 * 1024:
        raise ValueError("editor template exceeds 256 KiB")
    return content


def generate_tree_sitter_grammar(tokens: dict) -> str:
    """Render the authored structural grammar; token metadata cannot infer indentation syntax."""
    assert "keywords" in tokens
    return editor_template("grammar.js.in")


def main(arguments=None):
    """Regenerate authored templates, or report stale outputs without writing with --check."""
    arguments = sys.argv[1:] if arguments is None else arguments
    if arguments not in ([], ["--check"]):
        print("Usage: generate_editor_support.py [--check]", file=sys.stderr)
        return 1
    if not TOKEN_HEADER.exists():
        print(f"Error: {TOKEN_HEADER} not found", file=sys.stderr)
        return 1
    tokens = parse_tokens(TOKEN_HEADER)
    outputs = {
        GRAMMAR_JS: generate_tree_sitter_grammar(tokens),
        ZED_HIGHLIGHTS: generate_zed_highlights(tokens),
        ZED_HIGHLIGHTS.with_name("outline.scm"): editor_template("outline.scm.in"),
    }
    stale = []
    for path, content in outputs.items():
        if arguments:
            if not path.exists() or path.read_text() != content:
                stale.append(path)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content)
            print(f"Generated {path}")
    for path in stale:
        print(f"Stale editor output: {path}", file=sys.stderr)
    if stale:
        return 1
    print("Editor generated inputs are current" if arguments else
          "Use pinned Tree-sitter 0.26.12 and mise run editor-support-compile for native/WASM validation")
    return 0


if __name__ == "__main__":
    sys.exit(main())
