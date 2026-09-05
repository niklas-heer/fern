# Experimental Rust frontend

An independent, dependency-free Rust frontend for evaluating Fern's next compiler
architecture. The shipping `fern` compiler remains C. This prototype implements a
bounded subset; it is not a replacement for the current compiler.

## Run it

Install the [native build dependencies](../BUILD.md), plus Rust 1.75 or newer,
Cargo, rustfmt, and clippy. From the repository root:

```sh
just rust-build
./bin/fern-rs check compiler-rs/tests/corpus/hello.fn
./bin/fern-rs emit compiler-rs/tests/corpus/hello.fn
./bin/fern-rs build compiler-rs/tests/corpus/hello.fn -o hello-rs
./hello-rs
./bin/fern-rs run compiler-rs/tests/corpus/string_function.fn
./bin/fern-rs run compiler-rs/tests/collections/propagation.fn
./bin/fern-rs run compiler-rs/tests/types/project/main.fn
./bin/fern-rs fmt source.fn
just rust-check
```

For frontend-only work, no C build or native dependencies are necessary:

```sh
cargo test --locked --offline --manifest-path compiler-rs/Cargo.toml
cargo run --locked --offline --manifest-path compiler-rs/Cargo.toml -- check examples/tiny_cli.fn
```

`check` and `emit` use only Rust. `build` and `run` require `fern-qbe`,
`libfern_runtime.a`, a host C compiler, and the existing runtime's native libraries.
The helper and archive are located beside `fern-rs`, then in the development
checkout. `FERN_QBE` and `FERN_RUNTIME_LIB` override their paths; `CC` selects a
single compiler executable (not a shell command). `run source.fn -- args` forwards
literal arguments, available through `System.arg`, `System.args`, and `System.args_count`.

## Supported language

- Top-level functions with typed parameters, private return inference, and forward/recursive calls.
  Public function signatures require return annotations. Parameterless `main` returns
  `Int`, `Unit`, or `Result((), E)` with a concrete error type; omitted `main`
  return type means `Unit`. Unresolved recursive return types require an annotation.
- Signed 64-bit `Int`, IEEE double `Float`, `Bool`, UTF-8 `String`, `Unit`/`()`, and recursively nested
  concrete `List(T)`, `Option(T)`, and `Result(T, E)` types.
- Custom sum and record declarations, concrete generic type applications, record fields,
  and generic functions specialized for their concrete call types.
- Module declarations, public functions/types, qualified/aliased/selected imports,
  and public reexports from `module.fn` or `module/mod.fn`.
- Immutable `let` with inferred or annotated type, lexical scopes, and shadowing.
- Structural tuple types/literals, nested tuple/list patterns, suffix binding with `..rest`, and `.0` field access.
- Standard and placeholder pipes evaluate their input exactly once before other arguments.
- Integer/Float arithmetic, powers and comparisons, Int bitwise operations, string addition/equality, boolean operators
  with short-circuit evaluation, and unary `-`/`not`.
- Inline and indented `if`/`else`, including value-producing branches. An `if`
  without `else` has type `Unit`.
- Early returns, postfix conditionals, condition-only matches and `let-else` unwrapping.
- Sequential `with` Result bindings with typed handlers or implicit error propagation.
- List/Map/range `for` iteration, list enumeration and scoped `break`/`continue`.
- Dynamic `defer` cleanup runs at function exit, including early return and `?` propagation.
- `print`, `println`, `String.concat`, `String.eq`, and `String.len`.
- Immutable list literals, `List.len/get/head/tail/is_empty/push/reverse/concat`,
  and scalar `List.contains` (Int, Float, Bool, or String elements).
- `Some`, `None`, `Ok`, and `Err`, with payload types inferred from bindings,
  function signatures, calls, and branches. Empty lists also use this context.
- Exhaustive `match` on scalar literals, Bool, lists, nominal types, Option, or Result, with wildcard
  and name catchalls, recursively nested constructor patterns, guards, and scoped multiline arms.
  Guarded arms do not establish exhaustiveness.
- Postfix `?` unwraps Ok or immediately returns Err from a Result-returning helper;
  the enclosing error type must agree. Result expressions cannot be silently
  discarded. Result-returning main may propagate errors with `?`.
