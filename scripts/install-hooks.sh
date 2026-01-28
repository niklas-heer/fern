#!/bin/bash
# Install git hooks for Fern development

set -e

echo "Installing git hooks..."

# Create hooks directory if it doesn't exist
mkdir -p .git/hooks

# Install pre-commit hook
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Fern Compiler - Pre-Commit Hook
# Ensures code quality before allowing commits

set -e

echo "🔍 Running pre-commit checks..."

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print status
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $2"
    else
        echo -e "${RED}✗${NC} $2"
        exit 1
    fi
}

# 1. Check if we're committing any code files
CODE_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|h)$' || true)

if [ -z "$CODE_FILES" ]; then
    echo "No C code changes detected, skipping checks."
    exit 0
fi

echo -e "\n📝 Code files changed:"
echo "$CODE_FILES" | sed 's/^/   /'

# 2. Clean build
echo -e "\n🧹 Cleaning build artifacts..."
make clean > /dev/null 2>&1
print_status $? "Clean build"

# 3. Compile with strict warnings
echo -e "\n🔨 Compiling with strict warnings..."
if make debug 2>&1 | tee /tmp/fern_build.log | grep -q "error:"; then
    echo -e "${RED}✗ Compilation failed${NC}"
    cat /tmp/fern_build.log
    exit 1
fi
print_status 0 "Compilation (no warnings/errors)"

# 4. Run all tests
echo -e "\n🧪 Running test suite..."
make test > /tmp/fern_test.log 2>&1
TEST_EXIT_CODE=$?

if [ $TEST_EXIT_CODE -ne 0 ]; then
    echo -e "${RED}✗ Tests failed${NC}"
    cat /tmp/fern_test.log
    exit 1
fi

# Extract test results
PASSED=$(grep "Passed:" /tmp/fern_test.log | awk '{print $2}')

print_status 0 "All tests passing ($PASSED tests)"

# 5. Run FERN_STYLE checker
echo -e "\n📐 Checking FERN_STYLE compliance..."
if command -v uv &> /dev/null; then
    if ! uv run scripts/check_style.py lib/ src/ 2>&1 | tee /tmp/fern_style.log | grep -q "All files pass"; then
        # Check if there are actual errors (not just warnings)
        if grep -q "error" /tmp/fern_style.log 2>/dev/null; then
            echo -e "${RED}✗ FERN_STYLE violations found${NC}"
            cat /tmp/fern_style.log
            exit 1
        fi
    fi
    print_status 0 "FERN_STYLE compliance"
else
    echo -e "${YELLOW}⚠${NC}  Warning: uv not installed, skipping FERN_STYLE check"
    echo "   Install with: curl -LsSf https://astral.sh/uv/install.sh | sh"
fi

# 6. Check for common mistakes
echo -e "\n🔎 Checking for common mistakes..."

# Check for malloc/free (should use arena)
if echo "$CODE_FILES" | xargs grep -n "malloc\|free(" 2>/dev/null | grep -v "// OK:"; then
    echo -e "${YELLOW}⚠${NC}  Warning: Found malloc/free - should use arena allocation"
    echo "   If this is intentional, add '// OK: malloc' comment"
fi

# Check for char* string handling (should use String)
if echo "$CODE_FILES" | xargs grep -n "char\s*\*" 2>/dev/null | grep -v "const char\|String\|// OK:"; then
    echo -e "${YELLOW}⚠${NC}  Warning: Found char* - consider using String type"
    echo "   If this is intentional, add '// OK: char*' comment"
fi

# Check for manual tagged unions (should use Result macros)
if echo "$CODE_FILES" | xargs grep -n "enum.*kind\|\.kind\s*=" 2>/dev/null | grep -v "// OK:"; then
    echo -e "${YELLOW}⚠${NC}  Warning: Found manual tagged union - consider using Result macros"
fi

# 7. Verify ROADMAP.md and DECISIONS.md updates
COMMIT_MSG_FILE=".git/COMMIT_EDITMSG"
if [ -f "$COMMIT_MSG_FILE" ]; then
    COMMIT_MSG=$(cat "$COMMIT_MSG_FILE" 2>/dev/null || echo "")

    # Check for ROADMAP.md update on feat/fix commits
    if echo "$COMMIT_MSG" | grep -q "^feat\|^fix"; then
        if ! git diff --cached --name-only | grep -q "ROADMAP.md"; then
            echo -e "\n${YELLOW}⚠${NC}  Warning: feat/fix commit without ROADMAP.md update"
            echo "   Consider updating ROADMAP.md to track progress"
        fi
    fi

    # Remind about DECISIONS.md for architectural changes
    if echo "$COMMIT_MSG" | grep -q "^feat"; then
        if git diff --cached --name-only | grep -qE "include/|DESIGN.md"; then
            if ! git diff --cached --name-only | grep -q "DECISIONS.md"; then
                echo -e "${YELLOW}⚠${NC}  Reminder: Did you make a design decision?"
                echo "   If yes, document it: /decision DECISIONS.md | Title | Description"
            fi
        fi
    fi
fi

# 8. Summary
echo -e "\n${GREEN}✓ All pre-commit checks passed!${NC}"
echo ""
echo "Summary:"
echo "  - Compilation: clean"
echo "  - Tests: $PASSED passing"
echo "  - Ready to commit"
echo ""

exit 0
EOF

chmod +x .git/hooks/pre-commit

echo "✓ Pre-commit hook installed successfully!"
echo ""
echo "The hook will run automatically on every commit and check:"
echo "  - Code compiles without warnings"
echo "  - All tests pass"
echo "  - FERN_STYLE compliance (requires uv)"
echo "  - No common mistakes (malloc/free, char*, etc.)"
echo "  - ROADMAP.md is updated for feature commits"
echo ""
echo "To skip the hook (not recommended), use: git commit --no-verify"
