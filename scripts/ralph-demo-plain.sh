#!/usr/bin/env bash
# Ralph Loop - Simple Demo (Plain Text)

cat << 'EOF'

╔═══════════════════════════════════════════════════════════╗
║              Ralph Loop - Live Demo                      ║
╚═══════════════════════════════════════════════════════════╝

This demo shows how the Ralph Loop works with a simple task.

Task: Implement if expression parsing
Goal: Add 2 tests, implement feature, verify

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ITERATION 1: IMPLEMENTER

Step 1: IMPLEMENTER reads ROADMAP.md

Current Task:
  - [ ] Implement if expression parsing

Expected Tests:
  - test_parse_if_simple()
  - test_parse_if_else()

Expected Files:
  - tests/test_parser.c (add tests)
  - lib/parser.c (implement)

─────────────────────────────────────────────────────────────

Step 2: IMPLEMENTER writes tests (RED phase)

I would add to tests/test_parser.c:

void test_parse_if_simple(void) {
    Arena* arena = arena_create(4096);
    Parser* parser = parser_new(arena, "if true: 42");

    Expr* expr = parse_expr(parser);
    ASSERT_EQ(expr->type, EXPR_IF);
    // ... more assertions

    arena_destroy(arena);
}

void test_parse_if_else(void) {
    // Similar test with else branch
}

Run: make test
Result: FAIL - EXPR_IF not implemented yet

Commit: git commit --no-verify -m "test(parser): add if tests (RED)"

─────────────────────────────────────────────────────────────

Step 3: IMPLEMENTER implements feature (GREEN phase)

I would add to lib/parser.c:

// In parse_primary_internal():
if (match(parser, TOKEN_IF)) {
    Expr* condition = parse_expression(parser);
    consume(parser, TOKEN_COLON, "Expected ':' after if condition");
    Expr* then_branch = parse_expression(parser);

    Expr* else_branch = NULL;
    if (match(parser, TOKEN_ELSE)) {
        consume(parser, TOKEN_COLON, "Expected ':' after else");
        else_branch = parse_expression(parser);
    }

    return expr_if(parser->arena, condition, then_branch,
                   else_branch, parser->previous.loc);
}

Also add expr_if() helper function to lib/ast.c

Run: make test
Result: PASS - 38/38 tests passing!

Commit: git commit -m "feat(parser): implement if expressions (GREEN)"

─────────────────────────────────────────────────────────────

Step 4: IMPLEMENTER updates ROADMAP.md

Updates Ralph Loop Status section:
  - Agent Turn: CONTROLLER
  - Status: WAITING_VERIFICATION
  - Implementation Notes: (what was done)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

ITERATION 2: CONTROLLER

Step 1: CONTROLLER reads ROADMAP.md

Status: WAITING_VERIFICATION
Implementation Notes: (reads what IMPLEMENTER did)

─────────────────────────────────────────────────────────────

Step 2: CONTROLLER verifies

Run: make clean && make test
Result: 38/38 tests passing ✓

Review code: git diff HEAD~2
Checks:
  ✓ Uses arena allocation
  ✓ No compiler warnings
  ✓ Follows existing patterns
  ✓ Tests are comprehensive

─────────────────────────────────────────────────────────────

Step 3: CONTROLLER decides

Decision: ACCEPT ✓

Updates ROADMAP.md:
  - Mark task: [x] Implement if expression parsing ✅
  - Status: IN_PROGRESS
  - Agent Turn: IMPLEMENTER
  - Verification Notes: (results)
  - Next Task: Implement match expression parsing

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Loop continues with next task...

Key Points:
  ✓ ROADMAP.md is the shared state
  ✓ Strict TDD: RED → GREEN → VERIFY
  ✓ Quality gate: CONTROLLER can reject
  ✓ Full audit trail in ROADMAP.md and git

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Want to try it for real?

1. Manual test (recommended):
   cat RALPH_TEST.md - Read the step-by-step guide

2. Use Claude Code as IMPLEMENTER:
   - Read scripts/ralph-prompts.md for IMPLEMENTER prompt
   - Invoke Claude Code with Opus 4.5
   - Give it the IMPLEMENTER task from ROADMAP.md

3. Use Claude Code as CONTROLLER:
   - Read scripts/ralph-prompts.md for CONTROLLER prompt
   - Invoke Claude Code with Sonnet 4.5
   - Give it the verification task

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

EOF
