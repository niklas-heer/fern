# Fern task runner (Just)

set shell := ["bash", "-euo", "pipefail", "-c"]

prefix := env_var_or_default("PREFIX", "/usr/local")
destdir := env_var_or_default("DESTDIR", "")

cc := "clang"
base_cflags := "-std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude -Ilib -Ideps/qbe -Ideps/linenoise"
debug_flags := "-g -O0 -DDEBUG"
release_flags := "-O2 -DNDEBUG"
qbe_cflags := "-std=c99 -Ideps/qbe -Wno-unused-parameter -Wno-sign-compare"
linenoise_cflags := "-std=c11 -Ideps/linenoise -Wno-unused-parameter -Wno-missing-field-initializers"
civetweb_cflags := "-std=c11 -Ideps/civetweb/include -Ideps/civetweb/src -DNO_CGI -DNO_SSL_DL -DOPENSSL_API_1_1"

runtime_sources := "runtime/*.c"
src_sources := "src/*.c"
lib_sources := "lib/*.c"
test_sources := "tests/*.c"
qbe_sources := "deps/qbe/main.c deps/qbe/util.c deps/qbe/parse.c deps/qbe/abi.c deps/qbe/cfg.c deps/qbe/mem.c deps/qbe/ssa.c deps/qbe/alias.c deps/qbe/load.c deps/qbe/copy.c deps/qbe/fold.c deps/qbe/simpl.c deps/qbe/live.c deps/qbe/spill.c deps/qbe/rega.c deps/qbe/emit.c deps/qbe/amd64/targ.c deps/qbe/amd64/sysv.c deps/qbe/amd64/isel.c deps/qbe/amd64/emit.c deps/qbe/arm64/targ.c deps/qbe/arm64/abi.c deps/qbe/arm64/isel.c deps/qbe/arm64/emit.c deps/qbe/rv64/targ.c deps/qbe/rv64/abi.c deps/qbe/rv64/isel.c deps/qbe/rv64/emit.c"

# Default target
default:
    @just --list

[private]
_build-fern mode:
    #!/usr/bin/env bash
    set -euo pipefail

    mode="{{mode}}"
    case "$mode" in
      debug) mode_flags=( {{debug_flags}} ) ;;
      release) mode_flags=( {{release_flags}} ) ;;
      *) echo "Unknown build mode: $mode"; exit 1 ;;
    esac

    cc="{{cc}}"
    base=( {{base_cflags}} )
    qbe=( {{qbe_cflags}} )
    linenoise=( {{linenoise_cflags}} )

    mkdir -p build bin

    src_objs=()
    for src in {{src_sources}}; do
      obj="build/$(basename "${src%.c}").o"
      "$cc" "${base[@]}" "${mode_flags[@]}" -c "$src" -o "$obj"
      src_objs+=("$obj")
    done

    lib_objs=()
    for src in {{lib_sources}}; do
      obj="build/lib_$(basename "${src%.c}").o"
      "$cc" "${base[@]}" "${mode_flags[@]}" -c "$src" -o "$obj"
      lib_objs+=("$obj")
    done

    qbe_objs=()
    for src in {{qbe_sources}}; do
      rel="${src#deps/qbe/}"
      rel_obj="${rel%.c}"
      rel_obj="${rel_obj//\//_}"
      obj="build/qbe_${rel_obj}.o"
      "$cc" "${qbe[@]}" -c "$src" -o "$obj"
      qbe_objs+=("$obj")
    done

    "$cc" "${linenoise[@]}" -c deps/linenoise/linenoise.c -o build/linenoise.o

    "$cc" "${base[@]}" "${mode_flags[@]}" -o bin/fern \
      "${src_objs[@]}" "${lib_objs[@]}" "${qbe_objs[@]}" build/linenoise.o -lm

    echo "✓ Built fern compiler: bin/fern"

