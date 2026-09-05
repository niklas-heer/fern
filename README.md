# 🌿 Fern

> A statically typed, functional language with Python-like syntax and native compilation.

**Status:** Pre-1.0, in active development. Native CLI programs, core libraries,
editor tooling, and deterministic actor mailbox primitives are implemented.
The full language design is not finished. See [release readiness](docs/RELEASE_READINESS.md)
for the executable feature boundary and [ROADMAP.md](ROADMAP.md) for remaining work.

An independent [Rust frontend prototype](compiler-rs/README.md) evaluates a typed
compiler pipeline with the existing QBE backend and C runtime. See the
[evaluation results](docs/RUST_FRONTEND_EVALUATION.md); C remains the default.

```fern
fn greet(name: String) -> String:
    String.concat("Hello, ", name)

fn main():
    println(greet("Fern"))
```

## Try it

Install the [build dependencies](BUILD.md), then:

```sh
just debug
./bin/fern run examples/tiny_cli.fn
./bin/fern build examples/tiny_cli.fn -o hello
./hello
```

Expected output from the program: `hello, fern`.

Follow the [language guide](docs/LANGUAGE_GUIDE.md) for functions, immutable
values, lists, errors, and the edit/check/run workflow. Its programs run with
exact output assertions in the test suite.

To install under your home directory:

```sh
PREFIX="$HOME/.local" just install
export PATH="$HOME/.local/bin:$PATH"
fern --help
```

The installation includes `fern` and `libfern_runtime.a` in the same directory.
Keep both files together when moving a bundle. QBE is embedded, so a separate
QBE executable is unnecessary. Native compilation still needs the host C
compiler and the GC, SQLite, and OpenSSL libraries documented in [BUILD.md](BUILD.md).

## Why Fern?

Fern aims to make functional programming feel natural: readable indentation,
immutable values, explicit errors, and one clear way to express a task. Its
long-term design spans small CLI tools and concurrent applications with built-in
services. Working examples and native execution tests guide implementation.

What you can use today:

- Static type checking, inference for local values, functions, lists, strings,
  conditionals, and Result-based library errors.
- Native compilation through embedded QBE and a Boehm GC runtime.
- Filesystem operations, HTTP/HTTPS GET and POST, and SQLite open/execute calls.
- Terminal styling, panels, tables, editable input/password prompts, cursor
  controls, immutable trees, and deterministic log formatting.
- Explicit actor FIFO mailboxes with lifecycle, monitoring, and deterministic
  supervision policies. These are foundations for the future execution model.
- CLI diagnostics, formatter, REPL, LSP, and generated editor support.

Features in [DESIGN.md](DESIGN.md) can still be planned. In particular, spawned
Fern functions do not execute as concurrent actors, HTTP serving is absent,
and the current JSON compatibility API copies strings rather than validating
JSON. Native compilation rejects unsupported actor execution syntax with a
clear diagnostic. Read the [actor contract](docs/ACTOR_RUNTIME.md) and
[readiness checklist](docs/RELEASE_READINESS.md) before building on those areas.

## Modules and examples

Core modules use `String`, `List`, `System`, `Regex`, `Result`, `Option`, and
`Tui.*`. Service modules use `fs`, `json`, `http`, `sql`, and `actors`.
`File.*` remains a compatibility alias for `fs.*`.

- [Tiny CLI](examples/tiny_cli.fn): string output and command dispatch.
- [Actor mailboxes](examples/actor_app.fn): enqueue and explicitly consume jobs.
- [HTTP errors](examples/http_api.fn): deterministic error handling without network access.
- [Terminal project view](examples/tui_project.fn): structured trees and logs.
- [Stdlib reference](docs/STDLIB_API_REFERENCE.md): current module signatures.

## Develop and verify

```sh
just check                 # Clean build, unit/native tests, examples, strict style
just style-parity          # Native/reference diagnostic parity
just docs-check            # Documentation generation and doc examples
just fuzz-smoke            # Reproducible parser/formatter fuzzing
just perf-budget           # Measured release build/startup/size budgets
just release-package       # Compiler/runtime bundle and checksum
```

CI covers Linux and macOS. The tests include relocated installations, unusual
file paths, exact program output, pseudo-terminal interaction, and seeded actor
failure scenarios. Python remains the reference quality checker until the
entire native checker workflow reaches parity.

The complete release checklist is in [release readiness](docs/RELEASE_READINESS.md).
Releases use conventional commits and `release-please`, starting from the
`0.1.0` baseline. The release workflow requires its configured repository token.

## Documentation

- [Documentation Index](docs/README.md)
- [Language Guide](docs/LANGUAGE_GUIDE.md)
- [Build Guide](BUILD.md)
- [Language Design](DESIGN.md)
- [Implementation Roadmap](ROADMAP.md)
- [Decision Log](DECISIONS.md)
- [Coding Standards](FERN_STYLE.md)
- [Development Guidelines](CLAUDE.md)
- [Compatibility Policy](docs/COMPATIBILITY_POLICY.md)

Fern takes inspiration from Gleam, Elixir, Rust, Zig, Python, and Go. Contributions
follow the test-first workflow in [CLAUDE.md](CLAUDE.md).

MIT License — see [LICENSE](LICENSE).
