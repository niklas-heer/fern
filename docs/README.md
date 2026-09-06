# Fern Documentation Index

This folder contains project-level reference documents. Use this file as the canonical starting point.

## Start Here

- [Language guide](LANGUAGE_GUIDE.md): runnable first programs and everyday workflow.
- [Release readiness](RELEASE_READINESS.md): implemented features, remaining gaps, and release gates.
- [Actor runtime](ACTOR_RUNTIME.md): exact mailbox/supervision commitments and execution limits.
- [Rust frontend evaluation](RUST_FRONTEND_EVALUATION.md): prototype scope, native correctness, measured costs, and migration criteria.
- [Native checker progress](BOOTSTRAP_CHECKER.md): bounded command workflows and remaining default-migration gates.
- [Rust migration progress](RUST_MIGRATION.md): collections, error values, verification, and remaining parity work.

## Core References

- [`../README.md`](../README.md): project overview and quick start
- [`../ROADMAP.md`](../ROADMAP.md): active roadmap and current priorities
- [`../DECISIONS.md`](../DECISIONS.md): architecture and design decisions
- [`../DESIGN.md`](../DESIGN.md): language design and semantics
- [`../FERN_STYLE.md`](../FERN_STYLE.md): coding standards and style rules
- [`../CLAUDE.md`](../CLAUDE.md): AI-assisted workflow and quality process
- [`../BUILD.md`](../BUILD.md): build and troubleshooting guide

## Compatibility and Runtime

- [`COMPATIBILITY_POLICY.md`](COMPATIBILITY_POLICY.md): compatibility/deprecation guarantees
- [`STDLIB_API_REFERENCE.md`](STDLIB_API_REFERENCE.md): stable stdlib API signatures
- [`PROCESS_EXECUTION.md`](PROCESS_EXECUTION.md): bounded literal argv, stream limits and process cleanup
- [`MEMORY_MANAGEMENT.md`](MEMORY_MANAGEMENT.md): memory model plan and rationale

## Historical and Reports

- [`HISTORY.md`](HISTORY.md): curated timeline of completed phases and major shifts
- [`ROADMAP_ARCHIVE_2026-02-06.md`](ROADMAP_ARCHIVE_2026-02-06.md): frozen legacy archive pointer
- [`reports/benchmark-case-studies-2026-02-06.md`](reports/benchmark-case-studies-2026-02-06.md): reproducible benchmark report snapshot
- [`reports/memory-path-comparison-2026-02-06.md`](reports/memory-path-comparison-2026-02-06.md): memory-path comparison artifact

## Policy

When a document is superseded, keep a short pointer file at the old location and link to the current canonical source.
