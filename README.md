# Fern

> A statically-typed, functional language with Python aesthetics that compiles to single binaries.

**Status:** 🚧 In active development - Milestone 4 (Code Generation) in progress, 331 tests passing

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

- **Readability first** - Python-like indentation, no braces
- **Functional-first** - Immutable by default, pure functions
- **No crashes** - No null, no exceptions, no panics - only Result types
- **Predictable** - Explicit error handling, no hidden control flow
- **Dual-purpose** - Same code for CLI tools and servers
- **Single binary** - Deploy one file, no dependencies

## Key Features

- **Static types** with inference - safety without verbosity
- **Pattern matching** - exhaustiveness checking catches bugs
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
  - 331 tests passing

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

```bash
# Build the compiler
make

# Run tests
make test

# Build with debug symbols
make debug

# Check code style (FERN_STYLE compliance)
make style
```

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
