# Fern editor grammar

The editable sources are `scripts/editor/grammar.js.in`, its highlight/outline
query templates, and `src/scanner.c`. The Rust compiler is the syntax authority.
Never edit generated `grammar.js`, parser sources/headers, queries or WASM by hand.
`just editor-support` renders the templates unconditionally; `--check` on the
Python generator detects drift without writing.

Decision84 verifies aliases, newtypes and ordinary function clauses, including
qualified/generic/function types, constructor/list/tuple patterns, guards, colon
and arrow bodies, and the prerequisite expressions and indentation. The finite
corpus contains 24 accepted sources, eight recovery cases and eight incremental
edits, plus the original simple-function golden tree. Each accepted source is
checked by the Rust compiler. Native and WASM parsers must preserve declaration
identities, source byte ranges and the following declaration after malformed input.
All four Zed queries compile and execute; outline/highlight captures are checked.

This is a bounded editor checkpoint. Full actor/with/for/map/record-update syntax,
complete text/interpolation/comment behavior and remaining Rust language forms
still need corpus-backed implementation. Zed grammar registration and extension
packaging are separate open work; a valid parser module does not prove installation.

## Pinned tools

Use Tree-sitter CLI **0.26.12**, generated grammar ABI **14**, WASI SDK **29.0**
(LLVM21.1.4), and the matching web-tree-sitter **0.26.12** runtime. Node and clang
are test hosts. Exact official asset URLs and SHA256 digests for the verified
macOS arm64 profile are in `scripts/editor/toolchain.json`. Download into an
explicit tool directory, verify every archive before extraction, and keep tools
and caches outside the repository. This workflow does not modify Fern's Rust1.75
or any global tool installation. Other host profiles need separately verified
release asset hashes before claiming reproducible support.

For each locked asset, use its exact URL and digest (for example, a caller-chosen
`/tmp/fern-editor-tools` directory):

```sh
curl --fail --location --output "$TOOLS/tree-sitter-macos-arm64.gz" \
  https://github.com/tree-sitter/tree-sitter/releases/download/v0.26.12/tree-sitter-macos-arm64.gz
# Require this exact SHA256 before decompressing:
shasum -a 256 "$TOOLS/tree-sitter-macos-arm64.gz"
# a7ddeff2507391ebdfd0b8fa0dfe91babed291ea00d86426cb0e6cfade891697
gzip -dc "$TOOLS/tree-sitter-macos-arm64.gz" > "$TOOLS/tree-sitter"
chmod +x "$TOOLS/tree-sitter"
```

Verify and extract the locked `web-tree-sitter.tar.gz` and
`wasi-sdk-29.0-arm64-macos.tar.gz` similarly. Set the SDK path to the extracted
root containing `VERSION`, `bin` and `share`; set the web runtime to the extracted
`web-tree-sitter.cjs` beside its matching `web-tree-sitter.wasm`.

## Generate and verify

From the repository root, with an already built Rust compiler:

```sh
export TREE_SITTER_CLI="$TOOLS/tree-sitter"
export TREE_SITTER_WASI_SDK_PATH="$TOOLS/wasi-sdk"
export TREE_SITTER_WEB_RUNTIME="$TOOLS/web/web-tree-sitter.cjs"
export XDG_CACHE_HOME="$TOOLS/cache"
export FERN_RUST="$PWD/compiler-rs/target/debug/fern-rs"
env -u LIBRARY_PATH just editor-support-compile
env -u LIBRARY_PATH just editor-support-check
```

The compile recipe renders templates then runs `scripts/test_editor_support.py
--update`. That script generates all parser sources/headers in a temporary copy,
runs scanner release/ASan/UBSan tests, native golden and query tests, builds WASM
twice with the canonical `tree-sitter-fern.wasm` basename, publishes both identical
modules, loads each exact module with the web runtime, and executes native/WASM
parity and incremental tests. The check recipe performs the same verification and
requires exact equality with published artifacts. Every subprocess has a deadline;
missing tools or the Rust oracle fail explicitly. No Fern C compiler/runtime build
is needed. The gate also detects stale parser headers, not just `grammar.js`.

The scanner uses Tree-sitter allocator lifecycle helpers, as explicitly allowed
by Decision84/CLAUDE. It validates malformed incremental state without assertions,
retains at most 128 indentation levels, caps columns at 1 MiB, and serializes all
levels in at most 514 bytes. This is not a Fern runtime allocation exception.
