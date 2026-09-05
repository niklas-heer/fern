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

At this historical checkpoint, generic bodies were checked only when concretely
instantiated. The generic-definition checkpoint below closes that gap.

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


## Iteration and typed with handlers — 2026-09-05

For loops now consume one immutable List, Map or Range snapshot. Range values
store endpoints instead of allocating elements; native and interactive loops
check the inclusive endpoint before incrementing, including Int::MAX. Map loops
yield full-width key/value tuples in insertion order. List enumeration retains
semantic values, including Float and closures. Loop targets are local to each
function; break/continue preserve registered function-exit cleanup.

With blocks retain flat sequential steps and distinct typed error handlers.
Each handler checks applicable source arms in order and requires exhaustive
coverage, while preserving its error payload's real type. A failed step skips
all following initializers and the success body. Handlers cannot access success
bindings; they can return, propagate errors or control an enclosing loop using
the existing abrupt-exit rules. Omitting else uses ordinary Result propagation.

Native fixtures cover heterogeneous nominal errors, generic With specialization,
Float payloads, scope shadowing, guarded handlers, full-width range boundaries,
collection enumeration, retained range/loop captures and cleanup during loop
control. Interactive tests enforce the same semantics and bounded range work.


## Numeric domains, literal text and runtime faults — 2026-09-05

Int power uses bounded exponentiation by squaring; bitwise shifts normalize counts
modulo 64. Int minimum divided by -1 wraps consistently, and its remainder is zero.
Float power uses libm, and Float membership compares IEEE values in both direct
and first-class contains calls. Base-prefixed integer digits and separators are
validated before producing a full-width value.

Generated functions now receive an explicit fault-context pointer after their
closure environment. An escaping closure receives its current caller's context;
it never retains a pointer to a finished invocation. Guards check this context
before using call results or executing subsequent effects. Numeric domain errors
unwind through the same deferred cleanup path as other function exits. Cleanup
callbacks run with clear fault state, then restore the first failure. Native main
emits one stable diagnostic and exits 1. This changes only compiler-owned calling
conventions; the C runtime ABI and source Function/Result types are unchanged.

Triple strings preserve actual content bytes, block comments may nest within
limits, and @doc metadata remains associated with declarations. Unicode identifier
spelling is preserved without normalization. Formatting must retain text contents
and documentation. Runtime-fault fixtures separately verify cleanup/effect order,
error messages and exit status, alongside valid and rejected source programs.


## Private return inference and runtime boundaries

Private functions with annotated parameters may infer their returns before
concrete specialization. A bounded shared-constraint pass supports forward
references and recursive definitions with a type anchor, including generic
schemes established by their bodies. Unresolved recursive or generic results
require an annotation. Public signatures retain explicit return types, including
when definitions are imported; omitted main remains Unit. Retry work and
inference storage have a shared budget, rather than restarting the full budget
for every dependency retry. Parameter inference and function clauses remain open.

`main -> Result((), E)` accepts a concrete error type: Ok exits 0, Err exits 1
with `fern: main returned Err` on stderr, after cleanup. Error payload display
awaits a general display protocol. Runtime faults take precedence over entry
Results, and the first fault remains authoritative during cleanup.

List.get/head check bounds before access through direct, registry and first-class
calls. String.repeat checks its 16 MiB content cap before multiplication or
allocation and returns empty immediately for nonpositive counts or empty input.
The shared C helpers independently enforce these limits even in release builds;
Rust-generated calls additionally unwind deferred cleanup through the fault
context. These direct-valued APIs are not recoverable Result APIs yet.

String.slice retains clamped byte indexing but rejects endpoints inside Unicode
scalars. Empty-delimiter String.split produces complete scalars, including
separate combining marks, and produces no elements for empty input. A preflight
check also rejects malformed UTF-8 entering through legacy native I/O before
splitting, preserving cleanup. Nonempty-delimiter behavior remains unchanged;
consistent UTF-8 validation at all native string ingress is still an audit item.


## Shared list and tuple sequence patterns

Exact list patterns and final list/tuple rest patterns use the same AST, checked
IR, coverage analysis and execution engine in match, let-else, irrefutable
bindings, loops and with. Result discard checks include prefix and suffix values.
List lengths dominate all element reads; named suffix copying is delayed until
all nested structural checks succeed, before a guard needs its binders. Ignored
tails allocate nothing and whole-list bindings alias the original immutable list.
Tuple suffixes preserve singleton identity and use Unit for empty suffixes.

Coverage expands list shapes into conceptual empty/nonempty constructors, with
an expansion depth budget checked before building recursive matrices. The REPL
stages bindings atomically and reserves suffix-copy work before allocating, so
failed siblings or exhausted budgets cannot publish partial bindings. Retained
closure code storage also counts the new pattern variants. Named list suffixes
currently copy, making repeated recursive tail scans quadratic; persistent list
views and tail-call optimization remain separate performance work.


