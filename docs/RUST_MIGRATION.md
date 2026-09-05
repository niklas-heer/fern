# Incremental Rust migration

The Rust frontend now supports custom types, generic functions, modules, immutable
lists, and error/optional values from source parsing through native execution. The shipping `fern` compiler stays
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
  arms and type mismatches. General
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

## Custom types, modules, and formatting — 2026-09-05

- Nominal sum and record types, recursive generic layouts, field access, and
  bounded concrete specialization of generic functions.
- Nested pattern coverage, guarded arms with scoped bindings, and tag checks
  before loading payload fields. Guards never count toward exhaustiveness.
- Bounded module loading with aliases, selective imports, reexports, visibility,
  cycle checks, and diagnostics attributed to their original source files.
- A Rust formatter with comment preservation, idempotence, complete structural
  validation, and atomic updates. It never invokes the C formatter.
- Six native applications cover recursive trees, full-width record payloads,
  contextual generic inference, guard evaluation order, and a multi-file project.

Generic signatures and structural bounds are validated eagerly; generic bodies
are type-checked when concretely instantiated. Checking unused generic bodies
remains a diagnostic-parity limitation.

## Numeric, application, and interactive expansion — 2026-09-05

Float arithmetic/comparison/printing retains IEEE doubles through generic payloads;
signed zero and NaN behavior are checked natively. Structural tuples support nested
patterns, destructuring and projections. Pipes preserve evaluation order, and
interpolation supports nested expressions with typed scalar conversion.

The runtime registry describes source signatures and their physical transport.
String-list, packed byte Option, regex tuple and process-result adapters copy into
the Rust layouts explicitly. Substring offsets bypass the old narrowing ABI.
Opaque terminal objects cannot be fabricated from user records or integers.
The process runtime now executes `System.exec_args` as literal argv with
`posix_spawnp`, fixing an existing quote-expansion allocation bug.

A persistent typed-IR REPL retains successful bindings without replaying effects.
The Rust LSP tracks UTF-16 edits and imports across unsaved buffers. Both enforce
resource limits and recover from invalid user entries. These tools do not yet
establish complete editor or runtime parity.

## Runtime representation

Semantic types stay distinct throughout IR even when they have identical machine
widths. Lists and Results reuse the existing C runtime. Option uses that runtime's
heap-backed Result helpers internally, mapping Some to Ok and None to Err with an
unused zero payload. This avoids the old packed Option representation, which
truncates payloads to 32 bits. Native tests retain full Int values and String/list
pointers, including nested sum values and 1,500 allocations.

The shipping C Option ABI is unchanged. Rust uses an explicit adapter for the
lossless packed byte result from `String.char_at`; substring offsets use a
full-width implementation. Further packed Option APIs still require auditing. Valid-index and nonempty-list preconditions of
`List.get`/`List.head` remain unchanged; callers must check lengths before reading.

## Verification

The collections checkpoint had 94 passing Rust tests and 542 passing C tests.
The expanded checkpoint has 243 passing Rust tests, 550 passing C tests and 81 core native programs,
plus 34 rejected invalid programs. Directory contracts add four C ABI cases,
eight native programs and two Result binder regressions across both frontends.
The 192-case seeded mutation suite checks diagnostic termination, formatter
stability and unchanged output behavior. Format, clippy, C quality, documentation
and native/Python style-checker parity gates pass locally on macOS arm64.

The milestone adds 31 native programs (19 fixed, 12 seeded generated) and 22
negative compile cases. Fixed cases cover wide signed values, strings, booleans,
nested lists and sums, Unit, immutable aliases, branch-local bindings, evaluation
order, propagation, and retained pointers under repeated allocation. Invalid
programs must fail with source diagnostics and preserve an existing output file.

These are exact expected-output tests. The original 32-program differential
suite against C remains in place and currently reports four known backend
differences. The new heap Option representation is checked against Fern semantics
directly because it deliberately differs from C's packed ABI.

Run `just rust-check`, `just check`, and `just docs-check` to reproduce the gates.
The same Rust gates are configured in the existing Linux/macOS CI matrix; this
milestone was verified locally on macOS arm64.

## Collections-stage release measurements

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

`with`, delayed Result ownership, full actor execution, multiline strings,
block comments, and diagnostic/tooling parity remain open. Main Result exit semantics, safe
collection indexing, and packed Option interoperability also need follow-up.
REPL/native semantic coverage, additional LSP features, packaging, and full release
parity must pass before switching defaults.