- `Option.is_some/is_none/unwrap_or` and `Result.is_ok/is_err/unwrap_or`.
- Multiline lists, calls, and type arguments with checked delimiters.
- Nested string interpolation with Int/Float/Bool/String expressions and escaped literal braces.
- Immutable Map values with Int/Bool/String keys, ordered inspection and optional lookup;
  record updates that evaluate their base and fields once.
- First-class named/anonymous functions, contextual callback inference, escaping captures
  and typed higher-order List/Option/Result operations.
- Line and nested block comments, triple-quoted strings, declaration documentation,
  Unicode identifiers, validated base-prefixed integers and source-located diagnostics.
- Audited runtime bindings for files, strings, regex, arguments/processes, SQLite,
  HTTP clients, explicit mailboxes and terminal UI. Opaque annotations are qualified
  (`Tui.Panel`, `Tui.Tree`, etc.), keeping ordinary user-defined names available.

Unsupported syntax produces diagnostics. Gaps include actor execution, named
arguments and full private signature generalization.
Full release parity remains migration work.

`fmt` formats the supported syntax in place, preserves comments, and verifies that
the complete syntax tree remains equivalent before replacing the file atomically.
Comments inside multiline arguments may move adjacent to their statement. Invalid
source remains untouched. Every generic body is checked before specialization,
including unused definitions; concrete instantiation supplies a second validation
boundary.

An expression such as `let empty = []` or `let missing = None` needs enough later
usage or an annotation to determine its payload type. For example,
`let empty: List(String) = []` is concrete. Unsupported compound equality is
rejected; it does not silently compare pointers. Invalid `List.get`/`List.head`
access reports a runtime error after deferred cleanup. Their signatures still
return direct values; recoverable collection indexing remains future work.

Strings inherit the existing C runtime: embedded NUL is rejected, and `String.len`
counts UTF-8 bytes. String repetition rejects results larger than 16 MiB before
allocation; empty strings and nonpositive counts produce an empty string immediately.
String.slice uses clamped byte offsets and rejects endpoints inside a Unicode scalar.
Splitting on an empty delimiter returns complete Unicode scalars. Integer domain
faults are reported after cleanup, as described below. A source
file is limited to 1 MiB, 65,536 tokens, and a syntax nesting depth of 128. These
explicit prototype limits prevent unbounded parser recursion/allocation.

## List and tuple patterns

```fern
fn length(items: List(Int)) -> Int:
    match items:
        [] -> 0
        [_, ..rest] -> 1 + length(rest)

fn first_or_zero(items: List(Int)) -> Int:
    let [first, .._] = items else: return 0
    first
```

List patterns without rest require an exact length. A final `..rest` binds the
suffix; `.._` ignores it. Tuple suffixes remain tuples, including singleton tuples;
an empty tuple suffix is Unit. Ignoring a prefix or suffix that contains Result
values is rejected. Guards run after all structural checks and required bindings.

Ordinary let, for and with success bindings require irrefutable patterns. A
nonempty list prefix can fail, so use match or let-else. Zero-prefix `[..all]`
always matches a list and shares its original immutable value. Fixed tuple
suffix bindings are irrefutable when their prefix fits the tuple type.

Named list tails currently copy their suffix once the entire structural pattern
matches. Recursive decomposition can therefore take quadratic time; prefer for
or List.fold for large scans. Ignored tails allocate nothing. Interactive tail
copies share the evaluation work budget, and failed matches publish no bindings.
The compiler bounds both source pattern nesting and expanded coverage analysis.
Multiline match, if, for, with and callback expressions also work inside call
arguments, list elements and tuple elements, including inline separators/closers.
Result handling still uses reference-based checks beyond wildcard patterns;
inspecting a List(Result) length can currently satisfy its usage obligation.
Complete semantic Result-consumption tracking remains migration work.

## Entry results and return inference

A private function with annotated parameters can omit its return annotation:
`fn twice(value: Int): value * 2`. Forward calls and recursive definitions with
sufficient type constraints infer concrete return types; unanchored recursive
cycles require an annotation. Public signatures remain explicit. Generic helpers
retain their declared type parameters and specialize independently per call.

`main -> Result((), E)` exits 0 for `Ok(())` and 1 for `Err`, after deferred cleanup.
An unhandled entry error currently prints `fern: main returned Err` to stderr;
rendering arbitrary error payloads awaits a general display protocol. A runtime
fault in the body or its cleanup takes precedence over the entry Result.

## Generic definition checking