Embedded expression suites now preserve bounded indentation frames inside calls,
lists and tuples. Match, if, for, with and callbacks compose with inline separators
and closing delimiters; ordinary nested delimiters still suppress layout. Ten
native programs and formatting/checked-IR equivalence cover sequence bindings,
block arguments, effect order, full-width payloads, Result tails and escaping
closures. Fifteen invalid programs retain the output-file preservation contract.
An independent audit of 1,850 small Boolean/nested-list pattern matrices found no
coverage/usefulness mismatch; this supplements the bounded checker regressions.

## Function clauses and native self recursion — 2026-09-05

Adjacent typed parameter clauses normalize through the existing exhaustive match
engine before private return inference. Guards, source spans, module visibility,
function-owned return/defer behavior and Result obligations are preserved.
The formatter retains clause groups and arrow/colon syntax. Interactive `:paste`
and `:end` submit a whole group without retaining incomplete definitions.

Eligible direct self calls in return position jump to a local recursion header
only after all arguments finish. Parameter and for/with scratch slots are allocated
once in the native entry block. Million-step programs cover scalar and pointer
values, first-class entry, nested cleanup, argument order and runtime faults.
Owned defer registration disables frame reuse; indirect/mutual recursion and
interactive evaluation retain their existing call behavior. General proper tail
calls and zero-copy list suffixes remain future work.

This checkpoint passes 541 Rust tests, including the backend's independent
255-parameter coverage checks, and retains 550 passing C tests. New native
coverage includes seven clause programs, seven successful recursion programs
and one controlled-fault recursion program, plus 11 invalid inputs.

## Pattern-anchored private parameters — 2026-09-05

Omitted private parameter annotations now collect evidence across the entire
clause group before normalization. Literals, nested sequence/sum patterns and
nominal constructor schemas determine types independently of callers. Tuple-rest
constraints wait for known arity; later annotations can supply declared generics.
Public omissions, conflicting evidence and still-ambiguous types report diagnostics.
The pass uses a separate aggregate 400,000-operation inference budget across groups,
in addition to the existing source/type/depth and return-inference bounds.

Seven native programs cover recursive dispatch, inferred container payloads,
constructor schemas, tuple-rest ordering, independent generic instantiations and
an imported private helper. Nine invalid programs and two interactive regressions
check rejection and session recovery. Whole-signature generalization remains open.

The pattern-inference checkpoint passes 554 Rust tests, 550 C tests and the
complete native, fuzz and documentation gates on macOS arm64.

## Generic definition schemes — 2026-09-05

Every generic body is checked before specialization, including unused functions,
nested callbacks and pattern coverage. Declared variables obey rigid type equality;
a literal cannot satisfy an arbitrary promised generic return. Intrinsic operation
requirements preserve generic arithmetic, scalar display, membership and map keys,
and propagate through calls, function values and recursive helpers. Nominal field
requirements prevent invalid Map key types from hiding inside record signatures.

The solver performs one body check per generic definition and closes deduplicated
requirements under the existing shared 400,000-operation inference budget. Only
surviving finalized calls contribute requirements, preserving early exits. Concrete
specialization remains a second validation boundary, including conditional Result
restrictions. Full source-level traits/constraints and private signature generalization
remain separate milestones.

Four native programs cover parametric helpers, capabilities, callbacks, closures,
recursive requirements and nominal fields. Sixteen invalid CLI programs and three
interactive regressions check early rejection and preserved session state.

Generic scheme checkpoint gates pass: 579 Rust tests, 550 C tests, complete
native/fuzz checks and documentation checks on macOS arm64.

## Source navigation and completion

The Rust language server now advertises go-to-definition and completion alongside
its existing synchronization and diagnostics. Definition locations use original
source identities, preserving separate clause binders, local shadowing, captures,
constructor/type references and visible imported names. Unsaved dependencies,
module aliases and public re-exports use the compiler's module-resolution facts.

Each request builds a fresh bounded index from accepted editor buffers. Syntax
errors never reuse an older source location; type errors do not prevent lexical
navigation through a successfully parsed graph. Lexer spans exclude comments and
literal text while retaining code inside interpolation. Both cursor positions
and replacement ranges use UTF-16, including non-BMP identifiers.

Completion filters by lexical scope and prefix, with deterministic ordering and
limits of 256 entries and 1 MiB of serialized output. Builtin prefix suggestions
also work for incomplete expressions such as `List.`. Ordinary compilation does
not retain editor-only symbol copies. Typed hover, record-member completion,
rename, code actions and broader incomplete-code recovery remain open.

Retained editor symbol names have an aggregate 8 MiB byte limit, in addition to
the entry count bound. This bounds the new editor copies; the existing resolver
still has a separate transient alias-expansion limitation.

The checkpoint adds 21 navigation regressions and three module/index regressions.
An executable JSON-RPC smoke verifies initialization, definition, scoped
completion, shutdown and clean protocol output.

Reviewed navigation checkpoint gates pass: 603 Rust tests, 550 C tests, complete
native/fuzz gates and documentation checks on macOS arm64.

## Parser-based source documentation

