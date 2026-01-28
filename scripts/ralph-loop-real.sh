#!/usr/bin/env bash
# Ralph Loop - TRUE autonomous development
# Runs CONTROLLER and IMPLEMENTER agents in a loop via Claude Code CLI

set -e

ITERATIONS=${1:-10}
CURRENT_ITER=0

echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                 RALPH LOOP - AUTONOMOUS MODE                 ║"
echo "╔══════════════════════════════════════════════════════════════╗"
echo ""
echo "Running $ITERATIONS iterations..."
echo ""

while [ $CURRENT_ITER -lt $ITERATIONS ]; do
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Iteration $((CURRENT_ITER + 1))/$ITERATIONS"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

    # Step 1: CONTROLLER verifies and selects next task
    echo ""
    echo "🎭 CONTROLLER (Sonnet 4.5): Verifying previous work and selecting next task..."
    echo ""

    claude-code --model sonnet << 'CONTROLLER_EOF'
You are acting as the CONTROLLER agent in the Ralph Loop.

Read ROADMAP.md to see the current iteration status.

Your tasks:
1. Verify the most recent implementation (run tests, check code quality)
2. Update ROADMAP.md with verification notes
3. Select the next task from Milestone 2 (Parser)
4. Update ROADMAP.md with the new task for IMPLEMENTER
5. Commit your changes

Be concise. Focus on verification and task selection.
CONTROLLER_EOF

    if [ $? -ne 0 ]; then
        echo "❌ CONTROLLER failed"
        exit 1
    fi

    # Step 2: IMPLEMENTER implements the selected task
    echo ""
    echo "🎭 IMPLEMENTER (Opus 4.5): Implementing the selected task..."
    echo ""

    claude-code --model opus << 'IMPLEMENTER_EOF'
You are acting as the IMPLEMENTER agent in the Ralph Loop.

Read ROADMAP.md to see your assigned task.

Your tasks:
1. Write tests FIRST (RED phase) - commit with --no-verify
2. Implement the feature (GREEN phase) - make tests pass
3. Update ROADMAP.md with implementation notes
4. Commit your changes

Follow TDD strictly. Be thorough but concise.
IMPLEMENTER_EOF

    if [ $? -ne 0 ]; then
        echo "❌ IMPLEMENTER failed"
        exit 1
    fi

    CURRENT_ITER=$((CURRENT_ITER + 1))

    # Show current test status
    echo ""
    echo "📊 Current Status:"
    make test 2>&1 | grep -E "(Total:|Passed:)" || true
    echo ""
done

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║              RALPH LOOP COMPLETE - $ITERATIONS ITERATIONS              ║"
echo "╔══════════════════════════════════════════════════════════════╗"
echo ""
make test 2>&1 | tail -10
