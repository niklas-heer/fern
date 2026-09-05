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
- Signed 64-bit `Int`, `Bool`, UTF-8 `String`, and the `Unit` type.
- Immutable `let` with inferred or annotated type, lexical scopes, and shadowing.
- Integer arithmetic, comparisons, string addition/equality, boolean operators
  with short-circuit evaluation, and unary `-`/`not`.
- Inline and indented `if`/`else`, including value-producing branches. An `if`
  without `else` has type `Unit`.
- `print`, `println`, `String.concat`, `String.eq`, and `String.len`.
- Line comments, string escapes, and source-located diagnostics.

Unsupported syntax produces diagnostics. Gaps include modules/imports, public
declarations, generics, lists/maps/tuples/records, algebraic types, pattern matching,
Result propagation, closures, pipelines, actors, the wider standard library,
multiline expressions/strings, interpolation, block comments, named arguments,
non-ASCII identifiers, and inferred return types outside `main`. There is no
formatter, REPL, package manager, or LSP in this prototype.

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
runtime semantics are unchanged.

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

See [evaluation results](../docs/RUST_FRONTEND_EVALUATION.md),
[decision 45](../DECISIONS.md), and the [roadmap](../ROADMAP.md).
