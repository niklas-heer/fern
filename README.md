# 🌿 Fern

> A statically-typed, functional language with Python aesthetics that compiles to single binaries.

**Status:** 🚧 In active development - Gate D complete, 504 tests passing, release packaging/benchmark publishing in CI, and a new `Justfile` task runner

## What is Fern?

Fern is a programming language designed to make both **fast CLI tools** (<1 MB) and **full-stack applications** (2-4 MB) with the same elegant syntax. Compiler outputs are standalone and behavior is designed to stay predictable.

```fern
# Clean, readable syntax with explicit error handling
let content = read_file("config.txt")?
let config = parse_config(content)?
let validated = validate(config)?
Ok(process(validated))
```

## Design Philosophy

**Fern should spark joy** - for those familiar with functional programming, writing Fern should feel delightful, not tedious.

**One obvious way** - There should be one clear, idiomatic way to accomplish any task. No agonizing over which of five approaches to use.

**No surprises** - The language actively prevents the bugs that waste your afternoon. If it compiles, it probably works.

**Jetpack surface included** - Core modules ship with the compiler; SQL now has a concrete SQLite runtime backend, and HTTP GET/POST calls are backed by the civetweb runtime client.

### Core Principles

| Principle | How Fern Delivers |
|-----------|------------------|
| **Readable** | Python-like indentation, no braces, clear keywords |
| **Functional** | Immutable by default, pure functions, pattern matching |
| **Safe** | No null, no exceptions, no panics - only Result/Option types |
| **Predictable** | Explicit error handling, no hidden control flow, exhaustive matching |
| **Fast** | Compiles to native code, ~35x faster than Python |
| **Portable** | Single-binary distribution with embedded toolchain/runtime |

### What We Prevent

```
✗ NullPointerException     → Option type, exhaustive matching
✗ Unhandled exceptions     → Result type, must handle errors
✗ Race conditions          → Actors with isolated heaps, no shared state
✗ "It works on my machine" → Single binary, all dependencies included
✗ Silent failures          → Compiler enforces error handling
✗ Action at a distance     → Immutability prevents spooky mutation
```

## Key Features

- **Static types** with inference - safety without verbosity
- **Pattern matching** - exhaustiveness checking catches bugs
- **Garbage collected runtime** - Boehm-backed runtime plus Perceus baseline ownership primitives
- **Language tooling** - LSP with diagnostics, hover, definition, completion, rename, and code actions
- **Stable stdlib API surface** - `fs`, `http`, `json`, `sql`, `actors`, and `File` alias compatibility
- **Explicit runtime readiness** - SQL is SQLite-backed today; HTTP GET/POST are runtime-backed (HTTP URLs) with deterministic `Err(FERN_ERR_IO)` on invalid URLs/network failures
- **Helpful diagnostics** - snippets, notes, and fix hints in CLI workflows
- **Reproducible quality gates** - `just check`, fuzz smoke, perf budgets, release policy checks

## Module Naming

Canonical module naming in docs/examples:

- `String`, `List`, `System`, `Regex`, `Result`, `Option`, and `Tui.*`
- `fs`, `json`, `http`, `sql`, and `actors`
- `File.*` remains supported as a compatibility alias for `fs.*`

## Current Status

Fern is implemented with strict TDD. See [DESIGN.md](DESIGN.md) for language details and [ROADMAP.md](ROADMAP.md) for gate-by-gate status.

**Execution gates:**
- ✅ Gate A (DX + language feel) passed
- ✅ Gate B (reliability + regression resistance) passed
- ✅ Gate C (stdlib/runtime surface quality) passed
- ✅ Gate D (ecosystem/adoption hardening) passed

**Recent outcomes:**
- ✅ 506/506 tests passing in local `just test`
- ✅ Cross-platform CI (Ubuntu + macOS) with build/test/style/perf/fuzz/example checks
- ✅ Release packaging bundles (`fern` + `libfern_runtime.a` + policy/docs artifacts)
- ✅ Conventional-commit-driven semver + release notes via `release-please`
- ✅ Published reproducible benchmark + case-study report in `docs/reports/benchmark-case-studies-2026-02-06.md`
- ✅ LSP support beyond MVP (completion, rename, code actions, better source positions)

**Active focus:**
- 🚧 Post-Gate D stabilization and milestone polish (see [ROADMAP.md](ROADMAP.md))

## Documentation

