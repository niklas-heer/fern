# Fern backend reassessment — 6 September 2026

Recommendation: pursue a current, supported Cranelift AOT backend as a measured second backend, then decide whether to make it the default after Fern's full native acceptance corpus passes. Keep QBE as the working reference during that migration. Rust is a good implementation language for either choice; it does not require a Rust code generator. Cranelift is probably the alternative the user remembers.

This is an assessment plus an isolated scalar AOT experiment. The production backend, dependencies, MSRV and toolchain defaults are unchanged. Actor105 remains a separate milestone. The original local prototype files and raw measurements are under `/tmp/fern-backend-research`; those temporary paths are evidence from this run, not installed project components.

## What changed in the original rationale

Local Decision1 says QBE emits C and therefore reaches any platform with a C compiler. That is incorrect: QBE emits assembly for supported machine targets; an assembler and linker produce the executable. Its small C implementation and compact SSA interface remain real advantages. Decision23's embedded backend rationale and Decision45's Rust frontend/process boundary are independent choices. A Rust frontend can keep invoking QBE, embed it through a separately audited boundary, or emit Cranelift IR. [QBE overview](https://c9x.me/compile/), [QBE IL documentation](https://c9x.me/compile/doc/il.html).

Upstream QBE is actively maintained: release1.3 is dated 1 June2026 and adds Windows ABI support. Its archive SHA256 is `d587905d620dc5e1d2bfa7c2cc642b9b837aa89a3188c6e37b53d756cf66e320`, upstream commit `c0818978acec60ebb6167fade60fb7012cbf20ca`. Fern's January2026 vendor import should not be described as current1.3 without reconciliation. Updating QBE is a separate candidate worth benchmarking; this experiment used Fern's existing vendor plus the isolated Decision104 Apple fix. [QBE releases](https://c9x.me/compile/releases.html).

Cranelift is a general-purpose native code generator with AOT and JIT APIs, production use in Wasmtime, and x86-64, AArch64, s390x and RISC-V backends. Its advertised compilation-speed comparisons concern LLVM, not QBE or Fern. No evidence here establishes that Cranelift intrinsically compiles Fern faster or generates faster Fern programs. [Cranelift project](https://cranelift.dev/).

## Toolchain and maintenance decision

The verified current Cranelift release is0.135.1, in Wasmtime48.0.1. Its workspace requires Rust1.95.0 and edition2024; crates inherit that requirement. Current stable Rust is1.98.1, released 3 September2026. Thus modern Cranelift requires a deliberate upgrade from Fern's1.75 baseline, but no nightly compiler. The modern prototype below used an already-installed Rust1.97.1, which satisfies the requirement; it did not install or change the default toolchain. [Release manifest](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v48.0.1/Cargo.toml), [Codegen manifest](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v48.0.1/cranelift/codegen/Cargo.toml), [Rust release notes](https://doc.rust-lang.org/releases.html).

Historical Cranelift0.108.2/Wasmtime21.0.2 has a1.75 workspace minimum;0.109.1/Wasmtime22.0.1 moves to1.76. I compiled0.108.2 on1.75 only after pinning an old transitive indexmap version: unconstrained contemporary dependency resolution selected an edition2024 dependency that old Cargo cannot parse. This is concrete evidence that an old version number alone is not a maintained toolchain policy. Do not adopt that unsupported2024 release merely to preserve1.75. [Wasmtime21 manifest](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v21.0.2/Cargo.toml), [Wasmtime22 manifest](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v22.0.1/Cargo.toml).

Choose either an explicit compiler MSRV/dependency-policy upgrade for supported Cranelift, or a separately packaged modern-Rust backend helper consuming a versioned, bounded backend-neutral representation while the frontend remains1.75. The latter retains process isolation but adds protocol and deployment maintenance. Wasmtime publishes monthly releases and a defined support/LTS policy; adopt an upgrade owner and supported release window, rather than assuming Cranelift's API is permanently stable. [Support policy](https://docs.wasmtime.dev/stability-release.html).

## Concrete integration issues

Fern's approximately6315 lines of Rust QBE emitter/helpers are not just instruction printing. They implement full-width heap payloads, nominal/newtype/union layouts, closures, manual self-tail-call elimination, dynamic defers, fault propagation, checked indexing, JSON codec descriptors, public typed-IR validation and source-owned test exit behavior. Most of that must remain backend-independent and must not be reimplemented inconsistently in two emitters.

Extract a bounded lower-level control-flow/ABI representation after shared typed-IR validation and semantic lowering. Have QBE and Cranelift consume it. Do not parse emitted QBE text into Cranelift as the enduring architecture. The shared representation should explicitly describe scalar widths, C and Fern call signatures, hidden environment/fault arguments, block terminators, immutable data, relocations, scratch slots, source locations and GC-visible pointer lifetimes. Keep all existing fault/defer/tail semantics and independent expected-output tests.

Cranelift's object backend writes ELF, COFF and Mach-O directly, removing Fern's separate assembly subprocess. Linking the existing C runtime and its libraries remains necessary. A compiler built with Cranelift does not automatically link Rust std or Cranelift into every Fern application. Cross compilation still needs the correct runtime libraries, SDK and linker. The object backend explicitly rejects Wasm output: Wasmtime compiling Wasm into native code is a different direction from Fern targeting browser Wasm. A future browser backend still needs its own Wasm/runtime design. [Object backend implementation](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v48.0.1/cranelift/object/src/backend.rs).

Current Fern Float printing/interpolation helpers emit variadic `printf`/`snprintf` calls. Cranelift has an open general variadic-support issue. Before porting these paths, use small fixed-signature C runtime wrappers or demonstrate correct target-specific lowering on every supported platform. Do not assume a ordinary fixed signature reproduces Apple variadic conventions. [Upstream varargs issue](https://github.com/bytecodealliance/wasmtime/issues/1030).

Cranelift has an explicit AppleAarch64 calling convention and its AArch64 allocatable register environment excludes reserved x18, with x16/x17 scratch registers. That is encouraging evidence, not a substitute for Fern's new no-x18 and scalar-clobber regression. The Decision104 defect in Fern's QBE vendor is concrete; it remains unproven that it caused the original sampled checker crash. [Calling conventions](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v48.0.1/cranelift/codegen/src/isa/call_conv.rs), [AArch64 implementation](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v48.0.1/cranelift/codegen/src/isa/aarch64/abi.rs), [Apple ABI](https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms).

Debugging requires frontend work with either backend. Cranelift source locations are opaque frontend-defined IDs; Fern must provide file/line mapping and write suitable debug data/variable locations. In the current object backend, enabling its convenience `unwind_info(true)` path for Mach-O explicitly panics because that path does not implement Mach-O eh_frame. This is a specific missing object-emission path, not a claim that all Cranelift debugging on Apple is impossible. QBE already exposes experimental dbgfile/dbgloc operations, which the current Rust emitter does not use. Useful Fern line breakpoints could therefore be improved before a backend migration. [Source locations](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v48.0.1/cranelift/codegen/src/ir/sourceloc.rs), [Object unwind implementation](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v48.0.1/cranelift/object/src/backend.rs), [QBE releases](https://c9x.me/compile/releases.html).

A Cranelift JIT is a later project. It does not remove the REPL's effect, retained-value, GC, failure and safe-Rust boundaries. Likewise, using rustc_codegen_cranelift to compile the Fern compiler itself is a separate question from using Cranelift to compile Fern programs.

## Executed AOT experiment

Host: Apple M2 Pro, native macOS ARM64. Both backends compiled the same scalar `bench(n:i64, seed:i64)->i64` iterative xorshift algorithm; variants generated either1 or200 exported functions. An independent C reference checked inputs0..99 and a ten-million-step result, including a high-bit seed. Every native executable returned success and the final decimal result `2783892466536392950`.

Cranelift emitted Mach-O objects directly. QBE emitted assembly, then the same host C driver assembled it. Backend measurements used two warmups and ten measured processes; native runtime used six executions with the clock inside the binary, excluding startup. These are medians in milliseconds:

| Backend | Functions | Backend process | Through object creation | Native10m steps |
|---|---:|---:|---:|---:|
| Fern QBE +104 |1|2.904|41.052|18.172|
| Cranelift0.135.1 none |1|2.583|2.583|18.141|
| Cranelift0.135.1 speed |1|2.661|2.661|18.166|
| Fern QBE +104 |200|9.857|69.867|18.011|
| Cranelift0.135.1 none |200|9.711|9.711|18.601|
| Cranelift0.135.1 speed |200|12.042|12.042|18.169|

The measurable advantage here is principally avoiding the separate Apple assembler/driver stage. For200 functions, backend computation is comparable, and speed-mode Cranelift is slower to compile than QBE in this sample. Native execution is similar. The common link stage still took approximately62–66ms in individual, non-statistical observations. This is not an end-to-end Fern benchmark: QBE's input SSA was pre-serialized, Cranelift's process included IR construction, the host was not reserved, run order was not randomized, and the program has no GC, calls, Float, runtime IO or debug metadata. It supplies feasibility and a concrete pipeline hypothesis, not a production performance promise.

The1-function executables were33528 bytes with QBE and33536 with Cranelift. A Rust compiler backend dependency does not imply large generated programs. Backend utility binaries were roughly218KiB for QBE versus2.6MiB for the modern Cranelift prototype, but stripping/features/build flags differ, so this is only a rough compiler-distribution observation. Historical0.108.2 results are retained in raw JSON, not used to recommend an unsupported release.

Local experimental artifacts (not installed project components):

- `/tmp/fern-backend-research/prototype`: Rust1.75 + pinned Cranelift0.108.2, isolated Cargo home; initial MSRV failure and corrected build logs retained.
- `/tmp/fern-backend-research/current-prototype`: Cranelift0.135.1, Rust1.97.1; no shared dependency changes.
- `/tmp/fern-backend-research/measure.py`, `driver.c`, `measurements.json`, `measurements.log`.
- Measurement-script SHA256: `53f03ab31be52ed4df359dac2ee664c3fab4a4557ea8f1eef747e4c51423cac0`.
- Results SHA256: `37f6d03a71c503235df517cefaab4100d4a41ec064a1e57b82550b99d071ae99`.
- Modern lockfile SHA256: `705c8a49e661e576cc44becdc1d19203ad91421457f5bc91d14057d448691731`.
- Historical lockfile SHA256: `f502e0dcfca028293efb50d9556017a472d543d61184675828099367108dea09`.

## Acceptance and benchmark plan before changing the default

1. Approve the supported Cranelift/toolchain policy separately. Add an explicit experimental backend selection; preserve a single semantic frontend and existing QBE path. Establish typed fixed-signature runtime calls before any broad port.
2. Run the full native Fern oracle corpus independently through both backends on AppleARM64 and LinuxARM64, then Linuxx86-64. Include full-width Int extrema, Float negative zero/NaN, Bool, more than eight arguments, mixed C ABI arguments, closures/environment/fault pointers, tuples, JSON values/codecs, nominal and unboxed newtypes, unions and relocation/PIC cases. Retain expected outputs; differential agreement alone is insufficient.
3. Pin all cleanup/failure contracts: first fault, argument evaluation order, defer LIFO including failing cleanup, HOF callbacks, Try/Return, Result main, source test exit interception, checked collection bounds, and million-iteration constant-stack recursion. Stress Boehm-visible pointers in captures/records/collections across allocations and C calls. Every malformed public IR/resource-bound regression must remain rejected before backend execution.
4. Require real debugger acceptance: source breakpoint, correct line after branch/closure, stack through Fern and C calls, and documented variable visibility on LLDB/GDB. If full debug support is deferred, state its exact level instead of describing the switch as solving debugging.
5. Measure release compiler end-to-end check/build/run time, lowering time, code generation, object creation, linking, peak RSS, compiler build time and install size separately. Use tiny CLI, actual repository checker, JSON/collection work, closures/tail recursion and a large multi-module application. Use at least30 alternating randomized runs per configuration on otherwise idle hosts, report median/p95, state cold/warm cache definitions, and preserve raw inputs, tool versions and logs. Compare current QBE, separately updated QBE if warranted, and supported Cranelift none/speed with identical runtime/link options.
6. Make the default decision from user-visible build/debug benefits, maintenance cost and native regression evidence. Retain QBE until the complete semantic gates pass and no material unexplained performance or deployment regression remains. AOT Cranelift is a credible next architecture, but the tiny prototype does not justify deleting a working backend immediately.

The measured raw result table is retained in [the report data](reports/backend-reassessment-2026-09-06.json). Re-run the full acceptance plan above before making a backend or performance default decision.
