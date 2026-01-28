# Fern Compiler - Makefile
# A statically-typed, functional language that compiles to single binaries

CC = clang
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -Ilib
DEBUGFLAGS = -g -O0 -DDEBUG
RELEASEFLAGS = -O2 -DNDEBUG
LDFLAGS =

# Directories
SRC_DIR = src
TEST_DIR = tests
LIB_DIR = lib
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
LIB_SRCS = $(wildcard $(LIB_DIR)/*.c)

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/test_%.o)
LIB_OBJS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(BUILD_DIR)/lib_%.o)

# Binaries
FERN_BIN = $(BIN_DIR)/fern
TEST_BIN = $(BIN_DIR)/test_runner

# Default target
.PHONY: all
all: debug

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUGFLAGS)
debug: $(FERN_BIN)

# Release build
.PHONY: release
release: CFLAGS += $(RELEASEFLAGS)
release: clean $(FERN_BIN)

# Build fern compiler
$(FERN_BIN): $(OBJS) $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built fern compiler: $@"

# Build and run tests
.PHONY: test
test: CFLAGS += $(DEBUGFLAGS)
test: $(TEST_BIN)
	@echo "Running tests..."
	@$(TEST_BIN)

# Build test runner
$(TEST_BIN): $(TEST_OBJS) $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built test runner: $@"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile library files
$(BUILD_DIR)/lib_%.o: $(LIB_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

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

# Check FERN_STYLE compliance
.PHONY: style
style:
	@python3 scripts/check_style.py $(SRC_DIR) $(LIB_DIR)

# Check FERN_STYLE with strict mode (warnings are errors)
.PHONY: style-strict
style-strict:
	@python3 scripts/check_style.py --strict $(SRC_DIR) $(LIB_DIR)

# Help
.PHONY: help
help:
	@echo "Fern Compiler - Build Targets"
	@echo ""
	@echo "  make              - Build debug version"
	@echo "  make debug        - Build debug version with symbols"
	@echo "  make release      - Build optimized release version"
	@echo "  make test         - Build and run all tests"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install fern to /usr/local/bin"
	@echo "  make uninstall    - Remove installed fern"
	@echo "  make memcheck     - Run with Valgrind"
	@echo "  make fmt          - Format code with clang-format"
	@echo "  make style        - Check FERN_STYLE compliance"
	@echo "  make style-strict - Check FERN_STYLE (warnings are errors)"
	@echo "  make help         - Show this help message"
