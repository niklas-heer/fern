# Fern Compiler - Makefile
# A statically-typed, functional language that compiles to single binaries

CC = clang
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -Ilib -Ideps/qbe -Ideps/linenoise
DEBUGFLAGS = -g -O0 -DDEBUG
RELEASEFLAGS = -O2 -DNDEBUG
LDFLAGS =

# QBE compiler flags (embedded backend, more lenient warnings for external code)
QBE_CFLAGS = -std=c99 -Ideps/qbe -Wno-unused-parameter -Wno-sign-compare

# Linenoise flags (external library, lenient warnings)
LINENOISE_CFLAGS = -std=c11 -Ideps/linenoise -Wno-unused-parameter -Wno-missing-field-initializers

# Boehm GC for runtime (automatic garbage collection)
# Install with: brew install bdw-gc (macOS) or apt install libgc-dev (Linux)
GC_CFLAGS = $(shell pkg-config --cflags bdw-gc 2>/dev/null || echo "")
GC_LDFLAGS = $(shell pkg-config --libs bdw-gc 2>/dev/null || echo "-lgc")

# Directories
SRC_DIR = src
TEST_DIR = tests
LIB_DIR = lib
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
RUNTIME_DIR = runtime
QBE_DIR = deps/qbe

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
LIB_SRCS = $(wildcard $(LIB_DIR)/*.c)
RUNTIME_SRCS = $(wildcard $(RUNTIME_DIR)/*.c)

# QBE source files (embedded compiler backend)
QBE_COMMON_SRCS = $(QBE_DIR)/main.c $(QBE_DIR)/util.c $(QBE_DIR)/parse.c \
                  $(QBE_DIR)/abi.c $(QBE_DIR)/cfg.c $(QBE_DIR)/mem.c \
                  $(QBE_DIR)/ssa.c $(QBE_DIR)/alias.c $(QBE_DIR)/load.c \
                  $(QBE_DIR)/copy.c $(QBE_DIR)/fold.c $(QBE_DIR)/simpl.c \
                  $(QBE_DIR)/live.c $(QBE_DIR)/spill.c $(QBE_DIR)/rega.c \
                  $(QBE_DIR)/emit.c
QBE_AMD64_SRCS = $(QBE_DIR)/amd64/targ.c $(QBE_DIR)/amd64/sysv.c \
                 $(QBE_DIR)/amd64/isel.c $(QBE_DIR)/amd64/emit.c
QBE_ARM64_SRCS = $(QBE_DIR)/arm64/targ.c $(QBE_DIR)/arm64/abi.c \
                 $(QBE_DIR)/arm64/isel.c $(QBE_DIR)/arm64/emit.c
QBE_RV64_SRCS = $(QBE_DIR)/rv64/targ.c $(QBE_DIR)/rv64/abi.c \
                $(QBE_DIR)/rv64/isel.c $(QBE_DIR)/rv64/emit.c
QBE_SRCS = $(QBE_COMMON_SRCS) $(QBE_AMD64_SRCS) $(QBE_ARM64_SRCS) $(QBE_RV64_SRCS)

# Linenoise source files (line editing library)
LINENOISE_SRCS = deps/linenoise/linenoise.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%.o)
LIB_OBJS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(BUILD_DIR)/lib_%.o)
RUNTIME_OBJS = $(RUNTIME_SRCS:$(RUNTIME_DIR)/%.c=$(BUILD_DIR)/runtime_%.o)
# QBE object files (with flattened names to avoid subdirectory issues)
QBE_COMMON_OBJS = $(BUILD_DIR)/qbe_main.o $(BUILD_DIR)/qbe_util.o $(BUILD_DIR)/qbe_parse.o \
                  $(BUILD_DIR)/qbe_abi.o $(BUILD_DIR)/qbe_cfg.o $(BUILD_DIR)/qbe_mem.o \
                  $(BUILD_DIR)/qbe_ssa.o $(BUILD_DIR)/qbe_alias.o $(BUILD_DIR)/qbe_load.o \
                  $(BUILD_DIR)/qbe_copy.o $(BUILD_DIR)/qbe_fold.o $(BUILD_DIR)/qbe_simpl.o \
                  $(BUILD_DIR)/qbe_live.o $(BUILD_DIR)/qbe_spill.o $(BUILD_DIR)/qbe_rega.o \
                  $(BUILD_DIR)/qbe_emit.o
QBE_AMD64_OBJS = $(BUILD_DIR)/qbe_amd64_targ.o $(BUILD_DIR)/qbe_amd64_sysv.o \
                 $(BUILD_DIR)/qbe_amd64_isel.o $(BUILD_DIR)/qbe_amd64_emit.o
QBE_ARM64_OBJS = $(BUILD_DIR)/qbe_arm64_targ.o $(BUILD_DIR)/qbe_arm64_abi.o \
                 $(BUILD_DIR)/qbe_arm64_isel.o $(BUILD_DIR)/qbe_arm64_emit.o
QBE_RV64_OBJS = $(BUILD_DIR)/qbe_rv64_targ.o $(BUILD_DIR)/qbe_rv64_abi.o \
                $(BUILD_DIR)/qbe_rv64_isel.o $(BUILD_DIR)/qbe_rv64_emit.o
QBE_OBJS = $(QBE_COMMON_OBJS) $(QBE_AMD64_OBJS) $(QBE_ARM64_OBJS) $(QBE_RV64_OBJS)

# Linenoise object file
LINENOISE_OBJS = $(BUILD_DIR)/linenoise.o

# Runtime library (linked into compiled Fern programs)
RUNTIME_LIB = $(BIN_DIR)/libfern_runtime.a

# Binaries
FERN_BIN = $(BIN_DIR)/fern
TEST_BIN = $(BIN_DIR)/test_runner
FUZZ_BIN = $(BIN_DIR)/fuzz_runner
FUZZ_SRCS = tests/fuzz/fuzz_runner.c tests/fuzz/fuzz_generator.c
FUZZ_TEST_OBJ = $(BUILD_DIR)/fuzz_generator_test.o
PERF_BUDGET_FLAGS ?=
RELEASE_POLICY_DOC ?= docs/COMPATIBILITY_POLICY.md

# Default target
.PHONY: all
all: debug

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUGFLAGS)
debug: $(FERN_BIN) $(RUNTIME_LIB)

# Release build
.PHONY: release
release: CFLAGS += $(RELEASEFLAGS)
release: clean $(FERN_BIN) $(RUNTIME_LIB)

# Build fern compiler (includes embedded QBE backend and linenoise)
$(FERN_BIN): $(OBJS) $(LIB_OBJS) $(QBE_OBJS) $(LINENOISE_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built fern compiler: $@"

# Build runtime library (static archive for linking into Fern programs)
$(RUNTIME_LIB): $(RUNTIME_OBJS) | $(BIN_DIR)
	ar rcs $@ $^
	@echo "✓ Built runtime library: $@"

# Build and run tests
.PHONY: test
test: CFLAGS += $(DEBUGFLAGS)
test: $(FERN_BIN) $(TEST_BIN)
	@echo "Running tests..."
	@$(TEST_BIN)

# Build test runner (includes linenoise for REPL tests)
$(TEST_BIN): $(TEST_OBJS) $(LIB_OBJS) $(LINENOISE_OBJS) $(FUZZ_TEST_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built test runner: $@"

# Build fuzz runner
$(FUZZ_BIN): $(FUZZ_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(FUZZ_SRCS)
	@echo "✓ Built fuzz runner: $@"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile fuzz generator object for unit tests
$(FUZZ_TEST_OBJ): tests/fuzz/fuzz_generator.c tests/fuzz/fuzz_generator.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c tests/fuzz/fuzz_generator.c -o $@

# Compile library files
$(BUILD_DIR)/lib_%.o: $(LIB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile runtime files (with GC support)
$(BUILD_DIR)/runtime_%.o: $(RUNTIME_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GC_CFLAGS) -I$(RUNTIME_DIR) -c $< -o $@

# Compile QBE common files (embedded compiler backend, uses C99)
$(BUILD_DIR)/qbe_%.o: $(QBE_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(QBE_CFLAGS) -c $< -o $@

# Compile QBE amd64 target files
$(BUILD_DIR)/qbe_amd64_%.o: $(QBE_DIR)/amd64/%.c | $(BUILD_DIR)
	$(CC) $(QBE_CFLAGS) -c $< -o $@

# Compile QBE arm64 target files
$(BUILD_DIR)/qbe_arm64_%.o: $(QBE_DIR)/arm64/%.c | $(BUILD_DIR)
	$(CC) $(QBE_CFLAGS) -c $< -o $@

# Compile QBE rv64 target files
$(BUILD_DIR)/qbe_rv64_%.o: $(QBE_DIR)/rv64/%.c | $(BUILD_DIR)
	$(CC) $(QBE_CFLAGS) -c $< -o $@

# Compile linenoise (external library with lenient warnings)
$(BUILD_DIR)/linenoise.o: deps/linenoise/linenoise.c | $(BUILD_DIR)
	$(CC) $(LINENOISE_CFLAGS) -c $< -o $@

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "✓ Cleaned build artifacts"

# Install fern compiler
.PHONY: install
install: release
	install -m 755 $(FERN_BIN) /usr/local/bin/fern
	@echo "✓ Installed fern to /usr/local/bin/fern"

# Uninstall
.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/fern
	@echo "✓ Uninstalled fern"

# Run with Valgrind for memory checking
.PHONY: memcheck
memcheck: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(FERN_BIN)

# Format code
.PHONY: fmt
fmt:
	clang-format -i $(SRC_DIR)/*.c $(INCLUDE_DIR)/*.h $(TEST_DIR)/*.c

# Full quality check (build + test + style, strict mode)
.PHONY: check
check:
	@uv run scripts/check_style.py $(SRC_DIR) $(LIB_DIR)

# Style check only (no build/test)
.PHONY: style
style:
	@uv run scripts/check_style.py --style-only $(SRC_DIR) $(LIB_DIR)

# Lenient style check (warnings allowed)
.PHONY: style-lenient
style-lenient:
	@uv run scripts/check_style.py --style-only --lenient $(SRC_DIR) $(LIB_DIR)

# Run the Fern implementation of the style checker (bootstrapping progress)
.PHONY: style-fern
style-fern: debug
	@$(FERN_BIN) run scripts/check_style.fn

# Compare Python checker and Fern checker exit-code parity
.PHONY: style-parity
style-parity: debug
	@echo "Comparing style checker parity (python vs fern)..."
	@py_status=0; fern_status=0; \
	uv run scripts/check_style.py --style-only $(SRC_DIR) $(LIB_DIR) > /tmp/fern_style_python.out 2>&1 || py_status=$$?; \
	$(FERN_BIN) run scripts/check_style.fn > /tmp/fern_style_fern.out 2>&1 || fern_status=$$?; \
	echo "  python status: $$py_status"; \
	echo "  fern status:   $$fern_status"; \
	if [ $$py_status -ne $$fern_status ]; then \
		echo "  parity mismatch (see /tmp/fern_style_python.out and /tmp/fern_style_fern.out)"; \
		exit 1; \
	fi; \
	echo "✓ style checker exit-code parity"

# Pre-commit hook check
.PHONY: pre-commit
pre-commit:
	@uv run scripts/check_style.py --pre-commit $(SRC_DIR) $(LIB_DIR)

# Test examples - type check all .fn files in examples/
# Note: We only check compilation, not execution, since some examples
# return non-zero exit codes intentionally (e.g., hello.fn returns 42)
.PHONY: test-examples
test-examples: debug
	@echo "Testing examples..."
	@failed=0; \
	for f in examples/*.fn; do \
		name=$$(basename "$$f"); \
		printf "  %-30s" "$$name"; \
		if $(FERN_BIN) check "$$f" > /dev/null 2>&1; then \
			echo "✓ PASS"; \
		else \
			echo "✗ FAIL"; \
			$(FERN_BIN) check "$$f" 2>&1 | head -5; \
			failed=1; \
		fi; \
	done; \
	if [ $$failed -eq 1 ]; then \
		echo ""; \
		echo "Some examples failed type checking!"; \
		exit 1; \
	else \
		echo ""; \
		echo "✓ All examples type check!"; \
	fi

# Run grammar/property fuzzing for parser + formatter stability
# Usage:
#   make fuzz ITERS=1000 SEED=0x1234
.PHONY: fuzz
fuzz: debug $(FUZZ_BIN)
	@iters="$${ITERS:-512}"; \
	seed="$${SEED:-$$(date +%s)}"; \
	$(FUZZ_BIN) --iterations "$$iters" --seed "$$seed" --fern-bin "$(FERN_BIN)"

# Run a short fuzzing pass suitable for CI
.PHONY: fuzz-smoke
fuzz-smoke: debug $(FUZZ_BIN)
	@iters="$${ITERS:-64}"; \
	seed="$${SEED:-0xC0FFEE}"; \
	$(FUZZ_BIN) --iterations "$$iters" --seed "$$seed" --fern-bin "$(FERN_BIN)"

# Continuously run fuzzing with rolling seeds
.PHONY: fuzz-forever
fuzz-forever: debug $(FUZZ_BIN)
	@iters="$${ITERS:-256}"; \
	while true; do \
		seed="$$(date +%s)"; \
		echo "== FernFuzz run (seed=$$seed, iterations=$$iters) =="; \
		$(FUZZ_BIN) --iterations "$$iters" --seed "$$seed" --fern-bin "$(FERN_BIN)" || exit $$?; \
	done

# Check CI performance budgets (compile time, startup latency, binary size)
.PHONY: perf-budget
perf-budget:
	@python3 scripts/check_perf_budget.py $(PERF_BUDGET_FLAGS)

# Validate compatibility/deprecation policy document
.PHONY: release-policy-check
release-policy-check:
	@python3 scripts/check_release_policy.py $(RELEASE_POLICY_DOC)

# Generate editor support files (syntax highlighting, grammar, etc.)
# Run this after changing language keywords/tokens in include/token.h
.PHONY: editor-support
editor-support:
	@echo "Generating editor support files..."
	@python3 scripts/generate_editor_support.py
	@echo "✓ Editor support files regenerated"
	@echo ""
	@echo "To compile Tree-sitter grammar to WASM:"
	@echo "  make editor-support-compile"

# Compile Tree-sitter grammar to WASM
# Requires: npm install -g tree-sitter-cli
.PHONY: editor-support-compile
editor-support-compile: editor-support
	@echo "Compiling Tree-sitter grammar to WASM..."
	@if ! command -v tree-sitter >/dev/null 2>&1; then \
		echo "Error: tree-sitter CLI not found"; \
		echo "Install with: npm install -g tree-sitter-cli"; \
		exit 1; \
	fi
	@cd editor/tree-sitter-fern && tree-sitter generate
	@cd editor/tree-sitter-fern && tree-sitter build --wasm
	@cp editor/tree-sitter-fern/tree-sitter-fern.wasm editor/zed-fern/languages/fern/fern.wasm
	@echo "✓ Tree-sitter grammar compiled and installed to Zed extension"

# Help
.PHONY: help
help:
	@echo "Fern Compiler - Build Targets"
	@echo ""
	@echo "  make              - Build debug version"
	@echo "  make debug        - Build debug version with symbols"
	@echo "  make release      - Build optimized release version"
	@echo "  make test         - Build and run all tests"
	@echo "  make test-examples- Type check and run all examples"
	@echo "  make fuzz         - Run grammar/property fuzzing"
	@echo "  make fuzz-smoke   - Run short fuzzing pass"
	@echo "  make fuzz-forever - Run fuzzing in a loop"
	@echo "  make perf-budget  - Enforce compile/startup/size budgets"
	@echo "  make release-policy-check - Validate compatibility policy doc"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install fern to /usr/local/bin"
	@echo "  make uninstall    - Remove installed fern"
	@echo "  make memcheck     - Run with Valgrind"
	@echo "  make fmt          - Format code with clang-format"
	@echo ""
	@echo "Quality Checks (strict mode - warnings are errors):"
	@echo "  make check        - Full check (build + test + style)"
	@echo "  make style        - FERN_STYLE only (strict)"
	@echo "  make style-fern   - Run Fern implementation of style checker"
	@echo "  make style-parity - Compare Python vs Fern checker exit status"
	@echo "  make style-lenient - FERN_STYLE only (warnings allowed)"
	@echo "  make pre-commit   - Pre-commit hook check"
	@echo ""
	@echo "Editor Support:"
	@echo "  make editor-support         - Regenerate grammar/highlighting from token.h"
	@echo "  make editor-support-compile - Compile Tree-sitter grammar to WASM"
	@echo ""
	@echo "  make help         - Show this help message"
