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

## Standard Library API Contract (Gate C)

As of 2026-02-06, Fern reserves and stabilizes the following top-level stdlib entry points:

1. `fs`
2. `json`
3. `http`
4. `sql`
5. `actors`

Compatibility alias policy:

1. `File.*` remains supported as a compatibility alias for `fs.*`.
2. Any future alias deprecation must follow the lifecycle below and keep the minimum 2-minor-release support window.
3. New APIs may be added to these modules in minor releases, but existing signatures must remain backward compatible.

### Gate C Runtime Placeholder Contract (2026-02-06)

Until full `json/http/sql/actors` runtimes land, the current runtime behavior is stabilized as:

1. `json.parse(text)`:
   - `Err(FERN_ERR_IO)` for empty input.
   - `Ok(copy_of_text)` for non-empty input.
2. `json.stringify(text)`:
   - `Ok(copy_of_text)` for all string inputs (including empty).
3. `http.get(url)` / `http.post(url, body)`:
   - `Err(FERN_ERR_IO)` placeholder response.
4. `sql.open(path)` / `sql.execute(handle, query)`:
   - `Err(FERN_ERR_IO)` placeholder response.
5. `actors.start(name)`:
   - Returns deterministic, process-local, monotonic actor ids.
6. `actors.post(actor_id, msg)`:
   - `Ok(0)` placeholder acknowledgment.
7. `actors.next(actor_id)`:
   - `Err(FERN_ERR_IO)` when no queue/backend exists.

These behaviors are regression-tested in `tests/test_runtime_surface.c` and treated as the compatibility baseline for Gate C placeholders.

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
