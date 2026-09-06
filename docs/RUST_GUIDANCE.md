# Rust guidance adoption (Decision107)

Fern keeps production Rust 1.75, stable Rust, safe owned representations and a
standard-library-only compiler. This checkpoint adds strict incremental lint
checks and an independently locked Criterion developer workspace. It fixes an
actual directory-discovery boundary and unnecessary allocations without changing
language behavior or updating the production dependency graph.

## Verification and development workflow

The mise migration in Decision106 owns tool provisioning, CI, watchers and
nextest. Use [Development tasks and tools](DEVELOPMENT_ENVIRONMENT.md) for exact
pins and installation boundaries; no Nix/devenv or replacement Justfile is added.
Decision107 contributes `rust-lint-policy`, `rust-bench-smoke` and optional
`rust-bench` tasks. The underlying commands also work directly:

```sh
python3 scripts/test_rust_lint_policy.py
cargo fmt --manifest-path compiler-rs/Cargo.toml --all -- --check
cargo check --manifest-path compiler-rs/Cargo.toml --locked --all-targets --all-features
cargo clippy --manifest-path compiler-rs/Cargo.toml --locked --all-targets --all-features -- -D warnings
cargo test --manifest-path compiler-rs/Cargo.toml --locked --all-features
cargo test --manifest-path compiler-rs/Cargo.toml --locked --doc
cargo bench --manifest-path benchmarks/compiler-phases/Cargo.toml --locked --bench phases -- --test
```

The isolated checkpoint ran all of these on Rust 1.75, plus the benchmark
package's tests/Clippy, offline build and an actual statistical baseline.
Decision106 separately verified nextest, Bacon check/Clippy and a watchexec
source-change dispatch; nextest does not replace documentation tests.
The combined macOS/Linux integration also passed the complete Rust/native/C
quality gates, native cache/ownership suite and documentation checks. Nextest ran
1409 tests on macOS and1410 on Linux, all passing without skips; three Rust
documentation tests ran separately. Linux provisioned the pinned mise/Python/uv
tools and rust-src successfully. The workflow gate includes23 task, literal-flag
and script-lock checks; Criterion fixtures and all10 smoke phases pass on both.

## Enforced lint policy

