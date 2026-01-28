#!/usr/bin/env python3
"""
Ralph Loop - TRUE autonomous development system
Runs CONTROLLER and IMPLEMENTER agents in a loop via Claude Code CLI
"""

import subprocess
import sys
from pathlib import Path

def run_claude(model: str, prompt: str) -> bool:
    """Run claude with the given model and prompt."""
    try:
        result = subprocess.run(
            ["claude", "--model", model, "--print"],
            input=prompt,
            text=True,
            capture_output=True,
            check=True
        )
        print(result.stdout)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Error running claude: {e}", file=sys.stderr)
        print(e.stdout)
        print(e.stderr, file=sys.stderr)
        return False

def run_tests() -> tuple[int, int]:
    """Run tests and return (total, passed)."""
    try:
        result = subprocess.run(
            ["make", "test"],
            capture_output=True,
            text=True,
            cwd=Path(__file__).parent.parent
        )
        output = result.stdout + result.stderr

        # Parse test results
        for line in output.split('\n'):
            if 'Total:' in line and 'Passed:' in line:
                parts = line.split()
                total = int(parts[parts.index('Total:') + 1])
                passed = int(parts[parts.index('Passed:') + 1])
                return (total, passed)
        return (0, 0)
    except Exception as e:
        print(f"Warning: Could not run tests: {e}", file=sys.stderr)
        return (0, 0)

def main():
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 10

    print("╔══════════════════════════════════════════════════════════════╗")
    print("║                 RALPH LOOP - AUTONOMOUS MODE                 ║")
    print("╔══════════════════════════════════════════════════════════════╗")
    print()
    print(f"Running {iterations} iterations...")
    print()

    for iteration in range(1, iterations + 1):
        print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        print(f"Iteration {iteration}/{iterations}")
        print("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        print()

        # Step 1: CONTROLLER
        print("🎭 CONTROLLER (Sonnet): Verifying and selecting next task...")
        print()

        controller_prompt = """You are the CONTROLLER agent in the Ralph Loop.

Read ROADMAP.md to see the current iteration status.

Your tasks:
1. Verify the most recent implementation (run tests, check code quality)
2. Update ROADMAP.md with verification notes (add new verification section)
3. Select the next task from Milestone 2 (Parser)
4. Update ROADMAP.md to start the next iteration with the new task
5. Commit your changes

Be concise and focused."""

        if not run_claude("sonnet", controller_prompt):
            print("❌ CONTROLLER failed")
            sys.exit(1)

        print()

        # Step 2: IMPLEMENTER
        print("🎭 IMPLEMENTER (Opus): Implementing the task...")
        print()

        implementer_prompt = """You are the IMPLEMENTER agent in the Ralph Loop.

Read ROADMAP.md to see your assigned task under the current iteration.

Your tasks:
1. Write tests FIRST (RED phase) - commit with --no-verify
2. Implement the feature (GREEN phase) - make all tests pass
3. Update ROADMAP.md with implementation notes
4. Commit your changes

Follow TDD strictly. Use arena allocation. Keep code clean."""

        if not run_claude("opus", implementer_prompt):
            print("❌ IMPLEMENTER failed")
            sys.exit(1)

        print()

        # Show status
        total, passed = run_tests()
        print(f"📊 Status: {passed}/{total} tests passing")
        print()

    print()
    print("╔══════════════════════════════════════════════════════════════╗")
    print(f"║           RALPH LOOP COMPLETE - {iterations} ITERATIONS              ║")
    print("╔══════════════════════════════════════════════════════════════╗")
    print()

    total, passed = run_tests()
    print(f"Final: {passed}/{total} tests passing")

if __name__ == "__main__":
    main()
