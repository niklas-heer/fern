# Fern Language - Decision Log

This document tracks major architectural and technical decisions made during the development of the Fern programming language and compiler.

## Project Decision Log

### 40 Erlang-pattern hardening pass: explicit link context, deterministic supervision clock, and strategy child tables
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will harden actor supervision semantics by (1) requiring explicit current-process context for `spawn_link`, (2) adding `actors.demonitor` plus normal-exit classification (`normal`/`shutdown` do not auto-restart), (3) switching restart-intensity timing to a deterministic runtime clock API, and (4) implementing supervisor child-table-driven `one_for_all`/`rest_for_one` strategies alongside `one_for_one`.
* **Context**: The baseline supervision implementation was functionally useful but still fragile relative to Erlang/OTP behavior: implicit link-parent selection, no demonitor path, wall-clock-dependent restart windows, and no multi-child strategy semantics. The next reliability step required algorithmic behavior improvements rather than API renaming.
* **Consequences**: Runtime now exposes `fern_actor_set_current/fern_actor_self`, `fern_actor_clock_set/advance/now`, `fern_actor_demonitor`, `fern_actor_supervise_one_for_all`, and `fern_actor_supervise_rest_for_one`; checker/codegen now type-check/lower the new `actors.*` APIs; and supervisor child tables track child ids/order/strategy to drive restart targeting. Coverage is anchored by `test_runtime_actor_spawn_link_requires_current_actor_contract`, `test_runtime_actor_demonitor_stops_down_notifications_contract`, `test_runtime_actor_supervision_uses_deterministic_clock_contract`, `test_runtime_actor_supervise_one_for_all_restarts_all_children_contract`, and `test_runtime_actor_supervise_rest_for_one_restarts_suffix_contract`.

### 39 Erlang-style process lifecycle baseline: exited PIDs become dead and non-routable
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will enforce Erlang-inspired process lifecycle semantics where `fern_actor_exit` transitions a process to a dead PID state, dead actors reject send/receive/mailbox/scheduler participation, and manual `fern_actor_restart` is allowed only from dead actors.
* **Context**: Supervision intensity and monitor/restart contracts were in place, but exited actors remained routable in runtime state, which violated core process semantics and made supervision behavior less reliable. We needed a concrete lifecycle boundary so supervision algorithms operate on real process death, not soft notifications.
* **Consequences**: Runtime actor records now track alive/dead state. `fern_actor_send`, `fern_actor_receive`, `fern_actor_mailbox_len`, scheduler selection, `fern_actor_monitor`, and `fern_actor_supervise` all require live actors. `fern_actor_exit` marks actors dead before notifications/restart handling, and `fern_actor_restart` now requires a dead source actor id. Coverage is anchored by `test_runtime_actor_exit_marks_actor_dead_contract`.

### 38 Erlang-inspired actor monitoring baseline: `DOWN(...)` notifications + explicit restart primitive
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will model monitor behavior after Erlang by adding one-way monitor registrations that emit `DOWN(pid, reason)` messages on exit, while keeping linked-exit `Exit(pid, reason)` delivery and adding an explicit `actors.restart(pid)` primitive for baseline supervision workflows.
* **Context**: The prior supervision slice introduced `spawn_link` and linked exit notifications, but did not cover monitor semantics or restart APIs. We needed a practical, test-first step that reflects Erlang process semantics closely enough for adoption while staying within current runtime constraints (mailbox/scheduler baseline, no full supervisor tree policies yet).
* **Consequences**: Checker/codegen/runtime now expose `actors.monitor(Int, Int) -> Result(Int, Int)` and `actors.restart(Int) -> Result(Int, Int)`, runtime stores monitor registrations per actor, `fern_actor_exit` emits `DOWN(...)` to monitors and `Exit(...)` to linked parents, and restart returns a new actor id preserving name/link baseline and monitor registrations. Coverage is anchored by `test_check_actors_monitor_returns_result`, `test_check_actors_restart_returns_result`, `test_codegen_actors_monitor_calls_runtime`, `test_codegen_actors_restart_calls_runtime`, and `test_runtime_actor_monitor_and_restart_contract`.

### 37 Milestone 8 supervision baseline with `spawn_link` and linked `Exit(...)` notification
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will add a first supervision contract now by supporting `spawn_link(fn)` in checker/codegen and runtime linked-exit delivery via `fern_actor_exit`, with deterministic baseline linking to the most recently spawned actor id.
* **Context**: Gate D passed and roadmap focus shifted to milestone polish, but supervision remained an unstarted milestone gap even though the language design documents `spawn_link`/`Exit(...)` behavior. We needed a minimal, testable slice that introduces supervision semantics without waiting for full actor-process execution and restart machinery.
* **Consequences**: `spawn_link(...)` now type-checks and lowers to `fern_actor_spawn_link`, runtime actor records track a linked parent id, and `fern_actor_exit(actor_id, reason)` enqueues `Exit(actor_id, reason)` messages to the linked supervisor mailbox. Coverage is anchored by `test_check_spawn_link_returns_int`, `test_codegen_spawn_link_calls_runtime`, and `test_runtime_actor_spawn_link_exit_notification_contract`. Full monitor/restart policies remain future milestone work.

