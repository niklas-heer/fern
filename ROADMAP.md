# Fern Roadmap

Last updated: 2026-02-06

This is the single source of truth for current status and next priorities.
Detailed historical logs and old iteration notes were moved to:
- `docs/ROADMAP_ARCHIVE_2026-02-06.md`

## Current Snapshot

- Build/tests: `make test` passing (**479/479**)
- Style: `make style` passing
- Foundation status: lexer, parser, type checker, codegen pipeline, core runtime, and embedded toolchain are working
- Release automation: conventional-commit-driven semver + release notes configured via `release-please`
- Current focus: Post-Gate D stabilization and adoption docs maintenance

## Working Model

All active work follows strict TDD:
1. Write tests first (red)
2. Implement (green)
3. Run full checks (`make check`)
4. Update this roadmap immediately

## Execution Gates

Gates are sequential. Only one gate is active at a time.

1. Gate A (passed)
2. Gate B (passed)
3. Gate C (after Gate B passes)
4. Gate D (after Gate C passes)

### Gate A: Developer Experience and Language Feel

**Status:** PASSED (2026-02-06)
**Maps to milestones:** 6, 10, 10.5
**Dependency:** none

**Implementation checklist:**
- [x] Diagnostic UX pass: snippets, notes, fix hints, consistent formatting (`test_cli_check_syntax_error_includes_note_and_help`, `test_cli_check_type_error_includes_snippet_note_and_help`)
- [x] CLI polish: `--color`, `--quiet`, `--verbose`, stable behavior across commands (`test_cli_verbose_emits_debug_lines`, `test_cli_verbose_after_command_emits_debug_lines`, `test_cli_unknown_global_option_reports_unknown_option`, `test_cli_color_mode_always_and_never`)
- [x] `fern fmt` with deterministic output and regression tests (`test_cli_fmt_normalizes_and_is_deterministic`)
- [x] End-to-end golden tests for `build`/`check`/`parse`/`fmt` (`test_cli_e2e_command_flow_fmt_parse_check_build`)

**Pass criteria (all required):**
- [x] Onboarding flow can be completed from docs/examples without manual intervention (validated via `test_cli_e2e_command_flow_fmt_parse_check_build` + `make check` examples pass)
- [x] Diagnostic golden tests cover representative syntax/type/check failures (`tests/test_cli_parse.c` + `tests/test_cli_main.c`)
- [x] `fern fmt` determinism is validated in CI (`make check` includes `test_cli_fmt_normalizes_and_is_deterministic`)
- [x] `make check` remains green after each merged Gate A task (Task 1-4 all validated with `make check`)

### Gate B: Reliability and Regression Resistance

**Status:** PASSED (2026-02-06)
**Maps to milestones:** 11 + FernFuzz + FernSim foundation
**Dependency:** Gate A passed

**Implementation checklist:**
- [x] Grammar/property fuzzing with seed corpus and CI integration (FernFuzz) (`tests/fuzz/fuzz_runner.c`, `tests/fuzz/fuzz_generator.c`, `tests/fuzz/corpus/*.fn`, `make fuzz-smoke`, CI step `Run fuzz smoke test`, richer templates for `if`/`match`/`with`/typed signatures/layout)
- [x] Deterministic actor simulation scaffolding (FernSim foundation) (`include/fernsim.h`, `lib/fernsim.c`, `tests/test_fernsim.c`)
- [x] CI performance budgets: compile time, startup latency, binary size (`scripts/check_perf_budget.py`, `make perf-budget`, CI step `Check performance budgets`)
- [x] Compatibility/deprecation policy for language and stdlib changes (`docs/COMPATIBILITY_POLICY.md`, `scripts/check_release_policy.py`, `make release-policy-check`, `.github/workflows/release.yml`)

