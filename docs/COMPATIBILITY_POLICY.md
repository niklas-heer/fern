# Compatibility and Deprecation Policy

This policy defines how Fern evolves while keeping upgrades predictable.
It is enforced in CI and referenced by the release workflow.

## Compatibility Guarantees

Fern compatibility is defined for four surfaces:

1. Language syntax accepted by `fern parse/check/build`.
2. Type-checking behavior for valid programs.
3. Standard library module APIs shipped with the compiler.
4. CLI command and flag contract for stable commands.

Compatibility baseline:

1. Patch releases (`x.y.Z`) must remain backward compatible.
2. Minor releases (`x.Y.z`) may add features but must not break existing valid code.
3. Breaking changes are allowed only in major releases and must include migration notes.
4. Experimental features must be clearly marked experimental and may change before stabilization.

## Deprecation Lifecycle

Every removal or incompatible behavior change must follow this sequence:

1. Announcement: document deprecation in release notes and migration guidance.
2. Warning phase: compiler or docs include explicit deprecation warning and replacement.
3. Removal: remove behavior only after the support window is met.

Minimum support window: at least **2 minor releases** after first deprecation warning.

Required deprecation notice content:

1. What is deprecated.
2. Recommended replacement.
3. Earliest release where removal may happen.

## Versioning Policy

Fern uses Semantic Versioning aligned with `include/version.h`.

1. `MAJOR`: incompatible language/stdlib/CLI changes.
2. `MINOR`: backward-compatible features.
3. `PATCH`: backward-compatible fixes.

`include/version.h` is the single source of truth for version numbers.

## Release Checklist

Before any tagged release:

1. Run `make check`.
2. Run `make perf-budget` (or `make perf-budget PERF_BUDGET_FLAGS=--skip-build` if release build already ran in the same job).
3. Run `make release-policy-check`.
4. Publish release notes with:
   - compatibility notes,
   - deprecations and removals,
   - migration guidance.

The release workflow must fail if policy checks fail.

## Workflow Linkage

The GitHub release workflow references this policy via `make release-policy-check`.
This ensures compatibility/deprecation requirements are evaluated on every release run.
