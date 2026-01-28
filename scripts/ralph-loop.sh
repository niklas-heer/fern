#!/usr/bin/env bash
# Ralph Loop - Autonomous Development System
# This script tells Claude Code to continue the Ralph Loop

set -e

cat << 'EOF'
╔══════════════════════════════════════════════════════════════╗
║                    RALPH LOOP RUNNER                         ║
╔══════════════════════════════════════════════════════════════╗

This will prompt Claude Code to continue the Ralph Loop.

The Ralph Loop alternates between:
  - IMPLEMENTER (Opus 4.5): Writes tests and implementation
  - CONTROLLER (Sonnet 4.5): Verifies and selects next task

Progress is tracked in ROADMAP.md
EOF

echo ""
echo "To run the Ralph Loop, simply tell Claude Code:"
echo ""
echo "  👉 \"Continue the Ralph Loop for 10 iterations\""
echo ""
echo "Or for overnight:"
echo ""
echo "  👉 \"Run the Ralph Loop until you hit context limits\""
echo ""
echo "Claude Code will autonomously implement parser features!"