[private]
_build-runtime mode:
    #!/usr/bin/env bash
    set -euo pipefail

    mode="{{mode}}"
    case "$mode" in
      debug) mode_flags=( {{debug_flags}} ) ;;
      release) mode_flags=( {{release_flags}} ) ;;
      *) echo "Unknown build mode: $mode"; exit 1 ;;
    esac

    cc="{{cc}}"
    base=( {{base_cflags}} )
    civetweb=( {{civetweb_cflags}} )

    gc_cflags=()
    if pkg-config --exists bdw-gc >/dev/null 2>&1; then
      read -r -a gc_cflags <<< "$(pkg-config --cflags bdw-gc)"
    fi

    openssl_cflags=()
    if pkg-config --exists openssl >/dev/null 2>&1; then
      read -r -a openssl_cflags <<< "$(pkg-config --cflags openssl)"
    fi

    mkdir -p build bin

    runtime_objs=()
    for src in {{runtime_sources}}; do
      obj="build/runtime_$(basename "${src%.c}").o"
      "$cc" "${base[@]}" "${mode_flags[@]}" "${gc_cflags[@]}" -Iruntime -Ideps/civetweb/include -c "$src" -o "$obj"
      runtime_objs+=("$obj")
    done

    "$cc" "${civetweb[@]}" "${mode_flags[@]}" "${openssl_cflags[@]}" -c deps/civetweb/src/civetweb.c -o build/runtime_civetweb.o
    runtime_objs+=("build/runtime_civetweb.o")

    "$cc" {{linenoise_cflags}} "${mode_flags[@]}" -c deps/linenoise/linenoise.c -o build/runtime_linenoise.o
    runtime_objs+=("build/runtime_linenoise.o")

    ar rcs bin/libfern_runtime.a "${runtime_objs[@]}"
    echo "✓ Built runtime library: bin/libfern_runtime.a"

[private]
_build-test-runner mode:
    #!/usr/bin/env bash
    set -euo pipefail

    mode="{{mode}}"
    case "$mode" in
      debug) mode_flags=( {{debug_flags}} ) ;;
      release) mode_flags=( {{release_flags}} ) ;;
      *) echo "Unknown build mode: $mode"; exit 1 ;;
    esac

    cc="{{cc}}"
    base=( {{base_cflags}} )

    mkdir -p build bin

    test_objs=()
    for src in {{test_sources}}; do
      obj="build/test_$(basename "${src%.c}").o"
      "$cc" "${base[@]}" "${mode_flags[@]}" -c "$src" -o "$obj"
      test_objs+=("$obj")
    done

    "$cc" "${base[@]}" "${mode_flags[@]}" -c tests/fuzz/fuzz_generator.c -o build/fuzz_generator_test.o

    lib_objs=()
    for src in {{lib_sources}}; do
      lib_objs+=("build/lib_$(basename "${src%.c}").o")
    done

    "$cc" "${base[@]}" "${mode_flags[@]}" -o bin/test_runner \
      "${test_objs[@]}" "${lib_objs[@]}" build/linenoise.o build/fuzz_generator_test.o -lm

    echo "✓ Built test runner: bin/test_runner"

[private]
_build-fuzz-runner mode:
    #!/usr/bin/env bash
    set -euo pipefail

    mode="{{mode}}"
    case "$mode" in
      debug) mode_flags=( {{debug_flags}} ) ;;
      release) mode_flags=( {{release_flags}} ) ;;
      *) echo "Unknown build mode: $mode"; exit 1 ;;
    esac

    cc="{{cc}}"
    base=( {{base_cflags}} )

    mkdir -p build bin

    "$cc" "${base[@]}" "${mode_flags[@]}" \
      -o bin/fuzz_runner tests/fuzz/fuzz_runner.c tests/fuzz/fuzz_generator.c

    echo "✓ Built fuzz runner: bin/fuzz_runner"

# Debug build
debug:
    @just _build-fern debug
    @just _build-runtime debug

# Release build
release: clean
    @just _build-fern release
    @just _build-runtime release

# Build runtime library only
runtime-lib:
    @just _build-runtime debug

# Build and run tests
test: debug
    @just _build-test-runner debug
    @echo "Running tests..."
    @./bin/test_runner
    python3 tests/integration/test_installation.py
    python3 scripts/test_user_workflows.py
    python3 scripts/test_tui.py
    python3 scripts/test_runtime_process.py
    python3 scripts/test_string_codegen.py
    python3 tests/integration/test_print_types.py
    python3 scripts/test_release_workflow.py

# Run real user programs and tutorial output checks
test-user-workflows: debug
    python3 scripts/test_user_workflows.py

# Build fuzz runner
fuzz-bin: debug
    @just _build-fuzz-runner debug

# Clean build artifacts
clean:
    rm -rf build bin build_qbe_*.o
    @echo "✓ Cleaned build artifacts"

