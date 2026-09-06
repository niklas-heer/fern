# Building Fern Compiler

## Prerequisites

- C compiler (clang or gcc)
- mise 2026.9.1 or newer (CI pins 2026.9.1)
- Boehm GC development library (`bdw-gc`)
- SQLite development library (`sqlite3`)
- OpenSSL development library (`openssl`)
- `pkg-config` for native library discovery
- Clang 14+ and Bash 3.2+ for the native quality-checker launcher
- Python 3.11+ and `uv` for integration/reference tests and documentation tooling
- macOS, Linux, or other Unix-like OS

Install the native development dependencies:

```sh
# macOS (with Xcode command line tools installed)
brew install mise bdw-gc sqlite openssl pkg-config

# Ubuntu/Debian (install mise using its official installation instructions)
sudo apt-get install clang pkg-config libgc-dev libsqlite3-dev libssl-dev
```

The repository pins Rust 1.75.0, Python 3.14.7 and uv 0.12.5 in `mise.toml`.
Run `mise install`, then `mise run tool-versions`. Review and trust this checkout
when mise requests it; no global configuration or activation hook is needed.
The native packages above are host-managed, not a fully pinned OS image.
See [the task and tool environment](docs/DEVELOPMENT_ENVIRONMENT.md) for lockfiles,
optional runners, MSRV details and the remaining reproducibility boundary.

## Quick Start

```bash
# Build the compiler (debug mode)
mise run debug
# Run tests
mise run test

# Build release version
mise run release

# Clean build artifacts
mise run clean
```

## Build Targets

### Experimental Rust frontend

With Rust 1.75 or newer, Cargo, rustfmt, and clippy installed:

```sh
mise run rust-build
./bin/fern-rs run compiler-rs/tests/corpus/hello.fn
mise run rust-check
```

This builds a separate `fern-rs` compiler and `fern-qbe` backend helper, reusing
`libfern_runtime.a`. Normal build, install, and release commands still use C.
See the [prototype guide](compiler-rs/README.md) for its supported subset and
the [evaluation](docs/RUST_FRONTEND_EVALUATION.md) for measurements and migration criteria.

### Development

- `mise run debug` - Build debug version with symbols and assertions
- `mise run test` - Build and run all tests
- `mise run clean` - Remove all build artifacts

### Production

- `mise run release` - Build optimized release version

### Installation

- `mise run install` - Install fern and `libfern_runtime.a` together under `/usr/local/bin`
- `PREFIX="$HOME/.local" mise run install` - Install locally without administrator privileges
- `DESTDIR=/tmp/package PREFIX=/usr/local mise run install` - Stage an installation for packaging
- `mise run uninstall` - Remove both installed files (use the same `PREFIX`/`DESTDIR`)

### Debugging

- `mise run memcheck` - Run with Valgrind for memory leak detection

### Code Quality

- `mise run fmt` - Format code with clang-format
- `mise run check` - Native build/test/examples/style workflow plus explicit Python integration gates
- `mise run style` - Native style checks without Python or Cargo
- `mise run style-parity` - Compare source-compiled and cached native checkers with Python
- `mise run style-launcher-check` - Native launcher/cache/process infrastructure tests

See [native checker configuration and cache cleanup](docs/NATIVE_STYLE_CHECKER.md).

## Project Structure

```
fern/
├── src/         # Compiler source code
│   └── main.c   # Entry point
├── lib/         # Internal libraries (arena, string, etc.)
├── include/     # Header files
├── tests/       # Test suite
├── build/       # Build artifacts (generated)
├── bin/         # Compiled binaries (generated)
└── examples/    # Example Fern programs
```

## Running the Compiler

```bash
# After building
./bin/fern run examples/tiny_cli.fn
./bin/fern build examples/tiny_cli.fn -o hello
./hello
```

## Running Tests

```bash
mise run test
```

All tests should pass. If any test fails, please report it as a bug.

## Development Workflow

### First Time Setup

```bash
# Install git hooks for automatic quality checks
./scripts/install-hooks.sh
```

This installs a pre-commit hook that automatically:
- Compiles code with strict warnings
- Runs all tests
- Checks for common mistakes (malloc/free, manual unions, etc.)
- Reminds you to update ROADMAP.md

### Daily Development

1. Make changes to source code
2. Run `mise run test` to verify (or rely on pre-commit hook)
3. Update ROADMAP.md to track verified progress
4. Run `mise run check`, then commit (pre-commit hook runs automatically)

**Note:** The pre-commit hook will prevent commits if tests fail or code doesn't compile.

## Compiler Flags

### Debug Build

- `-std=c11` - C11 standard
- `-Wall -Wextra -Wpedantic -Werror` - All warnings as errors
- `-g` - Debug symbols
- `-O0` - No optimization
- `-DDEBUG` - Debug mode defines

### Release Build

- `-std=c11` - C11 standard
- `-Wall -Wextra -Wpedantic -Werror` - All warnings as errors
- `-O2` - Optimization level 2
- `-DNDEBUG` - Release mode (disables asserts)

## Troubleshooting

### "clang: command not found"

Install clang:
```bash
# macOS
xcode-select --install

# Ubuntu/Debian
sudo apt-get install clang

# Fedora
sudo dnf install clang
```

### "mise: command not found"

Install mise using [its official instructions](https://mise.jdx.dev/installing-mise.html),
then run `mise install` from this checkout. On macOS, `brew install mise` is supported.
No shell activation is required for `mise run` or `mise exec`.

### Tests fail

1. Run `mise run clean` to remove stale build artifacts
2. Run `mise run test` again
3. If still failing, check the error message and report a bug

### "ld: cannot find -lsqlite3" (or sqlite link errors)

Install SQLite development headers/libraries:
```bash
# macOS
brew install sqlite

# Ubuntu/Debian
sudo apt-get install libsqlite3-dev

# Fedora
sudo dnf install sqlite-devel
```

## Relocatable installations

`fern` locates `libfern_runtime.a` beside the actual compiler executable, including
when invoked through `PATH` or a symlink. Move both files together. Keep the native
development libraries installed for subsequent compilation. Generated executables
may depend on platform shared libraries; the release is not universally static.

`fern run` uses a private temporary directory, so simultaneous runs cannot collide
with another source file's basename. Build output paths can contain spaces,
quotes, and literal dollar signs.

If a quality check reports a nonexistent linker search directory, inspect the
shell's `LIBRARY_PATH`. Remove stale entries for that invocation (for example,
`env -u LIBRARY_PATH mise run check`); do not suppress compiler warnings globally.

## Next Steps

See [ROADMAP.md](ROADMAP.md) for active priorities and [docs/README.md](docs/README.md) for the full documentation map.
