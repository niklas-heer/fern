#!/usr/bin/env bash
# Ralph Loop - Quick Test
#
# This script sets up a simple test iteration to verify the Ralph Loop works

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ROADMAP_FILE="$PROJECT_ROOT/ROADMAP.md"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║          Ralph Loop - Quick Test Setup                   ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

log() {
    echo -e "${BLUE}[Test]${NC} $*"
}

success() {
    echo -e "${GREEN}[Success]${NC} $*"
}

info() {
    echo -e "${YELLOW}[Info]${NC} $*"
}

# Check if Ralph Loop is already initialized
if grep -q "## Ralph Loop Status" "$ROADMAP_FILE"; then
    info "Ralph Loop already initialized in ROADMAP.md"
    echo ""
    echo "Current status:"
    sed -n '/## Ralph Loop Status/,/^---$/p' "$ROADMAP_FILE" | head -n 20
    echo ""
    echo "To reinitialize, first remove the '## Ralph Loop Status' section from ROADMAP.md"
    exit 0
fi

log "Initializing Ralph Loop in ROADMAP.md..."

# Run the init script
if [ -f "$SCRIPT_DIR/ralph-init.sh" ]; then
    "$SCRIPT_DIR/ralph-init.sh"
else
    echo "Error: ralph-init.sh not found"
    exit 1
fi

success "Ralph Loop initialized!"
echo ""

# Now set up a simple test task
log "Setting up a test task..."

# Create a backup
cp "$ROADMAP_FILE" "$ROADMAP_FILE.test-backup"

# Update the Ralph Loop Status section with a concrete test task
cat > /tmp/ralph_test_task.md << 'EOF'

## Ralph Loop Status

**Current Milestone**: 2 - Parser
**Current Iteration**: 1 (TEST)
**Agent Turn**: IMPLEMENTER
**Status**: IN_PROGRESS
**Started**: $(date +"%Y-%m-%d %H:%M:%S")
**Last Updated**: $(date +"%Y-%m-%d %H:%M:%S")

### Current Task

- [ ] Implement if expression parsing (simple test)

**Expected Tests**:
- test_parse_if_simple() - Parse: if true: 42
- test_parse_if_else() - Parse: if x > 0: 1 else: 0

**Expected Files**:
- tests/test_parser.c (add 2 tests)
- lib/parser.c (add if parsing to parse_primary)

**Success Criteria**:
- Both new tests pass
- No regression in existing 36 tests
- No compiler warnings

**Context**:
This is a TEST iteration to verify the Ralph Loop system works.
The task is small and well-defined to make testing easy.

### Implementation Notes

(IMPLEMENTER will fill this after completing the task)

**Checklist**:
- [ ] Read DESIGN.md for if expression syntax
- [ ] Write test_parse_if_simple() in tests/test_parser.c
- [ ] Write test_parse_if_else() in tests/test_parser.c
- [ ] Run tests - confirm FAIL (RED phase)
- [ ] Commit tests with --no-verify
- [ ] Implement if parsing in lib/parser.c
- [ ] Run tests - confirm PASS (GREEN phase)
- [ ] Commit implementation
- [ ] Update this section with notes
- [ ] Set Status: WAITING_VERIFICATION
- [ ] Set Agent Turn: CONTROLLER

### Verification Notes

(CONTROLLER will fill this after verification)

### Blockers

None

---

EOF

# Evaluate the date commands
TEST_TASK=$(eval "cat << 'EVALEOF'
$(cat /tmp/ralph_test_task.md)
EVALEOF
")

# Find the Ralph Loop Status section and replace it
# First, create a temp file with everything before Ralph Loop Status
sed -n '1,/## Ralph Loop Status/p' "$ROADMAP_FILE" | sed '$d' > /tmp/roadmap_before.md

# Then get everything after the first --- following Ralph Loop Status
sed -n '/## Ralph Loop Status/,/^---$/p' "$ROADMAP_FILE" | tail -n +1 > /dev/null
sed -n '/## Ralph Loop Status/,$p' "$ROADMAP_FILE" | sed -n '/^---$/,$p' | tail -n +2 > /tmp/roadmap_after.md

# Combine them
{
    cat /tmp/roadmap_before.md
    echo "$TEST_TASK"
    cat /tmp/roadmap_after.md
} > "$ROADMAP_FILE"

# Clean up temp files
rm -f /tmp/ralph_test_task.md /tmp/roadmap_before.md /tmp/roadmap_after.md

success "Test task configured!"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "${GREEN}Ralph Loop Test Setup Complete!${NC}"
echo ""
echo "Next steps to test the loop:"
echo ""
echo "1. Read the task in ROADMAP.md:"
echo "   ${YELLOW}cat ROADMAP.md | sed -n '/## Ralph Loop Status/,/^---$/p'${NC}"
echo ""
echo "2. As IMPLEMENTER, complete the task:"
echo "   - Write the 2 tests in tests/test_parser.c"
echo "   - Implement if expression parsing in lib/parser.c"
echo "   - Update ROADMAP.md with implementation notes"
echo "   - Set Status: WAITING_VERIFICATION"
echo "   - Set Agent Turn: CONTROLLER"
echo ""
echo "3. As CONTROLLER, verify:"
echo "   - Run: make clean && make test"
echo "   - Check tests pass (should be 38/38)"
echo "   - Update ROADMAP.md with verification notes"
echo "   - Mark task complete: [x]"
echo ""
echo "Or use the Ralph Loop automation:"
echo "   ${YELLOW}./scripts/ralph-iteration.sh${NC}"
echo ""
echo "To restore ROADMAP.md if needed:"
echo "   ${YELLOW}mv ROADMAP.md.test-backup ROADMAP.md${NC}"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