Declared generic variables are rigid: `fn identity(x: a) -> a: 1` is an error even
if the function is never called. The checker also validates nested matches,
callbacks, return types and known Result obligations in unused definitions.

Generic operations retain their actual requirements. For example,
`fn square(x: a) -> a: x * x` accepts Int and Float instantiations, while
`fn describe(x: a) -> String: "value={x}"` supports scalar interpolation.
These requirements propagate through named calls, function values, closures and
recursive helpers. Map key restrictions also apply inside nominal field types.
No arbitrary Int instance is used to validate a generic body.

Intrinsic requirements are internal in this stage; public `where`/trait syntax
remains future work. Conditional restrictions on generic Result-bearing values
are still verified when concretely instantiated, and the broader reference-based
Result handling limitation described below remains.

## Pattern-based parameter inference

Private clauses can infer a shared input type from literals and constructors:

```fern
fn factorial(0) -> 1
fn factorial(n) -> n * factorial(n - 1)
```

This infers `Int -> Int` before considering callers. Nested list, tuple, Option,
Result and nominal patterns contribute constraints across the complete group.
An annotation in a later clause can anchor an earlier omission, including a
shared declared generic type. Tuple-rest patterns wait until another pattern or
annotation determines the complete tuple arity.

Unconstrained `fn id(x) -> x` and generic `length([])` / `length([_, ..tail])`
still require annotations in this stage. Full body-based signature inference and
generalization are separate work. Public function parameter annotations remain
mandatory even when a literal would make the type apparent.

## Function clauses and native recursion

Adjacent clauses support typed parameter patterns, guards and arrow bodies:

```fern
fn total([]: List(Int), acc: Int) -> acc
fn total([head, ..tail]: List(Int), acc: Int) -> total(tail, acc + head)
```

Clauses share parameter types and visibility, and any supplied return annotations
must agree. Public groups need a return annotation. Guards run in source order;
missing cases and unreachable clauses are errors. Patterns use the same rules as
`match`, including Result discard checks. Private parameter annotations can be omitted when patterns or other clauses
determine their complete types. Public parameters remain annotated. Put
whole-function `@doc` text before the first clause.

Native direct self calls in return position reuse the current stack frame when the
function has no owned `defer`. Arguments finish left-to-right before parameters
change; Float bits, pointers and the current fault context are retained. A function
with `defer` retains separate activations and cleanup for each call. Mutual and
indirect recursion still use ordinary calls. These are native optimizations;
interactive evaluation retains its explicit step/depth limits. List suffix patterns
still copy their tails, so tail-call elimination alone does not make repeated list
suffix traversal linear-time.

In the REPL, enter `:paste`, a complete clause group, then `:end` on its own line.
Blank lines inside paste mode are retained. Only a successful complete entry is
saved; EOF before `:end` discards the unfinished entry.

## Functions and callbacks

```fern
fn make_adder(base: Int) -> (Int) -> Int:
    (value: Int) -> base + value

fn main():
    let add = make_adder(40)
    println(add(2))
    let numbers = List.map([1, 2, 3], (value) -> value + 1)
    println(List.fold(numbers, 0, (total, value) -> total + value))
```

Both `(value) -> expression` and `fn(value) -> expression` support indented
bodies, including callbacks inside argument lists. Function types use
`(Int) -> Int` or `fn(Int) -> Int`. Context infers omitted lambda parameter types;
local function bindings remain monomorphic. Named generic and builtin/runtime
functions specialize from their concrete expected function types.

Captures evaluate once and survive their defining scope. Higher-order operations
preserve input order; `List.find`, `List.any` and `List.all` stop as soon as the
result is known. Empty collections and absent/error sums skip their callbacks.
Each callback has its own `?` propagation boundary. Capturing an already-produced
Result-bearing value currently gives an explicit diagnostic; delayed ownership
tracking is still required to lift that restriction. Returning a Result from a
function is supported. Function equality is not defined.

## Numeric values and literal text

Integers have signed 64-bit values with wrapping arithmetic in every build.
Decimal, `0x`, `0b` and `0o` literals accept checked digit separators and reject
out-of-range magnitudes. Power `**` is right-associative: `2 ** 3 ** 2` is 512.
Unary operators retain the existing precedence, so `-2 ** 2` is 4. Integer powers
require nonnegative exponents and define `0 ** 0` as 1. Float power uses IEEE/libm
behavior without implicit Int conversion.