Cargo's package lint table is supported by the existing toolchain; it does not
require a compiler upgrade. [Cargo reference](https://doc.rust-lang.org/cargo/reference/manifest.html#the-lints-section)

| Scope | Policy and evidence |
| --- | --- |
| Entire compiler package | Deny `dbg_macro`, `todo`, `unimplemented`, `exit`, `unchecked_duration_subtraction`, `unused_peekable`, `redundant_clone`, `or_fun_call`. All-target Clippy passes. |
| Production library and binary | Deny `panic` and `panic_in_result_fn` via `cfg_attr(not(test), ...)`; intentional test assertions/panics remain available. |
| Source-directory input boundary | Deny `pedantic`, `nursery`, `unwrap_used`, `expect_used`, `indexing_slicing`, `as_conversions`, `unreachable`, `string_slice`, `arithmetic_side_effects`. This is an audited first module, not a claim of whole-compiler compliance. |
| Narrow allowances | Discovery counter/depth arithmetic has explicit small bounds (8193 and 33); one documented function allowance preserves those validated operations. Explicit caller visibility has a separate `redundant_pub_crate` allowance. No crate-wide group allowance masks failures. |

The policy test compiles 17 deliberately bad, dependency-free temporary crates
under the selected toolchain and checks each expected diagnostic, then compiles
a checked positive and a test-only panic without executing either fixture. It rejects weakened manifest entries and unsupported lint
names. It uses offline Cargo and does not execute the panic/exit fixtures.

The requested `unchecked_time_subtraction` spelling is unavailable in 1.75.
Its applicable predecessor is `unchecked_duration_subtraction`, specifically
`Instant - Duration`; it does not promise to diagnose every time arithmetic
operation. The test pins that exact behavior against the
[pinned Clippy implementation](https://github.com/rust-lang/rust-clippy/blob/rust-1.75.0/clippy_lints/src/instant_subtraction.rs).
The supplied `allow-panic-in-tests` configuration is also unsupported on this
MSRV. Explicit production attributes provide the intended separation without
unknown configuration keys. No unused test-exception configuration is added.

A broad audit found over a thousand arithmetic warnings and hundreds of indexed
accesses, many in bounded IR/ABI traversal. Enabling every restriction globally
would require a separate invariant audit, rather than mechanically replacing
validated accesses or adding blanket allowances. The next modules should follow
the same rule: capture a real failing boundary test, fix the defect, and enable
the restriction with narrowly justified exceptions.

The directory pilot caught a real defect: a starting path over 4096 bytes was
copied and passed to filesystem lookup before the child-path limit applied.
It now rejects that input before copying or formatting an oversized OS error.
Other fixes remove unnecessary clones, eagerly created fallback strings/errors,
and a peekable iterator that never peeks. Existing semantic regressions remain
unchanged except removal of redundant test clones required by the same policy.

## Tools and API choices

| Recommendation | Decision and concrete reason |
| --- | --- |
| rustfmt, Clippy, unit/integration/doc tests, CI | Retained and exercised; mise provides focused and aggregate tasks. Strict warnings remain the enforcement boundary. |
| Rust Analyzer | Documented editor setup in Decision106; no user-editor installation. rust-src is a separate managed component, not a language server. |
| Bacon | Optional project-local tool, built with its separate toolchain and tested against compiler 1.75 by Decision106. It provides an interactive view beyond the simple watcher. |
| cargo-nextest | Optional pinned prebuilt runner, actually tested with 1.75. No retries conceal failing tests; doctests remain a separate command. |
| watchexec | Optional bounded check workflow, source-change behavior tested. It does not launch arbitrary Fern programs or alter dependencies. |
| Criterion | Added as a dev-dependency only in `benchmarks/compiler-phases`; pinned 0.5.1 and its own 1.75-tested lock, 10 real phase measurements and fixture oracles. [Methodology](../benchmarks/compiler-phases/README.md) |
| cargo-generate | Not installed: this repository maintains one compiler and no reusable starter-project template. There is no current scaffolding operation to exercise or validate. Revisit when a Fern application/extension template exists. |
| cargo-seek | Not installed: this change has one known developer dependency, verified against its official manifest and real locked builds. No ongoing interactive crate-discovery workflow justifies another executable; reconsider during actual dependency selection. |
| Typestate | Existing private probe/editor/codec-template tokens and publication validators express meaningful construction/execution boundaries and have compile-fail tests. No generic state wrapper is added to ordinary mutable checker state without an operation it would make impossible. |
| Nightly | Not needed for the measured phases, lint policy or compiler. Zed/developer-tool compilers and the experimental backend probe remain explicitly separate from the production MSRV. |
| Git hooks | Optional convenience; CI/task checks enforce the contract. No automatic hook installation or dependency updates on entering the environment. |

## Crate selection

The absence of a production dependency is a scoped capability decision, not a
rule against libraries. Reassess MSRV, maintenance, feature cost and actual API
fit when the capability is introduced. This checkpoint makes no unsupported
claim that an omitted crate is unsafe, unmaintained or intrinsically unsuitable.

| Suggested crate | Current decision |
| --- | --- |
| `color-eyre` | Existing public `Diagnostic` values and exact CLI/LSP source ranges must stay structured. No new application error-reporting boundary needs a second report type. |
| `itertools` | The audited operations use direct standard iterators; no awkward custom iterator algorithm was found that this change would replace. |
| `rayon` | No measured parallel workload is being implemented. Shared checker budgets/order and small fixture overhead need design and measurements before parallel execution. Criterion default Rayon is disabled too. |
| `serde` + derive | Fern's wire JSON codec operates on Fern types and native/REPL representations with exact number/error/resource contracts; Rust derive does not implement that contract. Criterion uses serde transitively for its developer reports only. |
| `clap` + derive | CLI parity has an independent Decision108 scope. Changing parser diagnostics/streams/exit behavior here would break that boundary. Criterion uses its own locked CLI dependency in the developer package. |
| Chrono / Jiff | Compiler timing uses monotonic `Instant`; no calendar, timezone or date parsing API needs either crate. |
| `cmd_lib` | Existing process capture has tested argv identity, deadlines, aggregate output limits, process ownership and cleanup. No demonstrated benefit warrants replacing it with shell-oriented execution. |
| `utoipa` | There is no Rust HTTP/OpenAPI service to describe. LSP JSON-RPC is not an OpenAPI endpoint. |
| `reqwest` / rustls | Compilation and LSP have no new outbound HTTP operation. Native Fern runtime networking does not make the compiler itself an HTTP client. |
| `sqlx` | The compiler does not query a database or own migrations. Runtime SQLite bindings are a different layer. |
| Leptos / Trunk | No Rust web frontend is being built. Editor grammar WASM and test drivers are not a web application. |
| Dioxus | No cross-platform application UI is part of this compiler checkpoint. |
| Tauri | Existing Zed extension/LSP packaging has its own host; introducing another desktop shell would not improve that integration. |

The production Cargo manifest and lockfile remain dependency-free. Criterion's
lock is reviewed independently and default plotting/parallel/async features are
disabled. No credentials, service defaults, production URLs, global installs or
automatic dependency updates are introduced.

## Remaining scope

Broader pedantic/restriction coverage requires more bounded module audits.
Statistical phase measurements need quiet-host before/after runs before any
performance claim; smoke tests alone prove operation, not speed. The baseline
records a development run with possible concurrent host activity. Backend
replacement evaluation, CLI parity, actors, recursive Result proofs and future
JSON features retain their own tests and decisions. No optional tool installation
or platform test that was not actually run is counted as completed here.
