# Fern Language Extension for Zed

This directory contains Fern editor integration work. Decision84 verifies the
bounded Tree-sitter grammar and queries through native and WASM execution;
Zed grammar registration and extension packaging remain unverified. The historical
installation instructions below are not an end-to-end installation guarantee.
See [grammar scope and reproducible tooling](../tree-sitter-fern/README.md).

## Features

- Syntax highlighting for `.fn` files
- Language Server Protocol (LSP) support via `fern lsp`
  - Real-time diagnostics (error reporting)
  - Hover information (type information)
  - Go-to-definition (function navigation)

## Requirements

The `fern` compiler must be installed and available in your PATH. The extension runs `fern lsp` to start the language server.

### Installing Fern

```bash
# Clone and build
git clone https://github.com/niklas-heer/fern
cd fern
just release
just install  # Installs to /usr/local/bin
```

## Installation

### Quick Install (Recommended)

From the Fern project root:

```bash
./editor/install-zed-extension.sh
```

Then reload extensions in Zed:
- Open Command Palette (`Cmd+Shift+P`)
- Run: "zed: reload extensions"

This creates a symlink for development - changes to the extension are reflected immediately after reloading.

### Manual Installation

1. Copy or symlink the extension directory:
   ```bash
   ln -s /path/to/fern/editor/zed-fern \
     "$HOME/Library/Application Support/Zed/extensions/installed/fern"
   ```

2. Reload extensions in Zed (Command Palette → "zed: reload extensions")

### Building the Extension

The extension is pre-built (`extension.wasm`), but to rebuild:

```bash
cd editor/zed-fern
cargo build --release --target wasm32-wasi
cp target/wasm32-wasi/release/zed_fern.wasm extension.wasm
```

## Configuration

No additional configuration is required. The extension will automatically:
- Recognize `.fn` files as Fern
- Start the language server when you open a Fern file

## Troubleshooting

If the language server isn't working:

1. Verify `fern` is in your PATH:
   ```bash
   which fern
   fern --version
   ```

2. Test the LSP manually:
   ```bash
   fern lsp
   # Should wait for JSON-RPC input on stdin
   ```

3. Check Zed's language server logs:
   - Open Command Palette (Cmd+Shift+P)
   - Run "debug: open language server logs"

4. Check the LSP log file:
   ```bash
   cat /tmp/fern-lsp.log
   ```

## License

MIT License - see [LICENSE](LICENSE)