**Pass criteria (all required):**
- [x] Regressions are caught by deterministic tests + fuzz jobs in CI (`make check` + CI fuzz smoke + FernSim deterministic unit tests)
- [x] Performance thresholds are enforced with failing CI on regressions (`make perf-budget` in CI)
- [x] Upgrade policy is documented and referenced in release workflow (`docs/COMPATIBILITY_POLICY.md` + `.github/workflows/release.yml`)

### Gate C: Product Surface and Standard Library Quality

**Status:** PASSED (2026-02-06)
**Maps to milestones:** 7, 8, 9
**Dependency:** Gate B passed

**Implementation checklist:**
- [x] Stabilize stdlib module APIs (`fs`, `http`, `json`, `sql`, `actors`) - completed with compiler/checker/codegen coverage plus runtime placeholder contract tests and compatibility docs (`tests/test_checker.c`, `tests/test_codegen.c`, `tests/test_runtime_surface.c`, `docs/COMPATIBILITY_POLICY.md`)
- [x] Milestone 7.7 / Step A: Introduce runtime memory abstraction (`alloc/dup/drop` API surface) behind current Boehm implementation (`runtime/fern_runtime.h`, `runtime/fern_runtime.c`, `test_runtime_memory_alloc_dup_drop_contract`)
- [x] Milestone 7.7 / Step B: Add Perceus object header + refcount ops for core heap value types (`fern_rc_alloc/dup/drop`, header metadata accessors, core type tags validated in `test_runtime_rc_header_and_core_type_ops`)
- [x] Milestone 7.7 / Step C: Add initial dup/drop insertion in codegen for a constrained, test-covered subset (`test_codegen_dup_inserted_for_pointer_alias_binding`, `test_codegen_drop_inserted_for_unreturned_pointer_bindings`)
- [x] Milestone 7.7 / Step D: Benchmark/compare Boehm bridge vs Perceus baseline vs WasmGC feasibility and record default + fallback path (`scripts/compare_memory_paths.py`, `docs/reports/memory-path-comparison-2026-02-06.md`)
- [x] Complete actor runtime core (`spawn`, `send`, `receive`, scheduler) with in-memory mailbox FIFO + round-robin scheduler tickets (`test_runtime_actors_post_and_next_mailbox_contract`, `test_runtime_actor_scheduler_round_robin_contract`)
- [x] Ship canonical templates/examples: tiny CLI, HTTP API, actor-based app (`examples/tiny_cli.fn`, `examples/http_api.fn`, `examples/actor_app.fn`, `tests/test_canonical_examples.c`)
- [x] Continuous example validation + doc tests in CI (`.github/workflows/ci.yml` step `Validate examples` runs `make test-examples`)
- [x] Add initial `fern doc` documentation generation pipeline (`src/main.c` `doc` command + `scripts/generate_docs.py` + CLI tests in `tests/test_cli_main.c`)
- [x] Advance bootstrapping tools (`fern doc`, `fern test`, `fern-style` parity targets) (`src/main.c` doc/test command updates, `scripts/generate_docs.py`, `Makefile` targets `style-fern` and `style-parity`, CLI coverage in `tests/test_cli_main.c`)
- [x] Expand `fern doc` output quality: cross-linked markdown module/function index, richer `@doc """..."""` extraction, and optional HTML output (`scripts/generate_docs.py`, `src/main.c`, `tests/test_cli_main.c`)

**Pass criteria (all required):**
- [x] Canonical examples and templates are continuously green in CI (`make test-examples` in CI + `tests/test_canonical_examples.c`)
- [x] Core stdlib APIs are stable and documented (`docs/STDLIB_API_REFERENCE.md`, `docs/COMPATIBILITY_POLICY.md`, `test_check_fs_api_signatures`, `test_check_json_api_signatures`, `test_check_http_api_signatures`, `test_check_sql_api_signatures`, `test_check_actors_api_signatures`, `test_check_file_alias_api_signatures`)
- [x] Milestone 7.7 implementation tranche (A-D) is complete with tests and measured tradeoffs, and a chosen default memory path for first WASM target (`docs/reports/memory-path-comparison-2026-02-06.md`, `docs/MEMORY_MANAGEMENT.md`, `DECISIONS.md`)
- [x] Actor baseline scenarios pass deterministic tests (`test_runtime_actors_post_and_next_mailbox_contract`, `test_runtime_actor_scheduler_round_robin_contract`, `make check`)