### 36 Civetweb runtime backend for `http.get`/`http.post` (ship now)
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will implement Fern's HTTP runtime backend using vendored civetweb now, replacing placeholder `Err(FERN_ERR_IO)` behavior for successful HTTP requests.
* **Context**: The stdlib HTTP surface (`http.get`, `http.post`) was already stabilized in checker/codegen and only lacked runtime execution. We considered layered socket/parser composition versus a single dependency and prioritized the "best option now" for maturity, auditability, and delivery speed.
* **Consequences**: Runtime now performs real HTTP client requests via civetweb and returns `Ok(response_body)` on `2xx` responses; invalid URLs, non-`2xx` responses, and transport failures return `Err(FERN_ERR_IO)`. Civetweb v1.16 is vendored under `deps/civetweb`, runtime build/link paths include civetweb + pthread/OpenSSL requirements, and runtime-surface coverage includes local loopback GET/POST success tests (`tests/test_runtime_surface.c`). HTTPS/TLS is enabled in the runtime build.

### 35 SQLite-first SQL runtime backend with libsql-compatible API surface
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will implement `sql.open` and `sql.execute` on top of SQLite (`sqlite3`) first, while keeping Fern's SQL API surface stable so we can layer or swap to libsql later without changing Fern source signatures.
* **Context**: The runtime previously returned placeholder `Err(FERN_ERR_IO)` for all SQL calls, which created a major product-surface gap even though `sql.*` type signatures were stabilized. Integrating full libsql transport/features immediately would add substantial dependency and packaging complexity. A SQLite-first backend delivers concrete local database behavior now and keeps progress aligned with current Gate C stabilization priorities.
* **Consequences**: `fern_sql_open` now returns opaque handle ids for opened SQLite connections and `fern_sql_execute` returns rows affected via `sqlite3_changes()`. Runtime/link paths now include `sqlite3` linkage, and runtime-surface tests now cover successful SQL create/insert flows plus invalid-handle errors. HTTP remains placeholder-backed until its runtime backend lands.

### 34 Gate C actor runtime core baseline: FIFO mailbox + round-robin scheduler tickets
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will implement Gate C actor runtime core as an in-memory runtime with per-actor FIFO mailboxes and a round-robin scheduler queue driven by send-time scheduler tickets, exposed through `fern_actor_spawn/send/receive/mailbox_len/scheduler_next` with `start/post/next` compatibility aliases.
* **Context**: Gate C Task 2 required concrete runtime behavior for `spawn`, `send`, `receive`, and scheduler operations, not placeholder acknowledgments. Existing actor runtime only returned monotonic ids, dropped posted messages, and returned `Err(FERN_ERR_IO)` for reads. We needed deterministic semantics that can be regression-tested immediately and used by both Fern stdlib calls and runtime C-ABI tests.
* **Consequences**: Runtime actor APIs now provide mailbox FIFO delivery and deterministic scheduler ordering (`a, b, a` shape for the covered scenario) while preserving compatibility for `actors.start/post/next`. Coverage is anchored in `tests/test_runtime_surface.c` via `test_runtime_actors_post_and_next_mailbox_contract` and `test_runtime_actor_scheduler_round_robin_contract`, and compatibility docs are updated to treat this as Gate C baseline behavior.

### 33 Step D memory-path selection for first WASM target
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will choose Perceus baseline (compiler-inserted `dup/drop` + RC headers) as the default memory path for Fern's first WASM target, keep Boehm bridge as an explicit temporary fallback for bring-up only, and defer WasmGC as a non-default option.
* **Context**: Milestone 7.7 Step D required measured comparison and a concrete default/fallback decision. The repository now has Step A-C primitives and constrained codegen insertion, but no shipping WASM backend yet. Step D measurements were captured via `scripts/compare_memory_paths.py` in `docs/reports/memory-path-comparison-2026-02-06.md`, including native perf snapshot, ownership-op microbenchmark (`fern_dup/drop` vs `fern_rc_dup/drop`), and local WASM/WasmGC feasibility probes.
* **Consequences**: The project advances to actor runtime work with memory-path direction settled for first WASM implementation. Future WASM work should implement backend/toolchain integration against Perceus default, use Boehm bridge only for short-lived bring-up, and re-run the Step D comparison artifact once real WASM binaries are produced.

