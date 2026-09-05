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

- Top-level functions, typed parameters, explicit return annotations, forward and
  recursive calls. Parameterless `main` returns `Int` or `Unit`; omitted `main`
  return type means `Unit`. Other functions require a return annotation.
- Signed 64-bit `Int`, IEEE double `Float`, `Bool`, UTF-8 `String`, `Unit`/`()`, and recursively nested
  concrete `List(T)`, `Option(T)`, and `Result(T, E)` types.
- Custom sum and record declarations, concrete generic type applications, record fields,
  and generic functions specialized for their concrete call types.
- Module declarations, public functions/types, qualified/aliased/selected imports,
  and public reexports from `module.fn` or `module/mod.fn`.
- Immutable `let` with inferred or annotated type, lexical scopes, and shadowing.
- Structural tuple types/literals, nested tuple patterns/destructuring, and `.0` field access.
- Standard and placeholder pipes evaluate their input exactly once before other arguments.
- Integer/Float arithmetic and comparisons, string addition/equality, boolean operators
  with short-circuit evaluation, and unary `-`/`not`.
- Inline and indented `if`/`else`, including value-producing branches. An `if`
  without `else` has type `Unit`.
- Early returns, postfix conditionals, condition-only matches and `let-else` unwrapping.
- Dynamic `defer` cleanup runs at function exit, including early return and `?` propagation.
- `print`, `println`, `String.concat`, `String.eq`, and `String.len`.
- Immutable list literals, `List.len/get/head/tail/is_empty/push/reverse/concat`,
  and scalar `List.contains` (Int, Bool, or String elements).
- `Some`, `None`, `Ok`, and `Err`, with payload types inferred from bindings,
  function signatures, calls, and branches. Empty lists also use this context.
- Exhaustive `match` on scalar literals, Bool, Option, or Result, with wildcard
  and name catchalls, recursively nested constructor patterns, guards, and scoped multiline arms.
  Guarded arms do not establish exhaustiveness.
- Postfix `?` unwraps Ok or immediately returns Err from a Result-returning helper;
  the enclosing error type must agree. Result expressions cannot be silently
  discarded. Main still returns Int or Unit and must explicitly handle errors.
- `Option.is_some/is_none/unwrap_or` and `Result.is_ok/is_err/unwrap_or`.
- Multiline lists, calls, and type arguments with checked delimiters.
- Nested string interpolation with Int/Float/Bool/String expressions and escaped literal braces.
- Immutable Map values with Int/Bool/String keys, ordered inspection and optional lookup;
  record updates that evaluate their base and fields once.
- First-class named/anonymous functions, contextual callback inference, escaping captures
  and typed higher-order List/Option/Result operations.
- Line comments, string escapes, and source-located diagnostics.
- Audited runtime bindings for files, strings, regex, arguments/processes, SQLite,
  HTTP clients, explicit mailboxes and terminal UI. Opaque annotations are qualified
  (`Tui.Panel`, `Tui.Tree`, etc.), keeping ordinary user-defined names available.

Unsupported syntax produces diagnostics. Gaps include actor execution, triple-quoted multiline strings, block comments, named
arguments, non-ASCII identifiers, and inferred return types outside `main`.
`with`, collection/range iteration and full release parity remain migration work.

`fmt` formats the supported syntax in place, preserves comments, and verifies that
the complete syntax tree remains equivalent before replacing the file atomically.
Comments inside multiline arguments may move adjacent to their statement. Invalid
source remains untouched. Generic bodies are checked at concrete instantiation;
unused generic bodies currently receive structural/signature validation only.

An expression such as `let empty = []` or `let missing = None` needs enough later
usage or an annotation to determine its payload type. For example,
`let empty: List(String) = []` is concrete. Unsupported compound equality is
rejected; it does not silently compare pointers. List indexing and `List.head`
retain the current runtime's valid-index/nonempty preconditions, so check lengths
before reading. General safe collection indexing remains future work.

Strings inherit the existing C runtime: embedded NUL is rejected, and `String.len`
counts UTF-8 bytes. Integer arithmetic uses QBE's native signed operations; total
arithmetic error handling (including divide by zero) remains future work. A source
file is limited to 1 MiB, 65,536 tokens, and a syntax nesting depth of 128. These
explicit prototype limits prevent unbounded parser recursion/allocation.

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
per checked module graph and advertises diagnostics/synchronization only.

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