`fern-rs doc source.fn` now generates Markdown from the Rust parser. `--html`
produces a standalone page, and `-o` installs either format atomically. Invalid
syntax leaves existing output intact; source aliases cannot be overwritten.
Generation needs neither main nor the native backend and executes no user code.

Functions retain original headers, including pattern clauses, guards, nested
function types and omitted annotations. Clauses share one documentation entry;
types retain their fields and variants. Literal @doc metadata is attached by
source position to its owning declaration. HTML escapes source, titles and doc
text; Markdown uses source-safe code fences and retains authored doc markup.

The initial command accepts one file and includes private/public declarations.
Input is limited to 1 MiB and 4,096 declarations; output is limited to 8 MiB.
This does not yet provide directory navigation/search, inferred documentation
signatures or executable Rust doc tests. Fifteen new library/CLI regressions
cover source syntax, ownership, escaping, resource limits and output preservation.

Source-documentation checkpoint gates pass: 618 Rust tests, 550 C tests, the full
native/fuzz suite and documentation checks on macOS arm64.

## Core whole-private-signature inference

The Rust checker now infers omitted private parameters and returns from all
clause patterns and function bodies before consulting callers. It analyzes
lexical dependencies with an iterative graph traversal and solves recursive
components callee first. Unfinished recursive members share type constraints;
completed named functions instantiate independent schemes at each reference.
Identity, generic list recursion, apply/compose, returned closures and nominal
constructor helpers work without explicit private parameter annotations.

Intrinsic requirements remain attached to generalized definitions: an inferred
addition helper supports Int, Float and String, while inferred multiplication
retains its numeric restriction. Literal evidence can establish a concrete type.
Return-only None/empty-list schemes receive independent contextual payload types.
Local aliases remain monomorphic; explicit universal annotations remain rigid.
Public boundaries, unanchored recursive returns, occurs checks, Result obligations
and concrete backend validation remain enforced. Fully annotated parameter
components preserve the existing generic return-inference behavior.

One 400,000-unit inference budget spans dependency traversal, pattern/body
constraints, recursive solving, generalization and final generic requirements.
The former repeatedly retried forward chain now resolves in dependency order.
Declared source/type/depth limits still apply before internal inference slots.
Diagnostic type descriptions use a bounded source renderer, stripping ownership
only from semantic compiler-owned rigid variables and retaining the distinction
between separate type parameters with the same original spelling.

Core acceptance adds eight native programs and 14 invalid programs, including
both caller orders, mixed annotated/inferred recursion and imported helpers.
Three REPL regressions cover retained generic definitions, callbacks and rollback.
The earlier identity/list-length rejection fixtures become positive coverage.
Delayed field/update/iteration/tuple-rest evidence from later expressions remains
an open checkpoint; unknown record shapes and tuple arities are never guessed.

Core signature checkpoint gates pass: 670 Rust tests, 550 C tests, 166 core native
programs, 21 entry/access programs, 148 invalid inputs and the complete fault,
fuzz and documentation suites on macOS arm64.

## Delayed private-signature shapes

Field access, record updates, tuple-rest bindings and iteration now gather later
body evidence through bounded structural obligations. Evidence can come from
either branch, a callback context or another use of the same value. Exact tuple
suffix evidence works in both directions, and receiver enumeration remains
distinct from a record callback named enumerate. Unknown shapes remain errors.

Inference uses explicitly tagged, non-executable probes with crate-private
construction. After settling obligations, the original source is rechecked with
its generalized signature. Final IR publication, QBE and REPL storage/execution
reject surviving probes, including unreachable children. Constraint storage,
field lookup, substitution and repeated settling share the inference work budget.

Fourteen checker regressions, six internal boundary/budget checks and a
compile-fail construction test cover the implementation. Seven native programs
and eight invalid programs verify output order, callbacks, guards, cleanup and
Result handling. Two interactive regressions cover retained generic closures,
iteration effects and rollback after rejected entries.

Delayed-shape checkpoint gates pass: 693 Rust tests, 550 C tests, 173 core native
programs, 156 invalid inputs and the complete entry/access, controlled-fault, fuzz
and documentation suites on macOS arm64.

## Native JSON core

The runtime now provides an immutable opaque JSON tree with validating parsing,
ordered compact serialization, typed accessors and exact number text. Explicit
Int conversion rejects fractions/overflow; Float conversion preserves binary64
bits and uses a private locale. Unicode validation, decoded duplicate-key
rejection, escaped NUL preservation and explicit resource limits are documented
in [the native contract](JSON_NATIVE_CORE.md).

This checkpoint preserves both legacy JSON symbols and current source bindings.
Fourteen new ABI symbols remain explicitly internal until the source-type and
adapter migration lands. Debug, release and ASan/UBSan runs each pass 14,309 API
checks, 24 internal budget checks and 6,000 deterministic numeric oracle cases.
The new native gate runs with the full Rust suite.

The integrated native JSON checkpoint passes all C/Rust/native/fuzz/documentation
gates on macOS arm64; existing source-language behavior remains compatible.