# Install fern compiler
install: release
    install -d {{quote(destdir + prefix + "/bin")}}
    install -m 755 bin/fern {{quote(destdir + prefix + "/bin/fern")}}
    install -m 644 bin/libfern_runtime.a {{quote(destdir + prefix + "/bin/libfern_runtime.a")}}
    @echo {{quote("✓ Installed fern and runtime to " + destdir + prefix + "/bin")}}

# Uninstall
uninstall:
    rm -f {{quote(destdir + prefix + "/bin/fern")}} {{quote(destdir + prefix + "/bin/libfern_runtime.a")}}
    @echo "✓ Uninstalled fern"

# Run with Valgrind for memory checking
memcheck: debug
    valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./bin/fern

# Format code
fmt:
    clang-format -i src/*.c include/*.h tests/*.c

# Full quality check (build + test + style, strict mode)
check:
    uv run scripts/check_style.py src lib

# Style check only (no build/test)
style:
    uv run scripts/check_style.py --style-only src lib

# Lenient style check (warnings allowed)
style-lenient:
    uv run scripts/check_style.py --style-only --lenient src lib

# Run the Fern implementation of the style checker (bootstrapping progress)
style-fern: debug
    ./bin/fern run scripts/check_style.fn

# Compare exact diagnostic records, severities, and exits on fixtures and repository
style-parity: debug
    uv run scripts/test_style_parity.py

# Pre-commit hook check
pre-commit:
    uv run scripts/check_style.py --pre-commit src lib

# Test examples - type check all .fn files in examples/
test-examples: debug
    #!/usr/bin/env bash
    set -euo pipefail

    echo "Testing examples..."
    failed=0

    for f in examples/*.fn; do
      name="$(basename "$f")"
      printf "  %-30s" "$name"
      if ./bin/fern check "$f" > /dev/null 2>&1; then
        echo "✓ PASS"
      else
        echo "✗ FAIL"
        ./bin/fern check "$f" 2>&1 | head -5
        failed=1
      fi
    done

    if [[ "$failed" -eq 1 ]]; then
      echo
      echo "Some examples failed type checking!"
      exit 1
    fi

    echo
    echo "✓ All examples type check!"

# Run doc tests extracted from @doc fenced `fern` snippets
test-doc: debug
    python3 scripts/run_doc_tests.py ${DOC_TEST_FLAGS:-}

# Generate unified docs site (Fern modules + C API)
docs:
    python3 scripts/generate_unified_docs.py ${DOC_FLAGS:-}

# Lightweight docs consistency checks (local links + status markers)
docs-consistency:
    python3 scripts/check_docs_consistency.py ${DOCS_CONSISTENCY_FLAGS:-}

# Validate docs generation + public API documentation + doc examples
docs-check: debug
    just docs-consistency
    python3 scripts/generate_unified_docs.py --check ${DOC_FLAGS:-}
    python3 scripts/run_doc_tests.py ${DOC_TEST_FLAGS:-}

# Run grammar/property fuzzing for parser + formatter stability
fuzz: fuzz-bin
    #!/usr/bin/env bash
    set -euo pipefail

    iters="${ITERS:-512}"
    seed="${SEED:-$(date +%s)}"

    ./bin/fuzz_runner --iterations "$iters" --seed "$seed" --fern-bin ./bin/fern

# Run a short fuzzing pass suitable for CI
fuzz-smoke: fuzz-bin
    #!/usr/bin/env bash
    set -euo pipefail

    iters="${ITERS:-64}"
    seed="${SEED:-0xC0FFEE}"

    ./bin/fuzz_runner --iterations "$iters" --seed "$seed" --fern-bin ./bin/fern

# Continuously run fuzzing with rolling seeds
fuzz-forever: fuzz-bin
    #!/usr/bin/env bash
    set -euo pipefail

    iters="${ITERS:-256}"

    while true; do
      seed="$(date +%s)"
      echo "== FernFuzz run (seed=$seed, iterations=$iters) =="
      ./bin/fuzz_runner --iterations "$iters" --seed "$seed" --fern-bin ./bin/fern
    done

# Check CI performance budgets (compile time, startup latency, binary size)
perf-budget:
    python3 scripts/check_perf_budget.py ${PERF_BUDGET_FLAGS:-}

# Validate compatibility/deprecation policy document
release-policy-check:
    python3 scripts/check_release_policy.py ${RELEASE_POLICY_DOC:-docs/COMPATIBILITY_POLICY.md}

# Publish reproducible benchmark + case-study report
benchmark-report:
    python3 scripts/publish_benchmarks.py ${BENCHMARK_FLAGS:-}

# Package release bundle (fern + libfern_runtime.a + docs/license) into dist/
release-package: release
    #!/usr/bin/env bash
    set -euo pipefail

    version="$(awk -F'"' '/^#define FERN_VERSION_STRING / {print $2; exit}' include/version.h)"

    rm -rf dist/staging
    mkdir -p dist/staging/docs

    cp bin/fern dist/staging/fern
    cp bin/libfern_runtime.a dist/staging/libfern_runtime.a
    cp LICENSE dist/staging/LICENSE
    cp README.md dist/staging/README.md
    cp docs/COMPATIBILITY_POLICY.md dist/staging/docs/COMPATIBILITY_POLICY.md

    python3 scripts/package_release.py package \
      --version "$version" \
      --staging dist/staging \
      --out-dir dist

# Validate staging layout for release packaging
release-package-check:
    #!/usr/bin/env bash
    set -euo pipefail

    rm -rf dist/staging
    mkdir -p dist/staging/docs

    cp bin/fern dist/staging/fern
    cp bin/libfern_runtime.a dist/staging/libfern_runtime.a
    cp LICENSE dist/staging/LICENSE
    cp README.md dist/staging/README.md
    cp docs/COMPATIBILITY_POLICY.md dist/staging/docs/COMPATIBILITY_POLICY.md

    python3 scripts/package_release.py verify-layout --staging dist/staging

# End-to-end LSP JSON-RPC smoke validation
lsp-rpc-smoke: debug
    python3 scripts/lsp_rpc_smoke.py

# Generate editor support files (syntax highlighting, grammar, etc.)
editor-support:
    @echo "Generating editor support files..."
    @python3 scripts/generate_editor_support.py
    @echo "✓ Editor support files regenerated"
    @echo
    @echo "To compile Tree-sitter grammar to WASM:"
    @echo "  just editor-support-compile"

# Compile Tree-sitter grammar to WASM
editor-support-compile: editor-support
    #!/usr/bin/env bash
    set -euo pipefail

    echo "Compiling Tree-sitter grammar to WASM..."

    if ! command -v tree-sitter >/dev/null 2>&1; then
      echo "Error: tree-sitter CLI not found"
      echo "Install with: npm install -g tree-sitter-cli"
      exit 1
    fi

    (cd editor/tree-sitter-fern && tree-sitter generate)
    (cd editor/tree-sitter-fern && tree-sitter build --wasm)

    cp editor/tree-sitter-fern/tree-sitter-fern.wasm editor/zed-fern/languages/fern/fern.wasm

    echo "✓ Tree-sitter grammar compiled and installed to Zed extension"

# Help
help:
    @just --list

# Rust frontend experiment (the default fern compiler remains C)
[private]
_build-rust-backend:
    {{cc}} {{base_cflags}} compiler-rs/backend/qbe_driver.c build/qbe_*.o -lm -o bin/fern-qbe

# Build the independent Rust frontend and reusable QBE process adapter
rust-build: debug
    just _build-rust-backend
    cargo build --locked --manifest-path compiler-rs/Cargo.toml
    cp compiler-rs/target/debug/fern-rs bin/fern-rs

# Rust safety/style/unit gates and native specification/differential checks
rust-check: rust-build
    cargo fmt --manifest-path compiler-rs/Cargo.toml -- --check
    cargo clippy --locked --manifest-path compiler-rs/Cargo.toml --all-targets -- -D warnings
    cargo test --locked --manifest-path compiler-rs/Cargo.toml
    python3 scripts/test_evaluate_rust_frontend.py
    python3 scripts/test_rust_frontend.py
    python3 scripts/test_rust_collections.py

# Build both frontends with release settings for the opt-in evaluation.
rust-release: release
    just _build-rust-backend
    cargo build --locked --release --manifest-path compiler-rs/Cargo.toml
    cp compiler-rs/target/release/fern-rs bin/fern-rs

# Record local measurements only after native output verification succeeds.
rust-evaluate: rust-release
    python3 scripts/evaluate_rust_frontend.py --output build/rust-frontend-evaluation.json