### 32 Constrained Step C dup/drop insertion in codegen
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will implement initial dup/drop insertion in codegen only for a constrained, test-covered subset: pointer alias let-bindings (`let y = x`) emit `fern_dup`, and function-scope owned pointer names emit `fern_drop` at return sites with returned-identifier preservation.
* **Context**: Milestone 7.7 Step C requires proving end-to-end codegen insertion before full ownership analysis. A broad first pass (all expressions/scopes/branches) would be high-risk and hard to validate in one step.
* **Consequences**: Fern now emits semantic `dup/drop` calls for simple pointer ownership flows while keeping behavior deterministic under the current Boehm bridge. Coverage is anchored by focused codegen regression tests in `tests/test_codegen.c` (`test_codegen_dup_inserted_for_pointer_alias_binding`, `test_codegen_drop_inserted_for_unreturned_pointer_bindings`), and broader ownership inference remains future work.

### 31 Perceus object header contract for core runtime heap values
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will add a stable Perceus-style object header API (`fern_rc_alloc`, `fern_rc_dup`, `fern_rc_drop`, and metadata accessors) and tag core runtime heap allocations (`Result`, `List`, `StringList`) with explicit RC type tags.
* **Context**: Milestone 7.7 Step B requires concrete runtime object metadata and refcount operations so later codegen work can insert dup/drop in a verifiable way. Step A only established abstraction entry points (`alloc/dup/drop`) without object header semantics or typed heap metadata.
* **Consequences**: Runtime now exposes header-level refcount/type/flag queries and updates, and core heap constructors use RC-tagged allocations while memory reclamation remains Boehm-driven for now. Compatibility and C-ABI regression coverage are extended in `tests/test_runtime_surface.c` (`test_runtime_rc_header_and_core_type_ops`).

### 30 Runtime memory API: `alloc/dup/drop` abstraction with Boehm bridge
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will standardize runtime memory ownership operations on `fern_alloc`, `fern_dup`, and `fern_drop`, with `fern_free` retained as a compatibility alias to `fern_drop`.
* **Context**: Milestone 7.7 Step A requires an explicit memory abstraction surface before Perceus object headers and codegen dup/drop insertion land. The runtime already had `fern_alloc`/`fern_free`, but no ownership-duplication primitive or stable drop semantics that future RC backends can target.
* **Consequences**: Boehm-backed runtime now exposes stable `dup/drop` symbols with no-op ownership semantics under GC while preserving API shape for future RC backends. C-ABI regression coverage for this contract is added in `tests/test_runtime_surface.c`.

### 29 Stabilize Gate C placeholder runtime behavior with error-return semantics
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will make Gate C placeholder runtime functions return deterministic `Err(FERN_ERR_IO)` results for unsupported or invalid placeholder paths, instead of aborting via assertions on user-provided empty inputs.
* **Context**: Task 1 stabilization required runtime behavior to be documented and regression-tested, not only checker/codegen symbol mapping. Existing placeholder functions (`json.parse`, `json.stringify`, `http.get`, `sql.open`) aborted on empty-string inputs in debug builds, which broke API predictability and testability.
* **Consequences**: Runtime placeholder contracts are now explicit in `docs/COMPATIBILITY_POLICY.md` and covered by `tests/test_runtime_surface.c`. Empty-input behavior is stable (no abort), and future runtime implementations can evolve behind these contracts with compatibility tracking.

### 28 Standardize stdlib entry points to fs/json/http/sql/actors
* **Date**: 2026-02-06
* **Status**: ✅ Accepted
* **Decision**: I will stabilize `fs`, `json`, `http`, `sql`, and `actors` as Fern's top-level stdlib entry points, while keeping `File.*` as a compatibility alias during migration.
* **Context**: Gate C requires a predictable product surface before deeper runtime work. The compiler already exposed mixed module names (`File`, `System`, `Regex`, `Tui.*`) and lacked a consistent contract for the next stdlib modules. We need a clear API front door now so future runtime implementation work can proceed without renaming churn.
* **Consequences**: Checker/codegen now recognize the stabilized entry points and are covered by regression tests. `File.*` remains supported for compatibility and will follow the formal deprecation policy if retired. Runtime semantics for the new module surfaces can evolve behind these stable names.

### 27 WASM memory strategy: Perceus target, Boehm bridge
* **Date**: 2026-02-06
* **Status**: ✅ Accepted ⬆️ Supersedes [26]
* **Decision**: I will keep Boehm GC as the shipping memory system for native in the short term, use it only as an optional bridge for early WASM bring-up, and keep Perceus-style compile-time reference counting as Fern's long-term memory model for both native and WASM.
* **Context**: Decision [26] assumed Boehm GC could not support WASM. Upstream Boehm now includes explicit WebAssembly (`WEBASSEMBLY`) support paths for Emscripten and WASI, but with practical constraints (notably wasm32 assumptions and limited threading support). Fern still needs deterministic memory behavior, predictable pauses, and a unified actor-friendly model, which align better with Perceus. We also need an incremental path that does not block Gate C work.
* **Consequences**: Milestone 7.7 becomes a concrete engineering spike with exit criteria (prototype both Boehm-on-WASM and Perceus runtime shape, compare pause behavior, binary size, and implementation risk). Decision [26] is superseded for the "Boehm cannot support WASM" claim, but Perceus remains the preferred end-state.

