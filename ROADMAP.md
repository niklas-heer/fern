# Fern Roadmap

Last updated: 2026-02-09

This file is the only active roadmap. Historical context is in [`docs/HISTORY.md`](docs/HISTORY.md).

## Current Status Snapshot

- Quality gate: `just check` passing (`534/534` tests, style passing, examples passing)
- Perf gate: `just perf-budget` passing (compile/startup/size budgets)
- Fuzz gate: `just fuzz-smoke` passing
- Docs gate: `just docs-check` passing
- Release readiness: `just release-package-check` now validates the real staging layout (`dist/staging`)

## Canonical Documents

- Project overview: [`README.md`](README.md)
- Build guide: [`BUILD.md`](BUILD.md)
- Design spec: [`DESIGN.md`](DESIGN.md)
- Decision log: [`DECISIONS.md`](DECISIONS.md)
- Coding standards: [`FERN_STYLE.md`](FERN_STYLE.md)
- Compatibility policy: [`docs/COMPATIBILITY_POLICY.md`](docs/COMPATIBILITY_POLICY.md)
- Documentation index: [`docs/README.md`](docs/README.md)

## Completed Foundations

- Gate A: Developer experience and language feel
- Gate B: Reliability and regression resistance
- Gate C: Product surface and stdlib quality baseline
- Gate D: Ecosystem and adoption hardening

## Active Priorities

### Priority 1: Repository Hygiene and Release Flow

Status: Active

- [x] Fix release staging validation mismatch (`release-package-check` now stages and validates `dist/staging`)
- [x] Keep generated outputs out of source control by default (release artifacts, benchmark binaries)
- [x] Keep documentation cross-linked and remove stale process instructions
- [x] Add a lightweight docs consistency check to CI (link + key status marker validation)

Exit criteria:
- Working tree does not accumulate tracked generated binaries during normal benchmark/release flows.
- Every top-level process document links to canonical docs and active roadmap.

### Priority 2: Milestone 8 Follow-Through (Actor Runtime Semantics)

Status: Active

- [ ] Close remaining supervision/runtime behavior gaps not yet modeled end-to-end
- [ ] Expand deterministic FernSim scenarios for supervision trees and failure policies
- [ ] Add stronger actor runtime invariants to regression suites

Exit criteria:
- Actor runtime semantics are deterministic, regression-covered, and documented as compatibility commitments.

### Priority 3: Milestone 9 Bootstrapping

Status: Not started

- [ ] Reach feature parity for `scripts/check_style.py` in `scripts/check_style.fn`
- [ ] Add parity assertions to CI (`just style-parity` as a required gate)
- [ ] Make Fern-native checker the default once parity is stable

Exit criteria:
- Style checks can run without Python for normal developer workflows.

### Priority 4: Milestone 10 TUI Completion

Status: Partially complete

- [ ] Finish prompt line editing and richer terminal cursor controls
- [ ] Implement tree/log modules for structured CLI UX
- [ ] Add canonical TUI examples with deterministic test coverage

Exit criteria:
- TUI surface is complete enough to support first-party tooling UX needs.

### Priority 5: Milestone 11 Polish and Optimization

Status: Active

- [ ] Reduce binary size and startup variance further while preserving ergonomics
- [ ] Tighten release checklist and pre-1.0 readiness criteria
- [ ] Expand user-facing docs (language guide/tutorial) for adoption

Exit criteria:
- Pre-1.0 release checklist is explicit, measurable, and continuously validated.

## Next Session Start Here

1. Complete repository hygiene tasks (generated artifacts + docs consistency checks).
2. Pick one Milestone 8 actor-runtime closure task and deliver test-first.
3. Advance Milestone 9 by landing the next parity slice for the Fern-native style checker.
