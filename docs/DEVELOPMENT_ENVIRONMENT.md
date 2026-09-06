# Development tasks and tools

Use `mise run <task>` from the checkout. `mise tasks ls` lists the available
commands. The former task names remain available, including `debug`, `release`,
`test`, `check`, `rust-build`, `rust-check`, documentation, fuzzing, editor and
release gates. Just and mask are not required. No Nix or devenv configuration is
introduced.

## Setup

Install mise using [its official instructions](https://mise.jdx.dev/installing-mise.html).
CI pins mise 2026.9.1 and the mise-action commit; the project accepts that version
or newer. Review the checkout, trust its configuration when requested, then run:

```sh
mise install
mise run tool-versions
mise run rust-build
mise run rust-check
```

Shell activation is optional. `mise run` and `mise exec -- <command>` select the
project tools directly. Tool installation is explicit in CI; merely opening a
shell does not run builds, mutate Cargo dependencies or install Git hooks.

The managed baseline is Rust **nightly-2026-09-06** (Rust 1.100.0-nightly),
Python **3.14.7**, and uv **0.12.5**.
Python matches the existing CPython 3.14 argparse reference contract. `UV_PYTHON`
selects that exact interpreter; uv cannot silently download a different Python.
The root `rust-toolchain.toml` matches mise and selects the dated nightly with
its rustfmt, Clippy and rust-src components for direct Cargo commands too.
Fern no longer promises Rust 1.75 compatibility. Cargo's numeric `rust-version =
"1.100"` is a coarse minimum check; it cannot encode a nightly date and does not
promise support for an untested stable compiler. The dated nightly is the
supported build policy. Edition 2021 is unchanged. The default compiler uses
the standard library; Decision112 adds pinned optional Cranelift dependencies
for native code generation. [Cargo version semantics](https://doc.rust-lang.org/cargo/reference/rust-version.html)

Install your editor's Rust Analyzer extension (for VS Code,
`rust-lang.rust-analyzer`; Zed includes Rust support), and launch it from the
mise environment. The extension supplies the language server; rust-src supplies
standard-library sources and is not itself a language server. No editor
extension is silently installed by project setup.
Mise selects Rust through rustup without changing its global default. Zed
packaging tasks explicitly select their separate pinned Rust 1.97.1 within the
package process, preserving dedicated RUSTUP_HOME/CARGO_HOME environments.
[Backend behavior](https://mise.jdx.dev/lang/rust.html)

Install the host's native dependencies separately as described in [BUILD.md](../BUILD.md):
Clang, Bash, pkg-config, Boehm GC, SQLite and OpenSSL development files. Optional
C formatting and memory tasks also need clang-format and Valgrind respectively.
The vendored QBE sources are built from the checkout. `tool-versions` verifies
managed pins and reports actual native compiler/library versions. It compares
the active rustc/Cargo verbose identities and rustfmt/Clippy versions against
the installed dated toolchain, and requires rust-src to be present.

## Required and focused checks

```sh
mise run check                 # Native C build/test/style and compatibility gates
mise run rust-check            # All Rust safety and native/differential gates
mise run rust-fmt              # Non-writing Rust format check
mise run rust-compile-check    # All targets/features, locked Cargo dependencies
mise run rust-clippy           # Warnings are errors
mise run rust-test             # Unit/integration/doc tests
mise run rust-doc-test         # Focused Rust documentation tests
mise run mise-workflow-check   # Task inventory, ordering and failure contracts
```

Native builds share `build/` and `bin/`, so the default task concurrency is one.
Composite commands explicitly order cleanup, builds and consumers; they do not
schedule clean/build as independent parallel prerequisites. Avoid overriding
this with concurrent native builds in the same checkout. `mise run --skip-deps`
is available for staging/testing an already built artifact. Task arguments are
literal argv; shell environment paths such as `PREFIX` and `DESTDIR` remain
quoted. Dry-run prints the command with those environment references unexpanded.

`scripts/build_config` owns the native flags and source lists. Both mise build
scripts and the cached native style-checker bootstrap read that same authored
configuration. Build helpers decode quoted/escaped flag records without shell
expansion, including pkg-config include paths, and reject malformed or oversized
records before invoking a compiler. The cache fingerprints its bytes, mise configuration and actual
native inputs. Native `style`, `style-lenient` and `pre-commit` checks still need
neither Python nor Cargo inside the checker at execution time. Mise still
resolves/provisions its configured project tools before running a task; use
`./scripts/check_style --style-only src lib` directly when only the native
environment is available. Full `check` deliberately runs the integration oracles too.

## Optional developer feedback

```sh
mise run rust-nextest          # Prebuilt nextest 0.9.143; four test threads, no retries
mise run rust-doc-test         # nextest does not run doctests
mise run rust-watch            # watchexec 2.7.1, queued cargo check on source changes
mise run rust-bacon-install    # Optional project-local Bacon build, separate Rust 1.98.1
mise run rust-bacon            # Bacon 3.25.0 check UI; `rust-bacon clippy` selects Clippy
```

These task-specific tools are installed only when requested. The watcher never
runs Fern applications or changes dependencies. Nextest's build MSRV differs
from the compiler it can test. The prebuilt runner follows the project nightly;
its original Decision106 verification used Rust 1.75.0. Required CI remains
`cargo test`, including doctests. Current migration evidence belongs in
[the roadmap](../ROADMAP.md). [Nextest version policy](https://github.com/nextest-rs/nextest/blob/cargo-nextest-0.9.143/README.md)

Bacon provides an interactive job/output UI in addition to the simple watcher.
Its opt-in installer pins Rust **1.98.1** and runs `cargo install --locked` for
Bacon **3.25.0** into `compiler-rs/target/dev-tools/bacon-3.25.0`. It does not
install a global executable or change Fern dependencies. This requires a second
Rust compiler and a source build; ordinary setup and CI do not install it.
The UI runs separately under the project environment, and both configured Bacon
jobs explicitly select **nightly-2026-09-06**. The installer retains its
independently pinned [build requirement](https://github.com/Canop/bacon/blob/v3.25.0/Cargo.toml);
its compiler does not select the toolchain used to check Fern.
[Bacon jobs](https://dystroy.org/bacon/config/)

Cargo-generate and cargo-seek do not serve
an existing project workflow. The [Rust guidance review](RUST_GUIDANCE.md) records the lint policy and the
separate Criterion developer package. Run `mise run rust-lint-policy` and
`mise run rust-bench-smoke` for its required CI checks, and `mise run rust-bench`
for optional statistical measurements. The default compiler has no application
dependencies; `mise run rust-cranelift-check` enables and checks the optional,
locked native backend dependencies described in [the backend guide](BACKEND_REASSESSMENT.md).

## Reproducibility boundaries

`mise.lock` records version-specific URLs/checksums for Python, uv and optional
binary tools on Linux/macOS x64/arm64. Config-scoped strict locking rejects a
missing supported-platform URL. Rust is date-pinned but uses rustup's own
distribution verification: mise's URL-lock enforcement does not apply to that
backend. This is not an offline environment or a pinned OS image. Native package
versions and platform SDKs remain host inputs. The three Python reference scripts
have checked-in uv script locks; maintained tasks and script shebangs use
`--locked`, so dependency metadata drift fails before execution. [Lockfile scope](https://mise.jdx.dev/dev-tools/mise-lock.html)

Update a pin deliberately, regenerate and review the lockfile, then run affected
gates. For Rust, update mise, the root toolchain file, Bacon jobs and their
contract tests together; verify required nightly components on supported hosts.
Run the full Rust/native/C/docs gates and developer-tool checks before recording
the new date as verified. Do not use a floating `nightly` or remove checksums to
bypass verification:

```sh
mise lock -p linux-x64,linux-arm64,macos-x64,macos-arm64
mise run tool-versions
mise run mise-workflow-check
```

Personal `mise.local.toml` and `mise.local.lock` overrides are ignored by Git.
Historical decisions and published measurement records retain their original
command spellings; active instructions and CI use mise.

Update a Python reference dependency deliberately with
`mise exec -- uv lock --script scripts/<name>.py`, review the `.py.lock` diff,
and rerun `mise run mise-workflow-check` plus the affected parity gates.
The workflow check verifies current locks and proves stale metadata cannot
execute the script or rewrite its lock. See [uv script locking](https://docs.astral.sh/uv/guides/scripts/#locking-dependencies).