### 26 Perceus-style reference counting for WASM support
* **Date**: 2026-01-29
* **Status**: 🔄 Superseded by [27]
* **Decision**: I will implement Perceus-style compile-time reference counting as Fern's long-term memory management strategy, replacing Boehm GC for both native and WASM targets.
* **Context**: Boehm GC works well for native targets but cannot support WASM (relies on stack scanning and OS features). Considered several alternatives: (1) Rust-style ownership - powerful but steep learning curve, (2) Swift ARC - requires manual weak references for cycles, (3) WasmGC - ties us to browser GC, may have pauses, (4) Perceus (from Koka/Roc) - reference counting with reuse optimization. Chose Perceus because: functional purity eliminates cycles (no weak refs needed), zero developer annotations required, works identically on native and WASM, no GC pauses, and enables "functional but in-place" optimization where unique values are mutated behind the scenes.
* **Consequences**: Created `docs/MEMORY_MANAGEMENT.md` with detailed design. Implementation in phases: (1) Keep Boehm for now, (2) Add Perceus for WASM target, (3) Replace Boehm everywhere, (4) Add reuse optimization. Compiler will insert dup/drop operations automatically. Developers write pure functional code; compiler figures out optimal memory strategy.

### 25 Four Pillars philosophy - joy, one way, no surprises, jetpack
* **Date**: 2026-01-29
* **Status**: ✅ Adopted
* **Decision**: I will design Fern around four core pillars: (1) Spark Joy - FP should feel delightful, (2) One Obvious Way - avoid "many ways to do it" confusion, (3) No Surprises - prevent bugs that waste debugging time, (4) Jetpack Included - batteries included like Bun/Elixir.
* **Context**: Needed to articulate what makes Fern distinctive beyond just "functional + Python syntax". The four pillars capture the user experience goals: joy for FP practitioners, clarity for teams, safety by default, and productivity through included batteries. This philosophy influences every design decision - from syntax choices to stdlib scope to error messages.
* **Consequences**: README and DESIGN.md updated with philosophy. All future features evaluated against these pillars. "No surprises" particularly important - we actively prevent null, unhandled errors, race conditions, silent failures. "One obvious way" means we document idioms clearly and avoid redundant features. "Jetpack" means stdlib includes actors, DB, HTTP, TUI, CLI tools - not just basics.

### 24 Tui.* nested module namespace for terminal UI
* **Date**: 2026-01-29
* **Status**: ✅ Adopted
* **Decision**: I will organize all terminal UI modules under a `Tui.*` namespace (e.g., `Tui.Panel`, `Tui.Table`, `Tui.Style`) instead of using flat top-level module names.
* **Context**: The original flat module names (`Panel`, `Table`, `Style`, `Status`, `Live`, `Progress`, `Spinner`, `Term`) were ambiguous - `Style` could mean anything, `Panel`/`Table` aren't clearly TUI-related at a glance, and `Term` (terminal capabilities) belongs with other TUI modules. Considered two approaches: (1) Keep flat names - simple but poor organization and discoverability, (2) Nested `Tui.*` namespace - groups related functionality, follows Elixir's convention (e.g., `Phoenix.HTML`, `Ecto.Query`), makes it clear these are terminal UI modules. The nested approach also prepares the language for future namespace organization (e.g., `Http.*`, `Json.*`, `Crypto.*`). Implemented proper nested module support using `try_build_module_path()` helper that recursively builds paths from dot expressions, enabling arbitrary nesting depth.
* **Consequences**: All TUI modules renamed: `Term` → `Tui.Term`, `Panel` → `Tui.Panel`, `Table` → `Tui.Table`, `Style` → `Tui.Style`, `Status` → `Tui.Status`, `Live` → `Tui.Live`, `Progress` → `Tui.Progress`, `Spinner` → `Tui.Spinner`. Both checker.c and codegen.c updated to recognize nested module paths. Examples updated to use new namespace. The module system now supports arbitrary nesting (e.g., `Tui.Style.Colors` would work if implemented). DESIGN.md updated with comprehensive Tui module documentation.

### 23 Embedded QBE compiler backend
* **Date**: 2026-01-29
* **Status**: ✅ Adopted
* **Decision**: I will embed QBE directly into the fern binary rather than requiring it as an external dependency.
* **Context**: The fern compiler was calling external `qbe` binary via system() which required users to install QBE separately. This conflicted with the "single binary" philosophy. Considered options: (1) Keep external qbe - simple but adds dependency, (2) Embed QBE source - removes dependency, single binary, (3) Use LLVM - powerful but massive dependency, (4) Write custom backend - flexible but huge effort. QBE is only ~6,650 lines of C with no dependencies, making it ideal for embedding. Modified QBE's main.c to expose `qbe_compile()` library function.
* **Consequences**: QBE source added to `deps/qbe/` (~16 files, 6.6K lines). Fern binary increased from ~200KB to ~540KB. Users no longer need to install qbe. The fern binary is now fully self-contained for development - only needs a C compiler (cc/clang) for assembling and linking, which is standard on all Unix systems.

