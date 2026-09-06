# Pinned decimal character data

Fern's `String.is_decimal` uses Unicode16.0.0 `General_Category=Nd`, matching the
explicit Python3.14 bootstrap reference. It intentionally does not use host Rust,
Python or locale character tables. Updating Unicode is a compatibility decision.

Primary input: [DerivedGeneralCategory.txt](https://www.unicode.org/Public/16.0.0/ucd/extracted/DerivedGeneralCategory.txt)
(274423 bytes), SHA256 `7676ab755a41ef82108460238569e60ad65c191ddafe61b36c6765ec1353f293`.
Copyright2024 Unicode, Inc.; distributed under [Unicode License V3](LICENSE.txt).
The full input retains its original copyright/attribution. Generated tables derive
71 intervals containing760 decimal scalar values. See
[UAX44 for Unicode16](https://www.unicode.org/reports/tr44/tr44-34.html).

Run `python3 scripts/generate_decimal_tables.py` offline to regenerate the C/Rust
outputs, or append `--check` to detect drift. Do not manually edit the generated
headers or Rust table. Normal C/Rust builds do not need Python or network access.
`python3 scripts/test_decimal_generator.py` verifies corruption rejection and
output identity. The native and Rust scalar tests independently parse this primary
file rather than using the generated interval array as their expected oracle.

The predicate is false for empty text, malformed native UTF8, signs, decimal
separators and nondecimal numeric characters such as superscripts and fractions.
It permits mixed-script digits without normalization. Native content above16MiB
raises the established string-size fault, preserving Fern cleanup in the Rust
backend through the internal size preflight. The legacy C frontend retains its
existing fatal runtime-fault convention; this does not promise C defer parity.
The REPL uses the same pinned classification and native ceiling alongside its
existing stricter interactive retention limits. CString terminators are part of
the native String ABI; this API cannot inspect foreign bytes after a NUL.

Interactive scanning reserves one existing evaluation step per64 input bytes
(at leastone) before work. The existing100000 normal steps and separate10000
cleanup steps remain bounded independently; repeated scans cannot evade either
budget. A failed reservation stays charged.
