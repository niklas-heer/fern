# Building Fern Compiler

## Prerequisites

- C compiler (clang or gcc)
- Make
- macOS, Linux, or other Unix-like OS

## Quick Start

```bash
# Build the compiler (debug mode)
make

# Run tests
make test

# Build release version
make release

# Clean build artifacts
make clean
```

## Build Targets

### Development

- `make` or `make debug` - Build debug version with symbols and assertions
- `make test` - Build and run all tests
- `make clean` - Remove all build artifacts

### Production

- `make release` - Build optimized release version

### Installation

- `make install` - Install fern to `/usr/local/bin`
- `make uninstall` - Remove installed fern

### Debugging

- `make memcheck` - Run with Valgrind for memory leak detection

### Code Quality

- `make fmt` - Format code with clang-format

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
./bin/fern examples/hello.fn
```

## Running Tests

```bash
make test
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
2. Run `make test` to verify (or rely on pre-commit hook)
3. Commit changes (pre-commit hook runs automatically)
4. Update ROADMAP.md to track progress

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

### "make: command not found"

Install make:
```bash
# macOS
xcode-select --install

# Ubuntu/Debian
sudo apt-get install build-essential

# Fedora
sudo dnf groupinstall "Development Tools"
```

### Tests fail

1. Run `make clean` to remove stale build artifacts
2. Run `make test` again
3. If still failing, check the error message and report a bug

## Next Steps

See [ROADMAP.md](ROADMAP.md) for the implementation plan and next milestones.