### 22 Boehm GC for automatic memory management
* **Date**: 2026-01-29
* **Status**: ✅ Adopted
* **Decision**: I will use Boehm GC for automatic garbage collection in Fern programs, with a future path to BEAM-style per-process heaps when actors are implemented.
* **Context**: Fern's immutable-first, functional style generates many intermediate values (strings, lists, etc.) that need automatic memory management. Considered several approaches: (1) Manual memory management - error-prone, leaks inevitable, (2) Reference counting - works but has cycles problem and overhead, (3) Boehm GC - conservative, drop-in replacement for malloc, proven in production, (4) Custom tracing GC - complex, takes months to implement well, (5) BEAM-style per-process heaps - ideal for actors but requires actor runtime first. Chose Boehm GC as the pragmatic v1 solution: ~100 lines of integration, zero memory leaks, works with C FFI. When actors are added (Milestone 8), we'll transition to per-process heaps where each actor has its own GC'd heap - this eliminates global GC pauses and enables instant memory reclamation on process death.
* **Consequences**: Runtime uses `GC_MALLOC` instead of `malloc`. All `_free()` functions become no-ops. Compiled programs link with `-lgc`. Requires `brew install bdw-gc` (macOS) or `apt install libgc-dev` (Linux). Binary size increased ~20KB. No measurable performance impact in benchmarks. Future actor runtime will use per-process heaps with generational collection within each process.

### 21 Built-in module syntax for standard functions
* **Date**: 2026-01-29
* **Status**: ✅ Adopted
* **Decision**: I will use `Module.function()` syntax for built-in functions instead of flat names like `str_len()`, organized into `String`, `List`, `File`, `Result`, and `Option` modules.
* **Context**: The original flat naming convention (`str_len`, `str_concat`, `list_get`, `file_read`) works but has discoverability issues. Users can't easily find what functions are available without memorizing prefixes. Considered two approaches: (1) Keep flat names - simple but poor discoverability, (2) Module-qualified syntax `String.len()` - familiar from many languages, enables LSP autocomplete on `String.`, groups related functions clearly. The module approach is foundational for the language to "feel right" and prepares for future LSP integration where typing `String.` shows all available string functions.
* **Consequences**: Added `is_builtin_module()` and `lookup_module_function()` in checker.c. Updated EXPR_DOT handling to recognize module access. Added codegen support for module.function calls. Old flat names still work for backwards compatibility. All examples updated to use new syntax. DESIGN.md documents the built-in modules. Future: deprecation warnings for old syntax, eventual removal.

### 20 Optional return type for main() (Rust-style)
* **Date**: 2026-01-29
* **Status**: ✅ Adopted
* **Decision**: I will allow omitting the return type for `main()` only, defaulting to Unit with automatic `ret 0`.
* **Context**: Writing `fn main() -> Int: 0` for simple programs that don't need a return value is tedious. Rust allows both `fn main()` (Unit return) and `fn main() -> Result<(), E>` (explicit return). We adopt a similar approach: `fn main():` defaults to Unit return and auto-returns 0 (success exit code), while `fn main() -> Int:` requires an explicit integer return. This special case applies ONLY to main() - other functions still require explicit return types or use type inference. This provides ergonomic shorthand for scripts and simple programs while maintaining explicitness for library code.
* **Consequences**: The type checker treats `main()` with no return type as returning Unit. The code generator emits `ret 0` for main() with Unit return. Both `fn main():` and `fn main() -> Int:` are valid. Other functions are unaffected.

### 19 Deterministic simulation testing for actors (FernSim)
* **Date**: 2026-01-28
* **Status**: ✅ Accepted
* **Decision**: I will implement deterministic simulation testing (FernSim) for the actor runtime and supervision trees, inspired by TigerBeetle's VOPR and FoundationDB's simulation testing.
* **Context**: To achieve BEAM-level reliability for Fern's actor system, real-world testing is insufficient - it would take years to hit rare edge cases. Deterministic simulation can explore millions of process scheduling interleavings, inject faults (crashes, timeouts, message loss), and reproduce any bug with a seed. TigerBeetle found critical bugs in 3 weeks that would have taken 5+ years to find in production. FoundationDB credits simulation testing for their legendary reliability.
* **Consequences**: FernSim will be a core part of Milestone 8 (Actor Runtime). The actor scheduler must support both real execution and simulated execution with a deterministic PRNG. All supervision strategies (one_for_one, one_for_all, rest_for_one) will be verified against fault injection. CI will run simulation tests on every PR. Success criteria: 1M+ simulated steps with zero invariant violations before release.