See the [supported-feature guide](../compiler-rs/README.md),
[decision 46](../DECISIONS.md), and the [roadmap](../ROADMAP.md).

Directory native parity exposed a C backend defect: matched Result payloads were
classified as Strings solely because their storage was 64 bits. Pattern binders and expressions
now retain checker-owned semantic types through emission, with all AST factories
initializing that metadata and match-arm scopes restoring it. Native regression
coverage distinguishes integer error arithmetic from String error concatenation;
this correction also supports the explicit directory Result migration in Decision 50.
Result constructors explicitly extend integer payloads or bitcast floating
payloads into the native full-width representation. Checked expression types
also determine function argument/result widths and prevent an outer pattern
binder from changing the type of a shadowing inner let expression.

The native bootstrap gate also exposed multiline match arms being parsed as single
expressions. C now retains their statement blocks, consumes only owned indentation
boundaries and stops after parse errors. Seven regressions cover mixed branch layouts, nested arms,
condition-only matches, following declarations and malformed-input termination.

## Functions and higher-order execution — 2026-09-05

The typed pipeline now infers anonymous functions, specializes named function
values and lifts escaping closures into explicit environments. Native generated
functions share one hidden-environment convention; runtime/builtin function values
use concrete wrappers. List map/fold/filter/find/any/all and Option/Result callbacks
execute typed calls, preserving Float payloads, short-circuiting and source order.
Empty list output uses a valid runtime allocation capacity.

Interactive closures retain their originating immutable program so later entries
cannot change function identities. Unique code and captured graphs share an
aggregate storage budget. The REPL accepts open calls and commented closure headers.
The formatter and module loader support block callbacks and nested function types.
Result-bearing captures currently receive an explicit diagnostic pending delayed
ownership tracking; callbacks may return and propagate Results normally.

Six native programs cover escaping/independent/nested captures, 512 retained closures,
full-width and Float callbacks, records and all ten higher-order operations. Six
invalid programs exercise type and obligation diagnostics. Additional checker,
parser, emitter, formatter, module and interactive regressions protect the pipeline.

Closure checkpoint gates pass locally: 287 Rust tests, 550 C tests, 87 core
native programs, 40 invalid programs, dual-frontend directory/Result contracts,
192 seeded mutations, formatting/clippy and documentation checks.

## Maps and record updates — 2026-09-05

Immutable maps support Int/Bool/String keys and arbitrary concrete values through
source parsing, contextual inference, generic specialization, native execution,
formatting, modules and interactive sessions. Compiler-owned helpers reuse GC
allocation and list primitives; they do not add a C Map ABI. String equality uses
contents. Duplicate keys keep their insertion position and last value. Public
updates preserve existing aliases, and empty maps use valid allocation capacity.
The initial search/update implementation is linear.

Record updates now construct changed records, evaluating the base and every field
expression once in source order. This is verified against explicit native output;
the C backend's unchanged-record behavior is not an oracle. Six new native programs
and nine invalid cases cover scalar keys, full-width/Float/Result/closure values,
ordered effects, missing keys, persistent aliases and retained environments.

The design document's old milestone checkmarks have been replaced by acceptance
criteria with links to the active roadmap, where remaining type-system and
standard-library work is explicit. C remains the shipping default while the
remaining language and migration milestones are completed.


## Early exits and deferred cleanup — 2026-09-05

The Rust frontend now supports explicit returns, postfix conditionals,
condition-only matches, let-else unwrapping and dynamic function-exit cleanup.
An internal bottom type identifies paths that do not produce values. Native
branches join only continuing paths; strict argument evaluation stops at the
first executed return, including inside otherwise polymorphic calls.

Deferred code is lifted into a captured Unit function. A GC-visible function
stack records registrations, and one epilogue preserves the full-width return
value before invoking cleanup in reverse order. Nested blocks retain their
registrations until the enclosing function exits. Early returns and propagated
Result errors use the same exit path. User lambdas retain independent boundaries.

Interactive evaluation preserves the same ordering and additionally attempts
cleanup after evaluator failures. An entry-wide 10,000-step cleanup budget is
separate from the 100,000 ordinary evaluation steps; each function permits at most
4,096 pending cleanups. The original evaluation error is preserved if cleanup also
fails. Retained code accounting includes cleanup and let-else branches.

Nine native programs and ten invalid cases cover skipped operands, Float/Int64
returns, conditional registrations, lexical snapshots, failure propagation,
let-else scopes, lambda boundaries and recursive captures. Interactive tests also
verify cleanup after division and work-budget failures. With blocks and collection
iteration remain the next control-flow requirements; C remains the default.
