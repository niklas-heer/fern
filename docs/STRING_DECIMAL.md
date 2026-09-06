# Decimal text classification

`String.is_decimal(String) -> Bool` returns true when nonempty text contains only
Unicode 16.0.0 General_Category=Nd characters. Both native frontends and the Rust
REPL implement it; `str_is_decimal` is an intentional compatibility spelling.

| Input | Result |
| --- | --- |
| `"123"`, `"١２3"`, supplementary mathematical digits | true |
| Empty text | false |
| Signs, decimal points, whitespace, combining marks | false |
| Superscripts, fractions, circled digits, Roman numerals | false |

Mixed digit scripts are allowed. This is a text predicate, with no normalization,
locale, numeric conversion or numeric-literal syntax change.

Native content is limited to 16 MiB excluding its terminator. Oversize input
raises `string size limit exceeded` even when its first character is not a digit.
Rust direct and first-class calls use the normal fault/cleanup path; C retains its
existing runtime-fault convention. Empty or malformed native UTF-8 is false.
Foreign bytes beyond the first NUL are not observable through the CString ABI.

The REPL retains its stricter stored-String ceiling and charges one step per
64 bytes, with a minimum of one, before scanning. Its existing 100,000 normal and
10,000 cleanup step budgets are unchanged. Cleanup exhaustion preserves an earlier
fault. The predicate allocates nothing during native classification.

The primary [Unicode 16 category data](https://www.unicode.org/Public/16.0.0/ucd/extracted/DerivedGeneralCategory.txt)
is vendored with attribution and Unicode License V3 under `deps/unicode`.
Its 274,423 bytes have SHA256
`7676ab755a41ef82108460238569e60ad65c191ddafe61b36c6765ec1353f293`.
The 71 Nd ranges contain 760 scalars. Unicode 16 matches the explicitly pinned
Python 3.14 checker reference; upgrades require a compatibility decision.

`scripts/generate_decimal_tables.py` verifies the input hash, size and ranges,
then produces the C/Rust tables. Use `--check` for drift detection; never edit
the generated tables. Ordinary builds need neither Python nor network access.

Native debug/release/sanitizer tests compare every Unicode code point against
an independent primary-data oracle and exercise malformed UTF-8 and byte limits.
Source tests cover Bool ABI, callbacks, once-only effects, exact limits, controlled
faults and preserved outputs on rejection. REPL tests cover aggregate work charging
and cleanup after failure.