### 18 HexDocs-style documentation generation
* **Date**: 2026-01-28
* **Status**: ✅ Accepted
* **Decision**: I will implement automatic documentation generation with two systems: (1) a built-in `fern doc` command for Fern code that generates HTML from `@doc` comments, and (2) a custom doc generator for the C compiler source code.
* **Context**: Good documentation is essential for adoption. Considered several approaches: (1) Doxygen for C code - industry standard but dated look, (2) Sphinx + Breathe - modern but complex setup, (3) Custom solution - tailored to our needs. For Fern language docs, a built-in command like `cargo doc` or `mix docs` provides the best developer experience. For compiler docs, a custom solution lets us match FERN_STYLE conventions and maintain a consistent look across both documentation systems.
* **Consequences**: Need to implement `fern doc` command that parses `@doc` comments and generates searchable HTML. Need to build a C doc extractor that understands our comment conventions. Both should share HTML templates for consistent styling. Documentation generation will be added to CI to keep docs up-to-date.

### 17 Unicode and emoji identifiers
* **Date**: 2026-01-28
* **Status**: ✅ Accepted
* **Decision**: I will allow Unicode letters and emojis as valid variable/function names, following Unicode identifier standards (XID_Start/XID_Continue) plus emoji support.
* **Context**: The question arose whether to restrict identifiers to ASCII or allow broader Unicode. Considered: (1) ASCII-only - simple but excludes international developers and mathematical notation, (2) Unicode letters only (XID categories) - allows π, θ, non-Latin scripts but not emojis, (3) Full Unicode + emojis - maximum expressiveness. Chose option 3 because it's the developer's choice to use identifiers responsibly. Languages like Swift and Julia allow emoji identifiers. While there are practical concerns (typing difficulty, rendering inconsistency, searchability), these are tradeoffs developers can evaluate for themselves.
* **Consequences**: The lexer must recognize Unicode XID_Start/XID_Continue categories plus emoji codepoints as valid identifier characters. DESIGN.md will document identifier rules. Test cases will verify Unicode identifiers work correctly (e.g., `let π = 3.14159`, `let 🚀 = launch()`).

### 16 Adopting TigerBeetle-inspired FERN_STYLE
* **Date**: 2026-01-28
* **Status**: ✅ Adopted
* **Decision**: I will adopt a coding style guide inspired by TigerBeetle's TIGER_STYLE, with emphasis on assertion density, function size limits, and fuzzing-friendly code.
* **Context**: TigerBeetle's engineering practices produce extremely reliable code through: (1) minimum 2 assertions per function, (2) 70-line function limit, (3) pair assertions for critical operations, (4) compile-time assertions, (5) explicit bounds on everything. These practices align well with AI-assisted development because they make invariants explicit, keep functions small enough for AI context windows, and enable effective fuzzing. The VOPR-style deterministic simulation testing approach will be adapted as FernFuzz for grammar-based compiler testing.
* **Consequences**: Created FERN_STYLE.md as the coding standard. All code must meet assertion density requirements. Functions over 70 lines must be split. Fuzzing infrastructure (FernFuzz) will be added to test lexer/parser with random programs. CI will enforce style compliance.

### 15 No named tuples (use records for named fields)
* **Date**: 2026-01-28
* **Status**: ✅ Adopted
* **Decision**: I will not support named tuple syntax `(x: 10, y: 20)`. Use positional tuples `(10, 20)` or declared records for named fields.
* **Context**: Named tuples create confusion because they look like records but aren't declared types. Users wouldn't know when to choose named tuples vs records. Keeping a clear distinction simplifies the mental model: tuples are positional and anonymous `(a, b, c)`, records are declared with `type` and have named fields. If you need named fields, declare a type.
* **Consequences**: Tuple syntax is positional only: `(10, 20)`. Named fields require a `type` declaration. Simpler grammar, clearer semantics, no ambiguity about tuple vs record.

### 14 No unless keyword
* **Date**: 2026-01-28
* **Status**: ✅ Adopted
* **Decision**: I will not include `unless` as a keyword. Use `if not` for negated conditions.
* **Context**: `unless` (from Ruby) is redundant with `if not` and adds cognitive overhead. Developers must mentally negate the condition to understand `unless`. It's especially confusing with already-negated conditions: `unless not ready`. Most Ruby style guides recommend avoiding `unless` with negations. Having one way to express conditionals (`if`) keeps the language simpler.
* **Consequences**: Only `if` for conditionals. Postfix conditionals use `if`: `return early if condition`. Negation uses `if not condition`. One less keyword to parse and teach.

### 13 No while or loop constructs (Gleam-style)
* **Date**: 2026-01-28
* **Status**: ✅ Adopted
* **Decision**: I will not include `while` or `loop` constructs. Stateful iteration uses recursion with tail-call optimization.
* **Context**: `while` and `loop` require mutable state between iterations, which conflicts with immutability. The semantics of rebinding inside loops are unclear and error-prone. Gleam takes the same approach: no loops, use recursion. Tail-call optimization makes recursion efficient. `for` loops over collections are kept since they don't require mutation - they're just iteration. Functional combinators (`fold`, `map`, `filter`, `find`) handle most cases elegantly.
* **Consequences**: Remove `while` and `loop` from grammar. Keep `for` for collection iteration. Recursion is the primary mechanism for stateful iteration. The lexer doesn't need TOKEN_WHILE or TOKEN_LOOP. Simplifies the language considerably.

