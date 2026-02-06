# Fern Roadmap

Last updated: 2026-02-06

This is the single source of truth for current status and next priorities.
Detailed historical logs and old iteration notes were moved to:
- `docs/ROADMAP_ARCHIVE_2026-02-06.md`

## Current Snapshot

- Build/tests: `make test` passing (**418/418**)
- Style: `make style` passing
- Foundation status: lexer, parser, type checker, codegen pipeline, core runtime, and embedded toolchain are working
- Current focus: pass gates in order, one gate at a time

## Working Model

All active work follows strict TDD:
1. Write tests first (red)
2. Implement (green)
3. Run full checks (`make check`)
4. Update this roadmap immediately

## Execution Gates

Gates are sequential. Only one gate is active at a time.

1. Gate A (current)
2. Gate B (after Gate A passes)
3. Gate C (after Gate B passes)
4. Gate D (after Gate C passes)

### Gate A: Developer Experience and Language Feel

**Status:** ACTIVE
**Maps to milestones:** 6, 10, 10.5
**Dependency:** none

**Implementation checklist:**
- [x] Diagnostic UX pass: snippets, notes, fix hints, consistent formatting (`test_cli_check_syntax_error_includes_note_and_help`, `test_cli_check_type_error_includes_snippet_note_and_help`)
- [ ] CLI polish: `--color`, `--quiet`, `--verbose`, stable behavior across commands
- [ ] `fern fmt` with deterministic output and regression tests
- [ ] End-to-end golden tests for `build`/`check`/`parse`/`fmt`

**Pass criteria (all required):**
- [ ] Onboarding flow can be completed from docs/examples without manual intervention
- [x] Diagnostic golden tests cover representative syntax/type/check failures (`tests/test_cli_parse.c` + `tests/test_cli_main.c`)
- [ ] `fern fmt` determinism is validated in CI
- [ ] `make check` remains green after each merged Gate A task

### Gate B: Reliability and Regression Resistance

**Status:** READY
**Maps to milestones:** 11 + FernFuzz + FernSim foundation
**Dependency:** Gate A passed

**Implementation checklist:**
- [ ] Grammar/property fuzzing with seed corpus and CI integration (FernFuzz)
- [ ] Deterministic actor simulation scaffolding (FernSim foundation)
- [ ] CI performance budgets: compile time, startup latency, binary size
- [ ] Compatibility/deprecation policy for language and stdlib changes

**Pass criteria (all required):**
- [ ] Regressions are caught by deterministic tests + fuzz jobs in CI
- [ ] Performance thresholds are enforced with failing CI on regressions
- [ ] Upgrade policy is documented and referenced in release workflow

### Gate C: Product Surface and Standard Library Quality

**Status:** QUEUED
**Maps to milestones:** 7, 8, 9
**Dependency:** Gate B passed

**Implementation checklist:**
- [ ] Stabilize stdlib module APIs (`fs`, `http`, `json`, `sql`, `actors`)
- [ ] Complete actor runtime core (`spawn`, `send`, `receive`, scheduler)
- [ ] Ship canonical templates/examples: tiny CLI, HTTP API, actor-based app
- [ ] Continuous example validation + doc tests in CI
- [ ] Advance bootstrapping tools (`fern doc`, `fern test`, `fern-style` parity targets)

**Pass criteria (all required):**
- [ ] Canonical examples and templates are continuously green in CI
- [ ] Core stdlib APIs are stable and documented
- [ ] Actor baseline scenarios pass deterministic tests

### Gate D: Ecosystem and Adoption

**Status:** QUEUED
**Maps to milestones:** 10.5 follow-up, 11
**Dependency:** Gate C passed

**Implementation checklist:**
- [ ] Expand LSP beyond MVP (completion, rename, code actions, better positions)
- [ ] Harden packaging/release workflow for single-binary distribution
- [ ] Publish reproducible benchmarks and case studies

**Pass criteria (all required):**
- [ ] First-run developer workflow is smooth across supported platforms
- [ ] Benchmark claims are reproducible from repository scripts
- [ ] Adoption docs/templates are versioned and maintained

## Milestone Status (Condensed)

### Completed

- Milestone 0: Project setup
- Milestone 0.5: FERN_STYLE enforcement
- Milestone 1: Lexer
- Milestone 2: Parser core
- Milestone 3: Type system
- Milestone 4: QBE code generation pipeline
- Milestone 5: Standard library core runtime/builtins
- Milestone 7.5: Boehm GC integration
- Milestone 7.6: Self-contained toolchain (embedded QBE + static GC link)
- Milestone 10.5: LSP MVP

### Partially Complete / Active

- Milestone 6: CLI tool polish and command surface
- Milestone 7: Extended standard library modules
- Milestone 10: TUI library completion

### Not Started / Future

- Milestone 8: Actor runtime and supervision model
- Milestone 9: Bootstrapping Fern tooling in Fern
- Milestone 11: Polish and optimization wave
- Milestone 7.7: WASM target + Perceus memory model research

## Next Session Start Here

Complete in order. Do not start Gate B until Gate A is passed.

1. [x] Gate A / Task 1: Diagnostic UX pass + golden tests (`make check` green, `test_cli_check_syntax_error_includes_note_and_help`, `test_cli_check_type_error_includes_snippet_note_and_help`)
2. [ ] Gate A / Task 2: CLI flag/output consistency pass + tests
3. [ ] Gate A / Task 3: `fern fmt` deterministic output + CI coverage
4. [ ] Gate A / Task 4: E2E command-flow tests (`build`, `check`, `parse`, `fmt`)
5. [ ] Gate A review: mark pass/fail in this file with evidence (`make check`, test names)

## Active Backlog (By Gate)

### Gate A (Current)

- [x] Diagnostic UX pass (snippets, notes, fix hints, consistency)
- [ ] CLI quality-of-life flag pass and output consistency
- [ ] `fern fmt` (stable formatting + tests)
- [ ] Gate A E2E golden test suite

### Gate B (Upcoming)

- [ ] Grammar/property fuzzing framework (FernFuzz)
- [ ] Deterministic actor simulation foundation (FernSim)
- [ ] Performance budgets in CI (compile time, startup, binary size)
- [ ] Compatibility/deprecation policy document

### Gate C/D (Later)

- [ ] Extended stdlib modules with doc tests
- [ ] Actor runtime core (`spawn`, `send`, `receive`, scheduler)
- [ ] `fern doc` documentation generation pipeline
- [ ] LSP expansion beyond MVP (completion/rename/code actions)

## Out of Scope for This File

To keep this roadmap clear, we no longer track:
- Historical per-iteration implementation logs
- Already-closed incident reports and one-off fixes
- Deep design notes better suited for `DESIGN.md` / `DECISIONS.md`

Historical details remain in `docs/ROADMAP_ARCHIVE_2026-02-06.md`.
