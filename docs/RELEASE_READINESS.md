# Fern release readiness

Fern is pre-1.0. The following describes executable behavior, not every feature in
[the language design](../DESIGN.md). Historical Gate A–D completion records
engineering milestones; they do not certify the entire language.

## Available and regression covered

| Surface | Current behavior | Verification |
| --- | --- | --- |
| First program | Check, format, compile, run; relocatable compiler/runtime pair; local install | Installation integration tests and tutorial output assertions |
| Native strings | Quotes, backslashes, control bytes, Unicode, long literals; typed user-function print results | String and print codegen execution regressions |
| Files | Bounded complete UTF-8 read/write/append with Result errors, plus delete/size | Native fault/limit tests on macOS/Linux and interactive text tests |
| HTTP | GET/POST clients, response bodies on 2xx, integer errors otherwise | Local HTTP/TLS runtime tests; offline error example |
| SQLite | Open a handle and execute statements | Runtime database regression tests |
| Actor foundation | String FIFO mailboxes, lifecycle/monitor/restart, three deterministic strategies | Six invariant scenarios and 1,536 seeded strategy crash steps |
| Terminal UI | Styled output, panels/tables, editable input/password prompts, cursor controls, immutable trees, logs | 13 native/PTY tests and a compiled example |
| Editor | Rust LSP, bounded Tree-sitter corpus and locally staged Zed extension | Native/WASM source parity, reproducible package tests and isolated actual-Zed LSP startup |
| Native checker | Style diagnostic parity and 66 build/test/example/Git/CLI workflows under both frontends | Required diagnostic and workflow gates on macOS/Linux arm64 |

## Blocking full language completion

- **Concurrency execution:** spawn does not run a function, and the complete typed
  receive/suspension/timeout/request-reply model is absent. Native compilation
  rejects the unsupported execution syntax; use `actors.start/post/next` for the
  available explicit mailbox operations. Descendant termination is verified; ancestor escalation and subtree reconstruction remain incomplete.
  See [the exact actor contract](ACTOR_RUNTIME.md).
- **JSON:** Rust native execution and its REPL use the bounded, validating opaque
  JSON model with exact numbers and immutable builders. The legacy C source API
  still copies strings and can accept invalid JSON. Explicitly derived record codecs
  now include regular recursive schemas with finite bases and transparent
  newtypes. Generic constraints, sum/union wire formats, general traits and the verified Rust default
  switch remain open; see [typed codecs](JSON_TYPED_CODECS.md) and
  [the Rust JSON contract](JSON_RUST_API.md).
- **Server and database APIs:** HTTP serving, typed SQL queries and the broader
  design-level application stack are not implemented by the current client and
  SQLite execute primitives.
- **Bootstrapping:** native build/test/Git workflows and diagnostic parity are
  covered, including Unicode negative-path CLI classification. A reliable native
  default launcher remains open. Python remains the quality-workflow entry point;
  see [the checker contract](BOOTSTRAP_CHECKER.md).
- **Result handling:** unused bindings and discarded Result expressions are
  rejected, but merely reading a collection's length or handling a value on only
  one branch can currently satisfy the binding-use check. Early-exit searches can
  leave later errors unhandled. Semantic handling on reachable paths remains open.
- **Editor completeness:** the verified grammar corpus is bounded. Source-label completion supports closed and EOF-open calls; remaining syntax
  and broader malformed-source recovery remain open. Local Zed packaging does not publish its pinned grammar revision.
- **Memory and targets:** Boehm GC remains the native memory backend. Ownership
  primitives are a baseline, not complete Perceus analysis. WASM is planned.
- **Language coverage:** every supported design construct still needs a complete
  parse/check/native-output audit. The existing examples cover a useful subset,
  and successful type checking alone does not certify executable semantics.

## Release verification

Before tagging a release, run these from a clean checkout with documented native
dependencies installed:

```sh
just check
just style-parity
just docs-check
just fuzz-smoke
just lsp-rpc-smoke
just release-policy-check
just perf-budget
just release-package
just release-package-check
```

`just check` includes native user workflows, installation, PTY, string/print, and
actor regression coverage. `just perf-budget` measures a release build; its
budgets are enforced in the script, not inferred from aspirational README sizes.
`just release-package` builds the release bundle, and its packaging script verifies
the archive checksum and required members.

The supported CI matrix is Linux and macOS. Local validation on one host does
not substitute for both CI jobs. Compilation requires a host C compiler, GC,
SQLite, and OpenSSL development libraries; compiled programs may retain platform
shared-library dependencies. A release must not advertise universal static
portability without checking its actual linked dependencies.

A 1.0 proposal must close the blocking items above, document compatibility and
migration behavior, and demonstrate real application execution under the
[compatibility policy](COMPATIBILITY_POLICY.md). See [ROADMAP.md](../ROADMAP.md)
for the current task list and [the language guide](LANGUAGE_GUIDE.md) to get started.