### 12 Elixir-style record update syntax
* **Date**: 2026-01-28
* **Status**: ✅ Adopted
* **Decision**: I will use `%{ record | field: value }` syntax for record updates instead of `{ record | field: value }`.
* **Context**: The original `{ record | field: value }` syntax conflicts with the "no braces" philosophy - Fern uses indentation, not braces, for control flow. Using `%{...}` for record updates matches map literal syntax `%{"key": value}` and is inspired by Elixir. This creates consistency: both maps and record updates use `%{...}`.
* **Consequences**: Record update syntax is `%{ user | age: 31 }`. Map literals are `%{"key": value}`. Braces without `%` are not used.

### 11 Using ? operator for Result propagation
* **Date**: 2026-01-28
* **Status**: ✅ Adopted ⬆️ Supersedes [10]
* **Decision**: I will use the `?` operator (Rust-style, postfix) for Result propagation, keeping `<-` only inside `with` expressions.
* **Context**: After writing real examples, the postfix `?` works better than prefix `<-` because: (1) you see WHAT might fail before the `?`, not after, (2) it's familiar from Rust which is widely known, (3) it chains naturally `foo()?.bar()?.baz()?`, (4) it integrates cleanly with `let` bindings: `let x = fallible()?`. The `<-` syntax is preserved only inside `with` blocks for complex error handling, similar to Haskell's do-notation where `<-` is scoped.
* **Consequences**: The lexer needs `?` as TOKEN_QUESTION. The `<-` token (TOKEN_BIND) is only valid inside `with` blocks. Simple error propagation uses `let x = f()?`, complex handling uses `with x <- f(), ...`.

### 10 Using <- operator instead of ?
* **Date**: 2026-01-27
* **Status**: ⛔ Deprecated by [11]
* **Decision**: I will use the `<-` operator for Result binding instead of the `?` operator.
* **Context**: Initially considered Rust's `?` operator (postfix), but this has clarity issues: (1) it comes at the END of the expression, so you don't immediately see that an operation can fail, (2) `?` is overloaded in many languages (ternary, optional, etc.), making it less obvious. The `<-` operator (from Gleam/Roc) addresses both issues: it comes FIRST so failure is immediately visible, it reads naturally as "content comes from read_file", and it's not overloaded with other meanings.
* **Consequences**: All error handling examples use `<-` syntax. The lexer must recognize `<-` as a distinct token. Error messages reference `<-` in explanations.

### 9 No panics or crashes
* **Date**: 2026-01-27
* **Status**: ✅ Adopted
* **Decision**: I will eliminate all panic mechanisms from Fern - programs never crash from error conditions.
* **Context**: Many languages (Rust, Go, Swift) include panic/crash mechanisms for "impossible" errors. However, panics are the worst possible behavior: they're unpredictable, lose all error context, and can't be recovered from. In server scenarios, a panic can take down the entire application. Instead, ALL errors must be represented as `Result` types that force handling. There is no `.unwrap()`, no `panic()`, no `assert()` in production code.
* **Consequences**: The compiler must enforce that all `Result` values are handled. Error types must be comprehensive enough to represent all failure modes. Standard library functions that might fail must return `Result`. Debug builds can use `debug_assert()` for development.

### 8 Actor-based concurrency model
* **Date**: 2026-01-27
* **Status**: ✅ Adopted
* **Decision**: I will use an actor-based concurrency model (Erlang/Elixir style) instead of threads, async/await, or channels.
* **Context**: Considered several options: (1) OS threads - too heavy, difficult to reason about shared state, (2) async/await - complex, color functions, can't block, (3) Go-style goroutines with channels - better but still allows shared memory bugs, (4) Actor model - isolated processes with message passing only, no shared memory, supervisor trees for fault tolerance. The actor model provides the best balance of safety and expressiveness. Even though it adds ~500KB to binary size, this is acceptable given the safety and capability benefits (can replace Redis, RabbitMQ in many cases).
* **Consequences**: Need to implement a lightweight process scheduler, message queues, and supervision trees. All concurrent code uses message passing. The runtime will be slightly larger but provides superior reliability.

### 7 Labeled arguments for clarity
* **Date**: 2026-01-27
* **Status**: ✅ Adopted
* **Decision**: I will require labeled arguments for same-type parameters and all Boolean parameters.
* **Context**: Function calls like `connect("localhost", 8080, 5000, true, false)` are impossible to understand without checking the definition. Which number is the port? What do those booleans mean? Labeled arguments solve this: `connect(host: "localhost", port: 8080, timeout: 5000, retry: true, async: false)` is immediately clear. The compiler enforces labels when (1) multiple parameters have the same type, or (2) any parameter is a Boolean, preventing ambiguous calls.
* **Consequences**: Function calls are more verbose but dramatically more readable. The parser must support labeled argument syntax. The type checker must enforce label requirements.