Bitwise operators are `&&&`, `|||`, `^^^`, `~~~`, `<<<` and `>>>`. Shifts normalize
counts modulo 64; right shifts preserve the sign. Float list membership compares
values: signed zeros compare equal, while NaN never equals another NaN.

Integer division/remainder by zero and negative integer exponents produce a
runtime diagnostic. Called functions and callbacks unwind through deferred
cleanup; the first error is retained if cleanup also fails. Compiled main reports
one error on stderr and exits 1. The REPL reports the same error and retains prior
successful bindings. Runtime faults are distinct from explicitly handled Results.

Triple-quoted strings preserve newline and indentation bytes exactly and support
ordinary escapes and interpolation. Nested `/* ... */` comments are bounded and
must close. `@doc """..."""` attaches literal documentation to the immediately
following function or type. Formatting and documentation preserve that metadata;
Rust doc-test execution remains part of the tooling work. Non-ASCII identifier
spelling is retained exactly, without Unicode normalization.

## Iteration and grouped error handling

```fern
fn main():
    for (index, name) in ["Fern", "Rust"].enumerate():
        println("{index}: {name}")
    for number in 0..=3:
        continue if number == 1
        println(number)
```

Ranges are immutable `Range` values with Int endpoints. `start..end` excludes the
end; `start..=end` includes it. Reversed ranges are empty, and even an inclusive
range ending at the maximum Int does not overflow. A loop evaluates its iterable
once. Lists follow element order, maps yield `(key, value)` in insertion order,
and `List.enumerate(values)` or `values.enumerate()` yields `(index, value)`.
Loop patterns must match every element. Break and continue affect the nearest
loop in the same function; deferred cleanup still waits until function exit.

```fern
fn read_size(path: String) -> Int:
    with
        text <- File.read(path)
    do
        String.len(text)
    else
        Err(_) -> 0
```

With bindings execute sequentially and stop on the first error. Each error type
has a checked exhaustive handler; different steps may have different success
and error types. Handlers use `Err(pattern)` or `_` and see the outer scope,
while later steps and `do` see successful bindings. Guarded arms preserve source
order. Omitting `else` propagates errors under the same Result constraint as `?`.

## Returns and cleanup

```fern
fn describe(value: Option(Int)) -> String:
    defer println("finished")
    let Some(number) = value else:
        return "missing"
    return "negative" if number < 0
    "present"
```

A return exits its nearest function, including an anonymous function. Expressions
and arguments after an executed return do not run. A `let-else` failure branch
must leave the function; successful bindings remain available below the statement.
Condition-only `match:` arms test their Boolean conditions in order and require a
final `_` fallback.

Deferred expressions run in reverse registration order when the function exits.
A defer registered inside an `if` runs only if that branch executes, and waits
until the whole function exits. Immutable values are captured when registered;
the expression and its call arguments execute during cleanup. Return values are
evaluated first. Cleanup must return Unit and cannot use return or `?` to leave its
caller. Each called function and lambda has its own cleanup stack. The REPL also
attempts cleanup after evaluation faults, with a separate bounded work budget.

## Maps and record updates

```fern
fn main():
    let original = %{ "name": "Fern", "stage": "prototype" }
    let updated = Map.put(original, "stage", "development")
    println(Option.unwrap_or(Map.get(updated, "name"), "unknown"))
```

`Map.new`, `get`, `put`, `delete`, `len`, `is_empty`, `contains`, `keys` and
`values` retain semantic types. Empty maps require an annotation or later use
that fixes both key and value types. Values may include records, closures and
Results. Keys are Int, Bool or String; String keys compare by contents.

Updating a key leaves its insertion position unchanged. Duplicate literal keys
keep their last value, and all key/value expressions still execute in source
order. Deletion followed by reinsertion appends the key. Operations preserve
existing aliases. The initial implementation uses linear searches and copies on
updates; it is intended as a correct baseline for later hashing optimization.

Record updates use `%{ point | y: next_y(), x: next_x() }`. The base executes
first, followed by fields in written order. Unmentioned fields retain their
values. Unknown, duplicate or incorrectly typed fields produce diagnostics.

## Interactive and editor tools

