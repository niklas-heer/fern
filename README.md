<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/fern-logo-light.png">
    <img src="docs/assets/fern-logo.png" alt="Fern logo: a geometric fern frond" width="128" height="128">
  </picture>
</p>

<h1 align="center">Fern</h1>

<p align="center">
  <strong>Readable code. Native programs.</strong><br>
  A statically typed, functional language with Python-like syntax.
</p>

Fern combines immutable values, pattern matching and explicit errors with
indentation-based syntax. Write a small program, check its types, and compile it
to a native executable.

```fern
fn greet(name: String) -> String:
    "Hello, {name}!"

fn main():
    let language = "Fern"
    println(greet(language))
```

**Early preview.** Fern is pre-1.0 and actively evolving. You can run native
programs today, but the complete language design is still being implemented.
[What's ready?](docs/RELEASE_READINESS.md) · [What's next?](ROADMAP.md)

## Why Fern?

- **Familiar syntax, functional foundations.** Indentation, immutable bindings,
  inferred local types and functions that return their last expression.
- **Errors you can see.** `Option`, `Result`, pattern matching and `?` keep
  missing values and fallible operations explicit.
- **Native executables.** Compile through QBE and the native runtime, with
  garbage collection handling memory allocation.
- **Useful libraries.** Work with files, HTTP/HTTPS clients, SQLite and terminal
  interfaces without assembling a separate framework.
- **Tools for everyday work.** Type checking, formatting, a REPL, documentation
  generation and [editor support](editor/zed-fern/README.md).

## Try Fern

Install [mise and the native build dependencies](BUILD.md), then:

```sh
git clone https://github.com/niklas-heer/fern.git
cd fern
mise install
mise run debug
./bin/fern run examples/tiny_cli.fn
```

The program prints `hello, fern`. To build and run an executable:

```sh
./bin/fern build examples/tiny_cli.fn -o hello
./hello
```

Follow the [language guide](docs/LANGUAGE_GUIDE.md) to write your own functions,
work with lists and handle errors. Its examples are checked against exact output
in the test suite. The [build guide](BUILD.md) covers local installation and
platform requirements; native builds need the host C compiler and the GC,
SQLite and OpenSSL libraries listed there.

### Try the Rust frontend

Fern is moving to Rust. The expanded frontend is available explicitly while the
C compiler remains the default:

```sh
mise run rust-build
./bin/fern-rs run examples/tiny_cli.fn
```

Mise selects the project's dated Rust nightly and required components. See the
[Rust frontend guide](compiler-rs/README.md) for supported syntax and commands,
or [package a relocatable preview](docs/RUST_PREVIEW_PACKAGING.md) to try it
outside the source checkout.

## Explore by example

| Example | What it shows |
| --- | --- |
| [Tiny CLI](examples/tiny_cli.fn) | Functions, strings and command dispatch |
| [HTTP errors](examples/http_api.fn) | Explicit error handling, with no network access required |
| [Terminal project view](examples/tui_project.fn) | Structured trees and logs |
| [Actor mailboxes](examples/actor_app.fn) | The default compiler's explicit mailbox operations |
| [Rust actors](compiler-rs/tests/actors/receive_continues.fn) | Typed messages and native receive continuations; use `fern-rs` |

## Where the project stands

Default builds use QBE and the native runtime. The Rust frontend adds
features including validating JSON, derived codecs and
[bounded typed actor execution](docs/RUST_ACTORS.md). The default C frontend
retains its legacy JSON compatibility API and explicit mailbox primitives.

Generalized actor suspension, typed supervision, actor REPL/FernSim parity and
HTTP serving remain open. An opt-in
[Cranelift backend](docs/BACKEND_REASSESSMENT.md) now emits native objects through
the shared compiler pipeline; QBE remains the default while acceptance continues.
The [readiness checklist](docs/RELEASE_READINESS.md)
defines the current feature boundaries; [DESIGN.md](DESIGN.md) also includes
planned features. Syntax and APIs are subject to the
[compatibility policy](docs/COMPATIBILITY_POLICY.md).

## Learn more

- [Language guide](docs/LANGUAGE_GUIDE.md) — write and run your first programs.
- [Standard library](docs/STDLIB_API_REFERENCE.md) — explore built-in modules.
- [Documentation index](docs/README.md) — find the deeper reference material.
- [Roadmap](ROADMAP.md) — see completed work and remaining milestones.
- [Development environment](docs/DEVELOPMENT_ENVIRONMENT.md) — tool pins, checks
  and optional developer tools.

## Contribute

Small examples, focused bug reports, documentation improvements and compiler
changes are welcome. Start with the [roadmap](ROADMAP.md) and the
[test-first contribution workflow](CLAUDE.md).

```sh
mise run check           # Native build, tests, examples and strict style
mise run rust-check      # Rust compiler and native integration checks
mise run docs-check      # Documentation links, generation and examples
```

CI covers Linux and macOS. Tests exercise native output, relocated packages,
unusual paths, resource limits and failure cleanup. See the
[development guide](docs/DEVELOPMENT_ENVIRONMENT.md) for lint, fuzzing,
benchmark and feedback tools.

Fern takes inspiration from Gleam, Elixir, Rust, Zig, Python and Go.
Released under the [MIT License](LICENSE).
