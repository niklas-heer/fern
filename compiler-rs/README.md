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
literal arguments, although this subset has no argument-reading builtin yet.

## Supported language

- Top-level functions, typed parameters, explicit return annotations, forward and
  recursive calls. Parameterless `main` returns `Int` or `Unit`; omitted `main`
  return type means `Unit`. Other functions require a return annotation.
- Signed 64-bit `Int`, `Bool`, UTF-8 `String`, `Unit`/`()`, and recursively nested
  concrete `List(T)`, `Option(T)`, and `Result(T, E)` types.
- Immutable `let` with inferred or annotated type, lexical scopes, and shadowing.
- Integer arithmetic, comparisons, string addition/equality, boolean operators
  with short-circuit evaluation, and unary `-`/`not`.
- Inline and indented `if`/`else`, including value-producing branches. An `if`
  without `else` has type `Unit`.
- `print`, `println`, `String.concat`, `String.eq`, and `String.len`.
- Immutable list literals, `List.len/get/head/tail/is_empty/push/reverse/concat`,
  and scalar `List.contains` (Int, Bool, or String elements).
- `Some`, `None`, `Ok`, and `Err`, with payload types inferred from bindings,
  function signatures, calls, and branches. Empty lists also use this context.
- Exhaustive `match` on scalar literals, Bool, Option, or Result, with wildcard
  and name catchalls, constructor payload bindings, and scoped multiline arms.
- Postfix `?` unwraps Ok or immediately returns Err from a Result-returning helper;
  the enclosing error type must agree. Result expressions cannot be silently
  discarded. Main still returns Int or Unit and must explicitly handle errors.
- `Option.is_some/is_none/unwrap_or` and `Result.is_ok/is_err/unwrap_or`.
- Multiline lists, calls, and type arguments with checked delimiters.
- Line comments, string escapes, and source-located diagnostics.

Unsupported syntax produces diagnostics. Gaps include modules/imports, public
declarations, user-defined generics and algebraic types, maps/tuples/records,
nested constructor patterns, match guards, closures, pipelines, actors, the wider
standard library, multiline strings, interpolation, block comments, named arguments,
non-ASCII identifiers, and inferred return types outside `main`. There is no
formatter, REPL, package manager, or LSP in this prototype.

An expression such as `let empty = []` or `let missing = None` needs enough later
usage or an annotation to determine its payload type. For example,
`let empty: List(String) = []` is concrete. Unsupported compound equality is
rejected; it does not silently compare pointers. List indexing and `List.head`
retain the current runtime's valid-index/nonempty preconditions, so check lengths
before reading. General safe collection indexing and higher-order operations
remain future work.

Strings inherit the existing C runtime: embedded NUL is rejected, and `String.len`
counts UTF-8 bytes. Integer arithmetic uses QBE's native signed operations; total
arithmetic error handling (including divide by zero) remains future work. A source
file is limited to 1 MiB, 65,536 tokens, and a syntax nesting depth of 128. These
explicit prototype limits prevent unbounded parser recursion/allocation.

## Architecture

```text
UTF-8 source → Rust lexer/parser → source AST
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
Option ABI truncates payloads to 32 bits, so Rust does not call those Option APIs;
future C APIs returning packed Options will need explicit adapters. Semantic
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
