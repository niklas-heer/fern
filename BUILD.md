# Building Fern Compiler

## Prerequisites

- C compiler (clang or gcc)
- Just task runner (`just`)
- Boehm GC development library (`bdw-gc`)
- SQLite development library (`sqlite3`)
- OpenSSL development library (`openssl`)
- `pkg-config` for native library discovery
- Python 3.11+ and `uv` for quality/documentation tooling
- macOS, Linux, or other Unix-like OS

Install the native development dependencies:

```sh
# macOS (with Xcode command line tools installed)
brew install just bdw-gc sqlite openssl pkg-config uv

# Ubuntu/Debian (install just and uv using their supported installers if unavailable)
sudo apt-get install clang pkg-config libgc-dev libsqlite3-dev libssl-dev python3
```

## Quick Start

```bash
# Build the compiler (debug mode)
just debug
# Run tests
just test

# Build release version
just release

# Clean build artifacts
just clean
```

## Build Targets

### Development

- `just debug` - Build debug version with symbols and assertions
- `just test` - Build and run all tests
- `just clean` - Remove all build artifacts

### Production

- `just release` - Build optimized release version

### Installation

- `just install` - Install fern and `libfern_runtime.a` together under `/usr/local/bin`
- `PREFIX="$HOME/.local" just install` - Install locally without administrator privileges
- `DESTDIR=/tmp/package PREFIX=/usr/local just install` - Stage an installation for packaging
- `just uninstall` - Remove both installed files (use the same `PREFIX`/`DESTDIR`)

### Debugging

- `just memcheck` - Run with Valgrind for memory leak detection

### Code Quality

- `just fmt` - Format code with clang-format
- `just check` - Full build/test/examples/style gate (recommended before commits)

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
just test
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
2. Run `just test` to verify (or rely on pre-commit hook)
3. Update ROADMAP.md to track verified progress
4. Run `just check`, then commit (pre-commit hook runs automatically)

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

### "just: command not found"

Install just:
```bash
# macOS
brew install just

# Ubuntu/Debian
sudo apt-get install just

# Fedora
sudo dnf install just
```

### Tests fail

1. Run `just clean` to remove stale build artifacts
2. Run `just test` again
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
`env -u LIBRARY_PATH just check`); do not suppress compiler warnings globally.

## Next Steps

See [ROADMAP.md](ROADMAP.md) for active priorities and [docs/README.md](docs/README.md) for the full documentation map.