`fern-rs repl` evaluates expressions, successful `let` bindings and typed function
or type definitions. Submit indented blocks and continued calls with a blank line. `:help`, `:reset`
and `:quit` control the session. It checks typed IR and retains values without
replaying earlier effects. Core values, matching, tuples, generics, common string
and list APIs, and local file operations execute interactively. Unsupported native
APIs produce a source-facing diagnostic; use `run` for those programs. Evaluation,
input, allocations and value previews are bounded. Closures retain their original
compiled code across later entries; unique retained programs count toward the
interactive storage budget. Session errors preserve prior
bindings; already completed filesystem effects cannot be rolled back.

`fern-rs lsp` serves JSON-RPC on standard input/output. It supports lifecycle,
UTF-16 diagnostics, full/incremental document changes and module-aware checking
against unsaved buffers. Imported errors retain their source URI; dependency edits
recheck callers and clear stale diagnostics. It currently reports the first error
per checked module graph. Go-to-definition follows source bindings, including
shadowed locals, clause parameters, captures and visible imported declarations.
Completion respects lexical scope and module visibility, replaces the current
identifier using UTF-16 ranges, and offers builtin prefixes such as `List.` even
when the surrounding source is incomplete. Comments and plain string contents
do not produce code suggestions.

Navigation rebuilds its bounded source index from current accepted buffers on
each request; it never falls back to stale locations from a previous valid edit.
It requires a parsable source graph but can work before type errors are fixed.
Completion returns at most 256 items and 1 MiB of output; retained editor symbol
names are capped at 8 MiB. Hover, typed record
members, rename and code actions remain subsequent tooling checkpoints.

## Source documentation

`fern-rs doc library.fn` writes Markdown to stdout. Use `--html -o docs.html`
for a standalone HTML page, or `-o docs.md` to save Markdown. Output replacement
is atomic; source files and their symlink/hardlink aliases cannot be overwritten.
`fern-rs doc --help` describes the options.

Documentation uses the parser to retain function clause groups, guards, nested
signatures, type declarations and Unicode names. All declarations are included,
with their original visibility and annotations. Each literal @doc belongs to its
own declaration. Generation requires valid syntax, but no main, backend or code
execution. It does not infer missing signatures. HTML displays documentation as
escaped literal text; Markdown retains authored documentation markup.

The command currently accepts one file, bounded to 1 MiB and 4,096 declarations;
output is limited to 8 MiB. Directory documentation, search and executable doc
tests remain separate tooling checkpoints.

## Architecture

```text
UTF-8 module graph → Rust lexer/parser → qualified source AST
            → name/type checking → typed IR (Type, FunctionId, LocalId)
            → QBE text → fern-qbe process → assembly
            → host C compiler/linker + existing C runtime → executable
```

The emitter consumes only typed IR. It never reconstructs a string's type from
its register width or variable spelling. Int and String both use QBE `l`, while
their semantic types stay distinct. Resolved function/local IDs separate source
names from generated symbols. An exported `fern_main` wrapper adapts Fern's
typed entry function to the runtime's C ABI.

Both Rust entry points forbid unsafe code. Owned enums and `Result` encode syntax
and fallible operations. The QBE adapter is a small C executable around the
vendored backend; its process boundary isolates global state and backend failure.
No C frontend executable or parser is invoked by Rust compilation. Fern's GC and
runtime semantics are unchanged. Lists and Results reuse the C runtime's existing
heap representations. Rust Options also use its heap-backed Result allocation,
tag, and payload helpers internally: Some is Ok and None is Err with unused zero
payload. This preserves full 64-bit integers and pointers. The C compiler's packed
Option ABI truncates payloads to 32 bits. Rust explicitly adapts the lossless byte
payload from `String.char_at`; additional packed APIs require an ABI audit. Semantic
Option and Result types remain distinct in checked IR.

Native tools receive literal argument vectors. Private temporary directories own
intermediate files. Successful builds atomically replace the requested output;
failed checks or backend invocations preserve existing outputs. Source/output
aliases are rejected. Normal install/release recipes continue to package C only.

## Evaluation and maintenance

Run `just rust-check` for format, clippy, Rust tests, and native differential
fixtures. CI runs it on Linux and macOS. Run `just check` for the existing C gates.
Native fixtures specify exact stdout and exit status independently of C. Known C
backend differences are named in the manifest and reported; they do not relax
Rust's expected output. Seeded generated programs exercise the shared subset.

See [migration progress](../docs/RUST_MIGRATION.md),
[initial evaluation results](../docs/RUST_FRONTEND_EVALUATION.md),
[decision 45](../DECISIONS.md), and the [roadmap](../ROADMAP.md).
