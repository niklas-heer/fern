# Fern for Zed

The extension registers `.fn` files, the Fern grammar, and the Rust language
server. The staged package has been tested with Zed **1.18.0** on macOS arm64.
It does not download a compiler. See the [grammar scope](../tree-sitter-fern/README.md)
for the verified syntax and the three known malformed-source recovery gaps.

## Language server

Build the Rust frontend with `cargo build --release --manifest-path compiler-rs/Cargo.toml`
and put `compiler-rs/target/release/fern-rs` in your PATH. The extension discovers
`fern-rs` in the worktree environment and launches `fern-rs lsp`. It deliberately
does not fall back to the legacy C `fern` frontend.

Alternatively, put this in your Zed settings, using your executable's real path:

```json
{
  "lsp": {
    "fern-lsp": {
      "binary": {
        "path": "/absolute/path/to/fern-rs",
        "arguments": ["lsp"]
      }
    }
  }
}
```

Include `arguments` when overriding `path`: Zed's binary override bypasses the
extension's default command arguments. Paths and arguments are passed literally;
no shell is invoked. Trust the project when prompted before starting its server.
[Official worktree trust documentation](https://zed.dev/docs/worktree-trust).

## Build a local package

The compiler retains Rust 1.75 compatibility. This extension has a separate
pinned Rust **1.97.1** toolchain and `wasm32-wasip2` target, with the published
`zed_extension_api` **0.7.0** dependency locked in Cargo.lock. Current Zed requires
Preview2 components; the old `wasm32-wasi` build instructions do not apply.
[Official extension development documentation](https://zed.dev/docs/extensions/developing-extensions).

Provision those tools explicitly, then run `cargo fetch --locked` in this
extension directory once. Package builds use `--offline --locked`. For isolation, set `RUSTUP_HOME` and
`CARGO_HOME` to dedicated temporary directories before installing the extension
toolchain; do not change the compiler toolchain. The package gate also requires
Python 3.11+, Node, Tree-sitter 0.26.12, WASI SDK 29.0, the pinned web runtime,
and wasm-tools 1.258.0. Version and download hashes are recorded in
[scripts/editor/toolchain.json](../../scripts/editor/toolchain.json) and
[zed-toolchain.json](../../scripts/editor/zed-toolchain.json).

From the repository root, choose a **new** output directory:

```sh
python3 scripts/package_zed.py \
  --output /tmp/fern-zed-package \
  --grammar-repository /absolute/path/to/fern \
  --cargo /absolute/path/to/cargo \
  --wasi-sdk /absolute/path/to/wasi-sdk-29 \
  --tree-sitter /absolute/path/to/tree-sitter \
  --web-runtime /absolute/path/to/web-tree-sitter.cjs \
  --wasm-tools /absolute/path/to/wasm-tools
```

`editor/install-zed-extension.sh` forwards to this staging command. It no longer
deletes or symlinks directories in your Zed profile. The command refuses existing
outputs and leaves them unchanged on failure. It produces `extension/`,
`archive.tar.gz`, and `build.json`; the latter records exact input/output hashes.
No prebuilt `extension.wasm` is claimed to be checked into this directory.

The package contains `extension.wasm`, **`grammars/fern.wasm`**, the language
config and four queries. It excludes Cargo caches and the historical
`languages/fern/fern.wasm` test artifact. The grammar is rebuilt from the manifest's
full commit, then compared byte-for-byte with that commit's pinned portable WASM.
It is validated and executed with the actual staged queries before publication.

This is our local staging pipeline, **not** the official `zed-extension` packager;
`build.json` records `official_packager: false`. Zed's source builder uses a
separate clang build profile. Its SDK25 output was accepted by native Zed 1.18.0,
but its `libc.so` dependency is not loadable by the pinned web runtime. The staged
portable grammar uses the project's verified Tree-sitter/SDK29 profile instead.
[Zed's official builder source](https://github.com/zed-industries/zed/blob/v1.18.1/crates/extension/src/extension_builder.rs).

## Verify and try the package

```sh
python3 scripts/test_zed_package.py
# scripts/test_zed_support.py runs the complete pinned build/parity gate; see --help.
python3 scripts/test_zed_smoke.py \
  --zed /Applications/Zed.app/Contents/MacOS/zed \
  --package /tmp/fern-zed-package \
  --rust /absolute/path/to/fern-rs
```

The opt-in smoke opens Zed with separate temporary profiles, exercises both PATH
discovery and explicit binary settings, and verifies real initialize/didOpen and
clean diagnostic messages. It terminates only its own test processes and retains
the temporary logs. It does not install into your normal editor profile.
[Zed CLI profile isolation](https://zed.dev/docs/reference/cli).

For a normal development installation, use Zed's **Install Dev Extension** action
and select this source directory after provisioning its build tools. That action
builds from the repository/revision/path in extension.toml. The pinned revision
must be available in the configured repository: an unpublished local commit
cannot be fetched remotely. Local package verification does not publish it.
[Grammar registration and local repositories](https://zed.dev/docs/extensions/languages).

If startup fails, verify `fern-rs` exists, check `binary.arguments`, and open
Zed's language-server logs. The extension never substitutes another compiler or
downloads an executable on your behalf.

MIT License — see [LICENSE](LICENSE).
