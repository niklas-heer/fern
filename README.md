# 🌿 Fern

> A statically-typed, functional language with Python aesthetics that compiles to single binaries.

**Status:** 🚧 In active development - Milestone 4 (Code Generation) in progress, 346 tests passing

## What is Fern?

Fern is a programming language designed to make both **fast CLI tools** (<1 MB) and **full-stack applications** (2-4 MB) with the same elegant syntax. No runtime dependencies, no crashes, predictable behavior.

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

**Jetpack included** - Everything you need is built-in: actors, databases, HTTP, CLI tools, TUI. Like Bun, but compiled.

### Core Principles

| Principle | How Fern Delivers |
|-----------|------------------|
| **Readable** | Python-like indentation, no braces, clear keywords |
| **Functional** | Immutable by default, pure functions, pattern matching |
| **Safe** | No null, no exceptions, no panics - only Result/Option types |
| **Predictable** | Explicit error handling, no hidden control flow, exhaustive matching |
| **Fast** | Compiles to native code, ~35x faster than Python |
| **Portable** | Single binary, no runtime dependencies |

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
- **Garbage collected** - automatic memory management (Boehm GC)
- **Actor concurrency** - Erlang/Elixir-style lightweight processes
- **Embedded database** - libSQL included (SQLite-compatible)
- **Helpful errors** - compiler guides you to solutions
- **Doc tests** - examples in docs are automatically tested

## Current Status

We're implementing the compiler using test-driven development. See [DESIGN.md](DESIGN.md) for the complete language specification.

**Completed:**
- ✅ Milestone 0: Project Setup
  - Build system and test infrastructure
  - Safety libraries (arena, strings, collections)
  - Development workflow and pre-commit hooks
  - CI/CD pipeline
- ✅ Milestone 1: Lexer
  - All keywords, operators, literals
  - `?` operator for Result propagation
  - Block comments (`/* */`)
- ✅ Milestone 2: Parser
  - Expression parsing with Pratt precedence
  - Statement parsing (let, fn, return, if, match, for)
  - Pattern matching, type annotations
  - Function definitions with clauses
- ✅ Milestone 3: Type Checker
  - Hindley-Milner type inference
  - Generic types, Result/Option handling
  - Pipe operator type checking

**In Progress:**
- 🚧 Milestone 4: Code Generation
  - QBE IR generation from AST
  - Runtime library (Result, List, String operations)
  - Automatic garbage collection (Boehm GC)
  - 346 tests passing

**Planned:**
- FernFuzz: Grammar-based fuzzing for compiler testing
- FernDoc: HexDocs-style documentation generation
- See [ROADMAP.md](ROADMAP.md) for full plan

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

**Replace your infrastructure:**
```fern
# No Redis, no RabbitMQ, no separate database
fn main() -> Result[(), Error]:
    let db = sql.open("app.db")?           # Embedded database
    let cache = spawn(cache_actor)          # In-memory cache (actor)
    let queue = spawn(job_queue)            # Job queue (actor)

    http.serve(8080, handler(db, cache, queue))

# One 3.5MB binary, no external dependencies
```

**CLI tools stay tiny:**
```fern
# Fast, small, no runtime
fn main() -> Result[(), Error]:
    let data = read_file("data.csv")?
    let processed = process(data)?
    write_file("output.csv", processed)?
    Ok(())

# 600KB binary, <5ms startup
```

## Building

**Dependencies (for building the compiler):**
```bash
# macOS
brew install bdw-gc

# Ubuntu/Debian
apt install libgc-dev

# Fedora
dnf install gc-devel
```

> **Note:** QBE is embedded in the compiler - no external `qbe` binary needed. Boehm GC is statically linked into compiled programs. The fern binary and compiled programs are **fully standalone** - no runtime dependencies, just like Go.

**Build the compiler:**
```bash
make           # Release build
make debug     # Debug build with symbols
make test      # Run test suite (346 tests)
make style     # Check FERN_STYLE compliance
```

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

Not yet. We're in active development (Milestone 4 of 9). See [ROADMAP.md](ROADMAP.md) for current progress. The compiler is being built with extensive testing, including planned deterministic simulation testing for the actor system.

## License

MIT License - see [LICENSE](LICENSE) for details.

**What this means:** You can use Fern for anything (personal, commercial, proprietary) without restrictions. We want Fern to be as widely useful as possible.

## Contributing

We're actively implementing the compiler using AI-assisted test-driven development. See [CLAUDE.md](CLAUDE.md) for development workflow and guidelines.

```bash
# Build and test
make test

# Check style compliance
make style

# See current progress
cat ROADMAP.md
```
