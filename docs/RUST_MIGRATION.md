# Incremental Rust migration

The Rust frontend now supports immutable lists and built-in error/optional values
from source parsing through native execution. The shipping `fern` compiler stays
C while language and tooling parity are developed. The original
[evaluation](RUST_FRONTEND_EVALUATION.md) is a snapshot of the initial scalar
prototype; its measurements do not describe the expanded compiler.

## Collections and error values — 2026-09-05

- Recursive concrete `List(T)`, `Option(T)`, and `Result(T, E)` types, including
  nested combinations and Unit payloads.
- Local constructor/empty-list inference with concrete type resolution before
  lowering. Underconstrained values require annotations instead of guessed types.
- Immutable list construction and core operations; scalar contains uses value
  equality, including String contents rather than pointer identity.
- Exhaustive matches with scalar literals, catchalls, Some/None/Ok/Err patterns,
  scoped payload bindings, and multiline arm bodies.
- Postfix `?`, which continues with an Ok payload or returns the Err value from
  its helper immediately, checking compatibility with the enclosing error type.
- Diagnostics for discarded Result expressions, unused Result-bearing bindings,
  parameters, named pattern bindings and terminal aliases, nonexhaustive/unreachable
  arms, type mismatches, and unsupported guards/nested patterns. General
  path-sensitive ownership analysis remains open.
- Multiline calls, lists, and type arguments, with explicit source/token/type/depth
  limits and an occurs check against recursive inferred types.

Try the collection and error examples:

```sh
just rust-build
./bin/fern-rs run compiler-rs/tests/collections/lists.fn
./bin/fern-rs run compiler-rs/tests/collections/propagation.fn
./bin/fern-rs run compiler-rs/tests/collections/nested_sums.fn
```

The propagation example prints `continued`, then `four`, then `division by zero`.
The failure returns before the second `continued` could execute. Main explicitly
matches the helper's Result; Rust main still returns Int or Unit.

## Runtime representation

Semantic types stay distinct throughout IR even when they have identical machine
widths. Lists and Results reuse the existing C runtime. Option uses that runtime's
heap-backed Result helpers internally, mapping Some to Ok and None to Err with an
unused zero payload. This avoids the old packed Option representation, which
truncates payloads to 32 bits. Native tests retain full Int values and String/list
pointers, including nested sum values and 1,500 allocations.

The shipping C Option ABI is unchanged. Rust calls to external APIs returning that
packed representation will need explicit adapters. No packed Option runtime call
is emitted by this frontend. Valid-index and nonempty-list preconditions of
`List.get`/`List.head` remain unchanged; callers must check lengths before reading.

## Verification

The complete Rust suite has 94 passing tests, including 24 parser tests,
29 checker tests, and 28 emitter tests. The existing 542 C tests remain passing.

The milestone adds 31 native programs (19 fixed, 12 seeded generated) and 22
negative compile cases. Fixed cases cover wide signed values, strings, booleans,
nested lists and sums, Unit, immutable aliases, branch-local bindings, evaluation
order, propagation, and retained pointers under repeated allocation. Invalid
programs must fail with source diagnostics and preserve an existing output file.

These are exact expected-output tests. The original 32-program differential
suite against C remains in place and continues to report its five known backend
differences. The new heap Option representation is checked against Fern semantics
directly because it deliberately differs from C's packed ABI.

Run `just rust-check`, `just check`, and `just docs-check` to reproduce the gates.
The same Rust gates are configured in the existing Linux/macOS CI matrix; this
milestone was verified locally on macOS arm64.

## Updated release measurements

`just rust-evaluate` passed with the expanded frontend, using the same 101-function
shared-subset fixture, 15 check/emit samples and three verified native builds per
compiler. The host is macOS arm64 with Rust 1.75 and Apple clang 16. See
[raw measurements and hashes](reports/rust-collections-evaluation-2026-09-05.json).

| Measurement | C | Expanded Rust |
| --- | ---: | ---: |
| Check median / p95 | 2.85 / 5.10 ms | 3.46 / 4.03 ms |
| Emit median / p95 | 4.03 / 4.68 ms | 3.23 / 4.28 ms |
| Native build median / maximum | 112.00 / 112.86 ms | 101.11 / 245.06 ms |
| Compiler + QBE backend | 549,384 bytes | 982,112 bytes |

The Rust executable is 668,072 bytes and its separate QBE helper is 314,040 bytes;
the shared runtime/system libraries are excluded from both totals. Native output
is verified for every timed build, and the new collections corpus also passes
against the release compiler. These small, local samples include process startup
and lack CPU isolation. They demonstrate a usable edit/check loop, not a general
speed advantage or full-language parity. This synthetic fixture does not measure
collection-heavy compilation separately.

## Remaining migration work

User-defined algebraic types and generics, records/tuples/maps, nested patterns and
guards, modules, closures/higher-order calls, `with`, full standard-library bindings,
and diagnostic/tooling parity remain open. Main Result exit semantics, safe
collection indexing, and the runtime's packed Option interoperability also need
explicit follow-up. Formatter, REPL, LSP, packaging, and full release parity must
pass before switching the default.

See the [supported-feature guide](../compiler-rs/README.md),
[decision 46](../DECISIONS.md), and the [roadmap](../ROADMAP.md).