### 6 with expression for complex error handling
* **Date**: 2026-01-27
* **Status**: ✅ Adopted
* **Decision**: I will provide a `with` expression for complex error handling scenarios where different error types need different responses.
* **Context**: While `?` handles simple error propagation, sometimes you need to handle different errors differently (e.g., return 404 for NotFound, 403 for PermissionDenied, 401 for AuthError). The `with` expression allows binding multiple Results using `<-` and pattern matching on different error types in an `else` clause, similar to Haskell's do-notation.
* **Consequences**: The parser must support `with`/`do`/`else` syntax. The `<-` operator is only valid inside `with` blocks. The type checker must verify all error types are handled in the else clause.

### 5 defer statement for resource cleanup
* **Date**: 2026-01-27
* **Status**: ✅ Adopted
* **Decision**: I will add a `defer` statement (from Zig) for guaranteed resource cleanup.
* **Context**: Resource cleanup (closing files, freeing locks, etc.) must be reliable even when errors occur. Considered: (1) try/finally blocks - verbose and easy to forget, (2) RAII/destructors - implicit, hard to see cleanup order, (3) `defer` statement - explicit, clear cleanup order (reverse of declaration), always runs on scope exit. Defer makes cleanup visible and guaranteed without ceremony.
* **Consequences**: The compiler must track defer statements and ensure they execute on all exit paths (return, error, normal). Deferred calls execute in reverse order of declaration.

### 4 Doc tests for reliability
* **Date**: 2026-01-27
* **Status**: ✅ Adopted
* **Decision**: I will support doc tests where examples in `@doc` comments are automatically tested.
* **Context**: Documentation often becomes stale because examples aren't verified. Rust's doc tests solve this by making documentation runnable and testable. This ensures examples always work and documentation stays current. It's especially valuable for AI-assisted development where examples serve as additional test cases.
* **Consequences**: The test runner must extract code blocks from `@doc` comments and execute them. Examples must be valid Fern code. Failed doc tests fail the build.

### 3 Python-style indentation syntax
* **Date**: 2026-01-26
* **Status**: ✅ Adopted
* **Decision**: I will use significant indentation (Python-style) instead of braces or `end` keywords.
* **Context**: Readability is a primary goal. Compared options: (1) Braces `{}` - familiar but add visual noise, (2) `end` keywords - clear but verbose, (3) Significant whitespace - clean and minimal. Python proves indentation works at scale. Modern editors handle indentation well. The reduced visual noise improves readability significantly.
* **Consequences**: The lexer must track indentation levels and emit INDENT/DEDENT tokens. Mixed tabs/spaces must be rejected. Error messages must handle indentation errors clearly.

### 2 Implementing compiler in C with safety libraries
* **Date**: 2026-01-26
* **Status**: ✅ Adopted
* **Decision**: I will implement the Fern compiler in C11 with safety libraries (arena allocator, SDS strings, stb_ds collections, Result macros) instead of Rust, Zig, or C++.
* **Context**: Considered several implementation languages: (1) Rust - safe but complex, steep learning curve, slower compile times, (2) Zig - interesting but immature, fewer AI training examples, (3) C++ - too complex, many ways to do things wrong, (4) C with safety libraries - simple, well-understood by AI, fast compilation, full control. Using arena allocation eliminates use-after-free and memory leaks. Using SDS eliminates buffer overflows. Using Result macros eliminates unchecked errors. C is also extremely well-represented in AI training data, making AI-assisted development highly effective.
* **Consequences**: Must use arena allocator exclusively (no malloc/free). Must use SDS for all strings. Must use stb_ds for collections. Must use Result types for fallible operations. Compiler warnings must be treated as errors.

### 1 Compiling to C via QBE
* **Date**: 2026-01-26
* **Status**: ✅ Adopted
* **Decision**: I will compile Fern to C using `QBE` as an intermediate representation.
* **Context**: I considered three approaches: (1) `LLVM` - powerful but extremely complex with 20+ million lines of code and steep learning curve, (2) Direct machine code generation - too low-level and platform-specific, requiring separate backends for each architecture, (3) `QBE` - a simple SSA-based IL that compiles to C. QBE hits the sweet spot: it's only ~10,000 lines of code (AI can understand it fully), generates efficient C code, handles register allocation and optimization, and lets me target any platform C supports. The C output can then be compiled with any C compiler (gcc, clang, tcc) for maximum portability.
* **Consequences**: I need to generate QBE IL from Fern AST, then invoke QBE to produce C code, and finally compile the C code to native binaries. This adds an extra compilation step but dramatically simplifies the compiler implementation and ensures broad platform support. Single binaries under 1MB are still achievable.