- [Language Design](DESIGN.md) - Complete specification
- [Implementation Roadmap](ROADMAP.md) - Development plan and progress
- [Decision Log](DECISIONS.md) - Architectural decisions
- [Coding Standards](FERN_STYLE.md) - TigerBeetle-inspired style guide
- [Development Guidelines](CLAUDE.md) - For AI-assisted development
- [Compatibility Policy](docs/COMPATIBILITY_POLICY.md) - Upgrade/deprecation guarantees

## Inspiration

Fern takes inspiration from the best features of:
- **Gleam** - Type system, simplicity
- **Elixir** - Pattern matching, actors, pragmatic design
- **Rust** - `?` operator, Result types
- **Zig** - Comptime, defer, minimalism
- **Python** - Readability, aesthetics
- **Go** - Single binary deployment

## Philosophy in Action

**Full-stack shape (stable API surface, SQL + HTTP runtime backends available):**
```fern
# No Redis, no RabbitMQ, no separate database
fn main() -> Result((), Int):
    let db = sql.open("app.db")?            # SQLite-backed runtime handle
    let cache = spawn(cache_actor)          # In-memory cache (actor)
    let queue = spawn(job_queue)            # Job queue (actor)

    http.serve(8080, handler(db, cache, queue))

# One 3.5MB binary, no external dependencies
```

**CLI tools stay tiny:**
```fern
# Fast, small, no runtime
fn main() -> Result((), Int):
    let data = fs.read("data.csv")?
    let processed = process(data)?
    fs.write("output.csv", processed)?
    Ok(())

# 600KB binary, <5ms startup
```

## Building

**Dependencies (for building the compiler):**
```bash
# macOS
brew install bdw-gc sqlite

# Ubuntu/Debian
apt install libgc-dev libsqlite3-dev

# Fedora
dnf install gc-devel sqlite-devel
```

Also install the task runner:
```bash
# macOS
brew install just

# Ubuntu/Debian
apt install just

# Fedora
dnf install just
```

> **Note:** QBE is embedded in the compiler - no external `qbe` binary needed. Boehm GC is statically linked into compiled programs. SQL stdlib calls link against SQLite (`sqlite3`) and HTTP stdlib calls are implemented via vendored civetweb client code in the runtime.

**Preferred task runner (`Justfile`):**
```bash
just debug
just test
just check
just docs
just docs-check
just release-package
just benchmark-report
```

`Justfile` is the primary developer entrypoint for all build and quality tasks.

## Release Automation

Fern uses [release-please](https://github.com/googleapis/release-please) with conventional commits to drive release PRs, semver bumps, and changelog notes automatically.
The initial release baseline is pinned to `0.1.0` (not `1.0.0`).

- `fix:` commits trigger patch bumps
- `feat:` commits trigger minor bumps
- `feat!:` or `BREAKING CHANGE:` triggers minor bumps while `<1.0.0` (and major bumps once `>=1.0.0`)
- Release notes/changelog entries are generated from conventional-commit history

The workflow in `.github/workflows/release-please.yml` requires `RELEASE_PLEASE_TOKEN` to be set in repo secrets to open/update release PRs.

## FAQ

### Why "Fern"?

Ferns are ancient, resilient plants that have survived for over 350 million years. Like the plant, Fern the language is designed to be:

- **Resilient** - No crashes, no nulls, explicit error handling
- **Elegant** - Simple fronds (syntax) that unfold into complex patterns
- **Evergreen** - Clean fundamentals that age well

Plus, it's short, memorable, and wasn't taken.

### What's with the 🌿 emoji?

The fern emoji (🌿) is Fern's visual identity. You can even use it as a file extension:

```bash
# Both work!
fern build hello.fn
fern build hello.🌿
```

Why? Because we can, and it makes your file explorer more interesting.

### Is Fern production-ready?

Not yet. Core compiler/runtime/tooling paths are heavily tested and Gate D is complete, but Fern is still pre-1.0 and evolving. See [ROADMAP.md](ROADMAP.md) and [docs/COMPATIBILITY_POLICY.md](docs/COMPATIBILITY_POLICY.md) for current guarantees and planned work.

## License

MIT License - see [LICENSE](LICENSE) for details.

**What this means:** You can use Fern for anything (personal, commercial, proprietary) without restrictions. We want Fern to be as widely useful as possible.

## Contributing

We're actively implementing the compiler using AI-assisted test-driven development. See [CLAUDE.md](CLAUDE.md) for development workflow and guidelines.

```bash
# Build and test
just test

# Check style compliance
just style

# See current progress
cat ROADMAP.md
```
