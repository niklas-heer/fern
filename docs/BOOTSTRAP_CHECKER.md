# Fern-native quality checker

The native checker runs build, test, examples, style and advisory Git checks using
literal subprocess arguments with explicit time/output limits. Both C and Rust
can compile `scripts/check_style.fn`; `just style-fern` runs its full workflow.
The Python checker remains the default and compatibility reference. The workflow
CLI oracle pins Python 3.14 through its uv script metadata: argparse short-help
cluster handling differs in Python 3.11, so host Python must not silently select
different expected behavior. Normal Python checker execution remains available
under its existing supported versions. Native CLI behavior is independent of
the installed Python version.

`just style-parity` compares exact style diagnostic identities, severities,
messages, counts and exits on five fixtures and every compiler/library C source,
then checks the full command workflow. `just check` includes the workflow cases,
and `just rust-check` repeats diagnostic/workflow checks with the Rust compiler.

The 47 workflow cases cover build warnings/failures, test counts, sorted literal
example paths, bounded error details, missing tools/directories, advisory Git
checks, CLI flags/abbreviations, summary mode and src-before-lib discovery.
Argument failures write stderr and exit 2, including failure of the diagnostic
write itself; help writes stdout. All subprocess calls use the
[bounded process API](PROCESS_EXECUTION.md) with a 300-second timeout and separate
8 MiB stdout/stderr caps. Process failures produce portable error messages.
Normal tool output, diagnostic records and command ordering remain checked against
the Python reference; terminal decoration and OS-specific exception wording are
not byte-for-byte compatibility promises.

Default migration remains open. One tracked argument-parser difference is Python's
Unicode decimal-number classification: a path such as `-١` is positional in
Python but requires `-- -١` in the current native checker. The native numeric-path
recognizer currently covers ASCII negative integers/decimals. Further CLI parity
and a stable native launch recipe must pass before changing defaults.