### Gate D: Ecosystem and Adoption

**Status:** PASSED (2026-02-06)
**Maps to milestones:** 10.5 follow-up, 11
**Dependency:** Gate C passed

**Implementation checklist:**
- [x] Expand LSP beyond MVP (completion, rename, code actions, better positions) (`include/lsp.h`, `lib/lsp.c`, `tests/test_lsp.c`)
- [x] Harden packaging/release workflow for single-binary distribution (`scripts/package_release.py`, `tests/test_release_packaging.c`, `.github/workflows/release.yml`)
- [x] Publish reproducible benchmarks and case studies (`scripts/publish_benchmarks.py`, `tests/test_benchmark_publication.c`, `docs/reports/benchmark-case-studies-2026-02-06.md`)

**Pass criteria (all required):**
- [x] First-run developer workflow is smooth across supported platforms (`release.yml` bundle matrix for Linux/macOS + release layout validation)
- [x] Benchmark claims are reproducible from repository scripts (`make benchmark-report`, `scripts/publish_benchmarks.py`, report artifact upload in release workflow)
- [x] Adoption docs/templates are versioned and maintained (canonical examples + published benchmark case-study report in `docs/reports/`)

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
- Milestone 7.7: WASM target + memory model implementation tranche (active, pre-actor-runtime dependency)
- Milestone 10: TUI library completion

### Not Started / Future

- Milestone 8: Actor runtime and supervision model
- Milestone 9: Bootstrapping Fern tooling in Fern
- Milestone 11: Polish and optimization wave

## Next Session Start Here

Gate D pass criteria are now closed and validated with test + workflow coverage. Next work should focus on milestone polish items and adoption follow-through.

1. [x] Gate D / Task 1: Expand LSP beyond MVP (completion, rename, code actions, better source positions)
2. [x] Gate D / Task 2: Harden packaging/release workflow for single-binary distribution
3. [x] Gate D / Task 3: Publish reproducible benchmarks and case studies

## Active Backlog (By Gate)

### Gate A (Passed)

- [x] Diagnostic UX pass (snippets, notes, fix hints, consistency)
- [x] CLI quality-of-life flag pass and output consistency
- [x] `fern fmt` (stable formatting + tests)
- [x] Gate A E2E golden test suite

### Gate B (Passed)

- [x] Grammar/property fuzzing framework (FernFuzz) + seed corpus + CI smoke
- [x] Deterministic actor simulation foundation (FernSim) + seeded scheduler/virtual clock tests
- [x] Performance budgets in CI (compile time, startup, binary size)
- [x] Compatibility/deprecation policy document + release workflow linkage

### Gate C/D (Later)

- [ ] Extended stdlib modules with doc tests
- [x] Milestone 7.7 WASM memory/runtime implementation tranche (abstraction + RC baseline + codegen subset + decision artifact)
- [x] Actor runtime core baseline (`spawn`, `send`, `receive`, scheduler`) with mailbox/scheduler runtime tests
- [x] `fern doc` documentation generation pipeline (`src/main.c`, `scripts/generate_docs.py`, `tests/test_cli_main.c`)
- [x] LSP expansion beyond MVP (completion/rename/code actions)

## Out of Scope for This File

To keep this roadmap clear, we no longer track:
- Historical per-iteration implementation logs
- Already-closed incident reports and one-off fixes
- Deep design notes better suited for `DESIGN.md` / `DECISIONS.md`

Historical details remain in `docs/ROADMAP_ARCHIVE_2026-02-06.md`.
