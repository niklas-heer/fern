# Fern Language - Decision Log

This document tracks major architectural and technical decisions made during the development of the Fern programming language and compiler.

## Project Decision Log

### 81 Run source-owned unit tests alongside documentation examples
* **Date**: 2026-09-06
* **Status**: Accepted for Rust migration completion
* **Decision**: I will discover zero-argument `test_` functions from parsed source and execute each checked Unit or Result(Unit,E) function independently. Normal `fern test` includes both unit tests and documentation examples; `--doc` restricts execution to documentation.
* **Context**: The design specifies ordinary named test functions, but the current command only executes documentation examples. Selecting by original source identity avoids rerunning imports or replacing user main references. Boolean/integer return values must not silently pass as unasserted tests.
* **Consequences**: Real private helpers and original main remain callable. Native tests use the existing bounded capture and cleanup mechanism, continue after failures (including invalid test signatures), and report original names and locations. Discovery retains parameterized groups so eligibility errors are reported independently for each test. Combined discovery is limited to 256 tests; unsupported result signatures reject before native compilation. Unit and documentation execution use a dedicated QBE test mode: invoking the resolved process-exit API always diagnoses and exits unsuccessfully, including through helpers and callbacks, so exit0 cannot bypass remaining assertions. Ordinary application emission and unused exit functions remain unchanged; exit-behavior tests must use a child process. Eligibility uses one ordinary checker pass and the selected reusable source signature, with no editor metadata budget. Assertion libraries, benchmarks, coverage and watch remain tracked separately. The unavailable `/decision` skill is replaced by the established decision format.

### 79 Preserve distinct newtype identities without wrapper allocation
* **Date**: 2026-09-06
* **Status**: Accepted for Rust migration completion
* **Decision**: I will represent newtypes as semantic nominal identities with validated unboxed payload layouts. Explicit construction and projection lower to the same native operand, while parameter, result, container and closure boundaries use the payload's full-width ABI.
* **Context**: DESIGN promises distinct UserId/ProductId identities with zero runtime cost. A tagged one-field record would introduce allocation and change Float/native ABI behavior. Concrete layout keys distinguish valid nested wrappers from impossible unboxed cycles, while existing heap indirection supports guarded recursion.
* **Consequences**: Same-identity scalar equality and List.contains inherit Int/Float/Bool/String behavior; Map keys inherit Int/Bool/String behavior with String content comparison. Arithmetic, ordering, Print/interpolation and implicit conversion remain unavailable without explicit projection or future traits. Wrapped Result values retain handling obligations. Checked Wrap/Unwrap and newtype patterns are validated at public IR boundaries; depth/type/program-work limits apply before expansion. REPL values reuse underlying storage, while formatter/docs/editor preserve source identities. The unavailable `/decision` skill is replaced by the established decision format.

### 80 Publish checked inferred documentation without executing examples
* **Date**: 2026-09-06
* **Status**: Accepted for Rust migration completion
* **Decision**: I will add explicit `doc --inferred` generation using one complete library check and bounded reusable source schemes per module graph. Original headers and documentation remain, supplemented by resolved signatures and intrinsic requirements.
* **Context**: Source-only documentation cannot explain omitted private types. Rechecking the program per declaration scales poorly and risks inconsistent generic identities; backend specializations erase source patterns and do not describe reusable functions. A checked library pipeline now validates all bodies without inventing main.
* **Consequences**: Default documentation remains parser-only. Checked mode resolves current imports, preserves exact source anchors, rejects invalid graphs and enforces aggregate graph/metadata/output budgets. Source contents are borrowed from bounded project caches, with actual loaded and cached copies charged once. Every documented source and loaded dependency is protected against output replacement, including hardlinks. Generated names never appear as user types, and directory search/escaping remain shared. The unavailable `/decision` skill is replaced by the established decision format.

### 78 Complete one incomplete member using independent current-source evidence
* **Date**: 2026-09-06
* **Status**: Accepted for Rust migration completion
* **Decision**: I will recover a single member selector for completion only when its receiver is already concrete and the enclosing function group has an independently fixed concrete signature. Recovery is an explicit partial proof with non-executable IR, never repaired source or a guessed member.
* **Context**: Ordinary checked source facts cannot cover the moment a user types `value.`. Feeding missing-operation constraints into whole-signature inference could fabricate receiver types, while candidate replacement can hide independent errors. The parser can preserve source offsets using one private token and an opaque site.
* **Consequences**: The ordinary graph, signatures, unaffected bodies and local constraints still validate. Only local hole-dependent unknowns may remain private editor markers; they never justify receiver evidence or enter schemes/QBE/REPL. Current overlays and exact UTF-16 edits are preserved, all existing limits apply, and unrelated errors retain lexical fallback. Editor library checking uses the real source graph without inserting main. See [the recovery contract](docs/EDITOR_RECOVERY.md). The unavailable `/decision` skill is replaced by the established decision format.

### 77 Evaluate immutable JSON in the REPL with explicit aggregate limits
* **Date**: 2026-09-06
* **Status**: Accepted for Rust migration completion
* **Decision**: I will implement the dynamic JSON API in the safe, std-only Rust evaluator using immutable shared nodes and the same native format/error/resource profile. No native FFI, subprocess evaluation or lossy serialization intermediary is used.
* **Context**: Native opaque JSON values now have a tested contract. Interactive programs must retain exact number text, Unicode including escaped NUL, insertion order, ordinary Result errors and immutable shared children across session entries. A per-call parser cap alone cannot bound repeated large operations in one entry.
* **Consequences**: Decimal-to-Float conversion uses Rust 1.75's nearest/ties-even parser after strict JSON validation, and Float construction reproduces the native 17-significant-digit spelling. Logical native allocation/work charges preserve per-operation limits; normal interactive evaluation additionally has separate 64 MiB aggregate allocation and work ceilings, with independent 8 MiB cleanup reserves. Charges occur before work/allocation, including failed attempts; aggregate allocation charges include larger semantic Rust node/collection representations while native logical per-operation counters stay unchanged. Retained storage uses an iterative unique-node walk under the existing 16 MiB/200,000 session ceilings; cached expanded sizes bound serialization but never replace physical sharing accounting. JSON domain errors remain ordinary Results; aggregate faults retain existing first-failure, cleanup and atomic binding behavior. C source migration and typed codecs remain separate. The unavailable `/decision` skill is replaced by the established decision format.


### 76 Execute documentation examples as checked native tests
* **Date**: 2026-09-06
* **Status**: Accepted for Rust migration completion
* **Decision**: I will implement explicit `fern-rs test --doc` execution of fenced Fern examples from parser-owned documentation. Trailing `# =>` expectations are checked Fern patterns, including Result/Option wildcards, and failures produce a nonzero test result.
* **Context**: The existing Python documentation check compiles snippets but strips their expected results and never executes them. DESIGN requires runnable examples, multiline setup and constructor-pattern expectations. Reusing the parser, checker and native backend preserves ordinary Fern semantics and gives examples access to private declarations in their owning module.
* **Consequences**: Each example receives isolated local bindings and a checked synthetic test function, while module imports resolve through an in-memory overlay. Library checking validates all bodies without inventing main; ordinary executable checking still requires an entry. Original entry points are preserved by function ID, and validated IR entry selection runs only the test harness. Expectations attach only to complete top-level expression statements, with lexical comment ranges distinguishing markers from string text. Discovery, example count/source bytes, runtime duration and captured output are bounded; current source is never overwritten. General unit-test syntax, coverage and watch mode remain separate CLI milestones. Tests explicitly execute user code; documentation generation itself never executes examples. The unavailable `/decision` skill is replaced by the established decision format.


### 75 Expose typed Rust JSON values through explicit native adapters
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will migrate the Rust frontend to opaque json.Value/json.Error types, immutable builders and lossless ordered collection access using explicit full-width native ABI adapters. The Json compatibility spelling resolves to the same identities.
* **Context**: The native JSON parser is verified, but the existing source API still copies Strings. Native JSON member records and Fern tagged tuples have different layouts, and Float arguments require a floating-point ABI rather than an integer bit argument. The C frontend has separate qualified-type and container limitations that must not be hidden by changing registry declarations alone.
* **Consequences**: Rust parse returns Result(Value, Error) and stringify accepts Value. Explicit conversions preserve exact numbers, Unicode and NUL errors. Members return JSON String keys as Values, and object builders convert a once-evaluated Map into checked parallel native lists. Adapter allocations and expanded shared subtrees are bounded before publication, including depth/node/output growth. Opaque values cannot be fabricated, inspected as records or implicitly compared/printed. The C source contract and old native symbols remain explicitly legacy until their own migration; Rust REPL evaluation stays explicitly unavailable for these operations until the following parity checkpoint. Ten native output cases and twelve semantic rejections define this vertical migration. The unavailable `/decision` skill is replaced by the established decision format.


### 72 Preserve resolved global references as explicit AST identities
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will distinguish resolved global names, calls and pipe targets from lexical names in the module-resolved AST. Resolution checks the original source root against lexical bindings before producing explicit global forms.
* **Context**: Rewriting an import alias to a canonical module string can accidentally capture a different local with that canonical spelling. This affects compilation, dependency ordering and checked editor facts: `import model as m` followed by `let model = 3` must not turn `m.value()` into a field access on that local. Giving all dotted names global priority would instead break actual lexical shadowing.
* **Consequences**: Function values, direct calls, pipes, captures and generic dependency discovery retain their resolved identity. Source parsing and formatting preserve written syntax; module resolution owns the transition to explicit global forms. Every AST visitor handles these forms explicitly, and regression tests cover aliases, canonical-name collisions and real source-root shadowing. No magic string prefixes or span-only identity side tables are used. The unavailable `/decision` skill is replaced by the established decision format.


### 74 Expand transparent aliases before nominal checking with shared budgets
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will implement scalar and generic transparent type aliases as source declarations, expanding their references before nominal registry construction without adding an executable representation. Alias expansion charges the same bounded work budget used by dependency analysis and inference.
* **Context**: DESIGN distinguishes transparent aliases from distinct zero-cost newtypes and set-theoretic unions. Reusing tagged one-field records for all three would change their promised identity or runtime cost. Aliases can provide useful source vocabulary while retaining existing type equality and runtime layout.
* **Consequences**: Original alias declarations and module identities remain available to formatting, documentation and editor navigation. Expansion substitutes generics without capture, rejects arity/name/cycle errors, and checks depth/node/output budgets before allocating expanded trees. Nominal recursive records remain valid; transparent cyclic aliases do not. Aliases add no constructors and do not create a privacy boundary, while existing constructor visibility stays enforced. Newtype representation and union coercion/narrowing remain separate checkpoints. The unavailable `/decision` skill is replaced by the established decision format.

### 68 Publish checked source facts for editor hover
* **Date**: 2026-09-05
* **Status**: Accepted for the authorized T2a Rust tooling milestone
* **Decision**: I will expose bounded source-facing type facts from finalized function validation, retaining original declaration and binding origins. Editor hover and valid-source member details will use these facts only after the complete ordinary checker succeeds on the current module overlay graph.
* **Context**: Source navigation already tracks scopes, aliases and exact UTF-16 locations. Final backend IR contains specialized copies and generated dispatch/closure names, while inference probes contain provisional variables; neither can safely define user-facing generic hover identities. Shared source presentation now validates types and patterns and explicitly renames inferred quantified identities without conflating them with declared variables.
* **Consequences**: Generic declaration schemes and instantiated occurrence types remain distinct; intrinsic requirements are reported in checker-owned language. Clauses and captured/shadowed locals preserve source anchors. Metadata allocation and output are bounded and optional, and ordinary compilation retains its existing API and behavior. Invalid current source yields no stale type facts. T2a covers hover and typed details on valid source; the separate T2b recovery milestone will handle incomplete receiver/member syntax. The unavailable `/decision` skill is replaced by the established decision format.

### 73 Generate bounded directory documentation as one searchable artifact
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will extend parser-based documentation to source directories with deterministic file ordering, module navigation and local text search in a single standalone HTML artifact. Markdown remains available and single-file behavior stays compatible.
* **Context**: Individual source documentation is implemented, but a library's users need to move between modules and find declarations. A single artifact avoids partial multi-file publication and filename collisions, and local filtering needs no network service or external dependencies.
* **Consequences**: Recursive discovery has explicit depth, entry, source-byte and file-count limits; symbolic links are not followed. Every source parses before output is installed, and output cannot replace any input inode. Source paths and documentation remain escaped data; the fixed search script only reads text and toggles visibility. Default directories exclude hidden entries and build/dependency directories, with the exclusions documented. Executable doc tests and inferred documentation signatures remain separately gated features. The unavailable `/decision` skill is replaced by the established decision format.

### 70 Replace the JSON string-copy baseline with opaque JSON values
* **Date**: 2026-09-05
* **Status**: Accepted for staged Rust migration completion
* **Decision**: I will implement immutable opaque json.Value and json.Error types with a validating parser, explicit conversions and bounded serialization. New native symbols preserve the old String-copy ABI until each frontend's source API is migrated and verified.
* **Context**: Existing json.parse and json.stringify copy strings without validating JSON. A dynamic value model must preserve exact number text and valid JSON strings containing escaped NUL, even though Fern String cannot currently represent NUL. Typed derive/decode codecs need this foundation first.
* **Consequences**: The public migration will use Result(json.Value, json.Error) and Result(String, json.Error); String-as-Value calls become type errors. JSON numbers retain their lexemes, objects preserve insertion order and reject duplicate decoded keys, invalid Unicode/unpaired surrogates fail, and one leading UTF-8 BOM is accepted. Parsing is bounded to 1 MiB input, depth 128, 100,000 values and 32 MiB logical allocation; encoding is bounded to 16 MiB with charged traversal. JSON-to-Fern String conversion rejects embedded NUL without truncation. Native runtime, frontend/ABI and REPL/builders land as separate verified checkpoints; lowercase json remains canonical and existing Json compatibility spelling is preserved when migrated. The format profile follows RFC 8259 with explicit stricter duplicate/Unicode choices. The unavailable `/decision` skill is replaced by the established decision format.

### 71 Represent delayed inference shapes with non-executable probe nodes
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will retain delayed field, update, tuple-rest and iteration constraints in a bounded obligation table, using an explicitly tagged probe-only IR node to continue gathering later body evidence. Construction is crate-private and requires a private token; the node is never executable Fern IR.
* **Context**: Retrying a body after its first unresolved projection cannot discover an annotation or call later in that same body. Returning a fake Unit, local or field-index value would obscure this gap and could corrupt later compiler passes. A second complete type checker would duplicate the existing typing rules.
* **Consequences**: Probe nodes retain evaluated child expressions and their result type slot; delayed obligations resolve only from independent type evidence, without guessing nominal types or tuple arity. Union/assignment revisions determine progress, and retries share the whole-signature work budget. Probes are discarded before generalized source is rechecked. Finalization, public IR validation, code generation, interactive execution and editor fact publication must reject any surviving probe. All affected visitors are updated explicitly and rejection/resource/branch-order tests guard the boundary. The unavailable `/decision` skill is replaced by the established decision format.

### 67 Generalize private signatures by recursive component
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will infer omitted private parameter and return types from patterns and function bodies in callee-first recursive components, then publish closed schemes with intrinsic capability requirements. Calls to completed schemes instantiate fresh variables; unfinished inferred recursive members share monotypes.
* **Context**: Pattern-only inference requires annotations for ordinary identity and higher-order helpers. Using whichever caller is visited first would make types order-dependent. Explicit generic annotations remain universal and must not be weakened to make inference succeed; existing complete-parameter return inference already handles annotated mutual recursion with distinct generic names.
* **Consequences**: Public boundaries remain annotated and omitted main remains Unit. Explicit type variables stay rigid, local bindings stay monomorphic, and inferred polymorphic recursion requires a complete annotation. Generalization preserves parameter/result relationships and rejects unanchored recursive results or ambiguous requirements. Source-provided Infer remains forbidden. Dependency traversal, constraints, recursive solving and scheme closure share bounded work. Core generalization lands before explicit delayed shape obligations; the milestone stays open until later body evidence can resolve fields, updates, iteration and tuple-rest shapes without guessed types or fake values. The unavailable `/decision` skill is replaced by the established decision format.

### 69 Generate source documentation with the Rust parser
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will generate Rust frontend documentation from parsed source declarations and literal @doc metadata, grouping function clauses and retaining their original source signatures. Documentation generation does not require an executable main or run examples implicitly.
* **Context**: The current Python generator recognizes signatures with a regular expression, which cannot cover nested function types, clause patterns or Unicode identifiers reliably. The Rust parser already establishes declaration boundaries and documentation ownership.
* **Consequences**: The first checkpoint accepts one source file, writes Markdown by default or standalone escaped HTML, and supports atomic output files without overwriting source aliases. Parsing and output are bounded. All declarations are included and public visibility is shown; inferred signatures are not invented from omitted annotations. Directory navigation/search and explicit executable doc tests follow as separate checkpoints. Documentation text is literal data in HTML; no scripts or remote assets are required. The unavailable `/decision` skill is replaced by the established decision format.

### 66 Validate generic bodies with rigid equality and capability requirements
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will validate every generic body before specialization using rigid declared type variables and explicit internal requirements for overloaded operations. Type equality will no longer accept an arbitrary concrete type merely because one side is generic.
* **Context**: The current template probe can accept an unused `fn bad(x: a) -> a: 1`, while concrete specialization rejects some later uses. Existing generic arithmetic and scalar interpolation are useful and must retain their actual numeric/display restrictions rather than be checked with an arbitrary Int instance.
* **Consequences**: Capability requirements preserve the concrete domains of arithmetic, addition, ordering, equality, printing, collection membership and map keys. Calls and function values instantiate and propagate those requirements with their types, under bounded work. Concrete impossible requirements and incompatible universal returns are errors even when unused. Conditional Result discard obligations remain distinct from type equality. Concrete specialization continues to validate the backend boundary. Public where/trait syntax and whole private-signature generalization build on this internal scheme representation separately. The unavailable `/decision` skill is replaced by the established decision format.


### 65 Index editor symbols by source identity and lexical scope
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will build bounded editor symbol snapshots from the current source graph and lexical bindings, with definitions identified by their original file and source anchor. Navigation and completion will use the same module visibility and qualification facts as compilation.
* **Context**: Final IR identifiers are local to functions and may be duplicated by generic specialization or closure lifting. Text matching cannot distinguish shadowed bindings, separate clause parameters or imported private names. Existing unsaved overlays and UTF-16 synchronization already define coherent editor inputs.
* **Consequences**: Accepted edits invalidate semantic snapshots. Unresolved or invalid current source produces no stale semantic locations. Completion is deterministic, bounded and respects scopes, aliases and public exports; builtin prefix completion may remain available on incomplete source without inventing types. Exact source ranges distinguish code from comments and literal text. Semantic hover and typed members require checker facts and will be advertised only when implemented. The unavailable `/decision` skill is replaced by the established decision format.

### 64 Infer private parameter types from complete pattern evidence
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will collect constraints from every clause pattern and supplied parameter annotation before normalizing a private function group. This stage fills omitted annotations only when their types are fully determined, including explicitly anchored generic variables.
* **Context**: Literal and constructor patterns often establish a function's input type without any caller. Using the first caller as evidence would make otherwise generic functions depend on call order. Full private signature generalization needs a separate recursive-component solver.
* **Consequences**: All clauses constrain one slot per parameter position. Public parameter annotations remain mandatory. Empty lists, generic nullary constructors, catchalls and tuple-rest shapes alone may remain ambiguous and require annotations until whole-signature inference lands. Constructor schemas use fresh variables, explicit generic names remain rigid, conflicts report source diagnostics, and pattern/type work has one bounded budget across the pass. Existing coverage and Result checks still run after normalization. The unavailable `/decision` skill is replaced by the established decision format.


### 62 Eliminate eligible self-tail calls without changing cleanup semantics
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will lower direct self calls in return position to parameter updates and a function-local backedge when that function has no owned defer registration. Compiler scratch stack slots will be declared in the entry block, with initialization retained at each logical use.
* **Context**: Fern uses recursion instead of while/loop. Ordinary native calls grow the stack, and QBE alloc8 outside the entry block can allocate dynamically on repeated paths. Deferred cleanup must still execute once per actual function activation.
* **Consequences**: Argument expressions evaluate left-to-right into temporary values before any parameter slot changes. Faults and early exits skip later arguments. Full-width values and the existing environment/fault context are preserved. Functions owning defer, mutual recursion and indirect calls keep ordinary calls; nested lifted closure bodies do not disable an otherwise eligible parent. This is direct self-tail-call elimination, not a general proper-tail-call guarantee. Hoisted scratch storage prevents loop and with temporaries from growing the stack on each backedge. The unavailable `/decision` skill is replaced by the established decision format.

### 63 Normalize adjacent function clauses through the shared pattern engine
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will retain adjacent clauses in source syntax and normalize each group to one checked function before return inference. Typed pattern parameters, guards and arrow bodies use the existing exhaustive match semantics.
* **Context**: DESIGN specifies function clauses and pattern parameters, while the Rust frontend already has shared pattern coverage, function-owned control flow and cleanup. A separate dispatch implementation would risk different coverage and Result handling rules.
* **Consequences**: Clauses must agree on arity, parameter types, visibility and supplied return annotations; initially generic names must remain consistent across a group. Guards do not guarantee coverage, and missing cases are errors rather than DESIGN's earlier warning. Whole-function documentation appears before the first clause. Synthetic argument names cannot collide with source identifiers; balanced dispatch tuples preserve the 255-parameter limit. Whole-pattern Result discard checks precede hidden argument reads, and each arm retains its own binder obligations. This checkpoint requires annotated parameter patterns; pattern-anchored inference and complete private signature generalization follow separately. The unavailable `/decision` skill is replaced by the established decision format.

### 61 Preserve indentation for block expressions inside delimiters
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will preserve bounded indentation frames for multiline expression suites inside calls, lists and tuples, including inline separating commas and closing delimiters.
* **Context**: Suppressing all layout within parentheses prevented valid composition such as `println(match value: ...)`. Block callbacks already needed a limited version of the same mechanism. Users should not need a temporary variable merely to pass an expression to a function.
* **Consequences**: Match, if, for, with and callback suites restore significant layout at their owning delimiter depth. Ordinary nested delimiters still suspend layout. Frames close only their owned indentation before separators/closers; malformed or excessive nesting reports a source diagnostic. Comment and multiline-string contents do not become layout instructions. Formatting must retain equivalent checked IR and remain idempotent. The unavailable `/decision` skill is replaced by the established decision format.

### 60 Share bounded sequence patterns across language constructs
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will support exact list patterns and list/tuple suffix patterns ending in `..name` or `.._` through the common checked pattern engine. Potentially failing destructuring requires match or let-else; ordinary let, for and with success bindings retain their existing irrefutability requirement.
* **Context**: DESIGN specifies list and tuple rest patterns, including function clauses, but its plain-list destructuring examples do not explain length mismatch. Silently reading beyond a list or introducing an unchecked failure would violate the existing binding contract.
* **Consequences**: List lengths and nested tags are checked before projections. Named tails are materialized only after the whole structural pattern succeeds and before any guard that uses them; ignored tails allocate nothing. List tails initially copy a bounded suffix and preserve immutable aliases. Tuple tails retain tuple identity, including singleton tuples, while an empty suffix is Unit. Match coverage models empty/nonempty lists and remains bounded under sequence expansion. Rest must appear last and can only bind or discard; Result-bearing values cannot be silently discarded by prefix or suffix patterns. Refutable plain-list examples require an else branch or match until a stronger static length proof exists. The unavailable `/decision` skill is replaced by the established decision format.

### 59 Infer private return schemes before concrete specialization
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will infer omitted private function returns with shared, bounded type constraints before the existing concrete specialization pass. Public return signatures remain explicit and omitted main remains Unit.
* **Context**: DESIGN permits internal inference while requiring annotated APIs. Checking definitions independently loses forward and recursive return constraints; specializing a generic probe as Int would reject valid Float uses or silently change a scheme.
* **Consequences**: Annotated parameters remain the boundary for this checkpoint. Return evidence from tails, early returns and propagation can establish concrete types or declared generic schemes. Only unresolved shape dependencies are retried; genuine errors remain errors. Unanchored cycles require an annotation. A shared work and inference-storage budget bounds retries across all definitions. Public provenance survives module flattening. This does not claim full parameter inference, function clauses, or complete unused generic-body checking. The unavailable `/decision` skill is replaced by the established decision format.

### 58 Preserve UTF-8 strings at slicing and splitting boundaries
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will retain byte-indexed String.slice and its existing index clamping, while requiring clamped endpoints to be Unicode scalar boundaries. Splitting on an empty delimiter produces one complete Unicode scalar per String.
* **Context**: Native byte-by-byte splitting can produce invalid UTF-8, while interactive strings already reject invalid slices. DESIGN defines byte lengths without specifying these non-ASCII corner cases. A String must remain valid UTF-8 across these operations.
* **Consequences**: Clamping first sets start to at least zero and end to at least start, then bounds both by byte length. Interior-byte endpoints are errors even when the requested slice is empty. Rust guards execute deferred cleanup before reporting `String.slice indices must be UTF-8 character boundaries`; the shared legacy C function rejects the same request independently. Empty input split on an empty delimiter yields an empty list; combining marks remain separate scalars, with no implicit grapheme segmentation or normalization. Nonempty delimiter behavior is preserved. The unavailable `/decision` skill is replaced by the established decision format.

### 57 Define entry errors and guard legacy runtime preconditions
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will accept `main -> Result((), E)` for every concrete error type, exit zero for Ok and one for Err after deferred cleanup, and report `fern: main returned Err` for an unhandled entry error. Runtime faults take precedence. Existing direct-valued list access keeps its source signature and reports invalid access through the explicit Rust fault context.
* **Context**: DESIGN specifies Result entry points but does not define a universal Error type or an error-display protocol. The existing List.get/head signatures have incompatible direct-value and recoverable descriptions. Their native assertions are not a safe execution contract. String repetition can overflow its allocation size from a tiny input.
* **Consequences**: Rust-generated List.get/head failures run cleanup and never load out-of-bounds storage. Shared C helpers independently report the same failures before access in debug and release builds; their legacy callers do not receive the Rust cleanup protocol. General error rendering and recoverable indexing APIs remain separate work. String.repeat permits at most 16,777,216 content bytes, checks before multiplication/allocation, and returns empty immediately for empty input or nonpositive counts. Rust checks before calling C so cleanup executes; the legacy C ABI independently rejects oversized requests with the same diagnostic and exit 1, without Rust's cleanup protocol. The REPL applies this language limit before its stricter interactive storage limit. No arbitrary error payload is printed as an address, and no failure is replaced with an empty string. The unavailable `/decision` skill is replaced by the established decision format.

### 55 Define numeric domains and unwind runtime faults through cleanup
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will use wrapping Int arithmetic consistently, bounded exponentiation by squaring for nonnegative Int powers, IEEE Float power, and value-based Float list membership. Invalid integer division/remainder by zero or negative integer exponents produce controlled runtime diagnostics after deferred cleanup.
* **Context**: The existing REPL diagnoses zero division while native signed division can trap or vary by architecture. DESIGN's debug-overflow panic rule conflicts with its no-panic aspirations and the current wrapping interactive implementation. Fern's scalar operators retain scalar result types; invalid numeric domains need a defined execution failure rather than an arbitrary value or a hardware-dependent crash.
* **Consequences**: Generated functions receive an explicit fault context after their environment argument; closures receive the current caller's context and never capture a stack context. Fault checks dominate uses of function/callback results. Faulting paths run ordinary function cleanup, and the first fault wins even if cleanup also fails. Each cleanup callback runs with a cleared context, so remaining callbacks still execute. Main reports one diagnostic and exits 1. This introduces no mutable process-global state and changes neither source Function/Result types nor the C runtime ABI. Int::MIN divided by -1 wraps to MIN, with remainder 0; 0**0 is 1. Power is right-associative and retains existing unary precedence. Elixir-style bitwise operators &&&/|||/^^^/~~~/<<</>>> keep record-update and pipe delimiters distinct; shifts normalize counts modulo 64 and right shifts preserve the sign. General recoverable checked-arithmetic APIs remain a separate library requirement. The unavailable `/decision` skill is replaced by the established decision format.

### 56 Preserve literal contents and document Unicode identifier spelling
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will parse base-prefixed integers with explicit digit/separator/range validation, preserve the exact contents of triple-quoted strings, and accept bounded nested block comments. Documentation attributes retain their declaration association. Non-ASCII non-whitespace identifier characters retain the C frontend's broad spelling compatibility, with exact UTF-8 identity and no normalization.
* **Context**: Fern specifies multiline strings, block comments, documentation attributes and full-width numeric values, while legacy lexical acceptance includes incomplete or malformed cases. Reusing ordinary strings' escapes/interpolation and retaining newline/indent bytes avoids implicit transformations. Bitwise token choices must coexist with record updates and pipes.
* **Consequences**: Unterminated comments/strings and invalid digits, separators or integer magnitudes are diagnostics. Case-insensitive 0x/0b/0o prefixes select bases; a leading minus permits the exact Int minimum. String contents do not undergo automatic dedenting. ASCII identifiers begin with a letter or underscore and continue with letters/digits/underscores; non-ASCII spelling is preserved exactly. Formatting must preserve parsed semantics, literal values, comments and documentation metadata. The unavailable `/decision` skill is replaced by the established decision format.

### 54 Keep iteration lazy and error handlers concretely typed
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will represent ranges as immutable Int endpoints with an inclusive flag, iterate List/Map/Range values once in their defined order, and give each function its own stack of loop targets. With blocks will retain sequential checked steps and handlers specialized for each distinct error type.
* **Context**: Materializing a full-width integer range would allocate unbounded memory or overflow at an inclusive maximum endpoint. Fern specifies heterogeneous errors in with blocks; forcing them into one inferred error type would reject the documented control flow. Repeatedly nesting source-level matches would also turn a flat block into deep compiler recursion.
* **Consequences**: Empty or reversed ranges produce no iterations. Inclusive ranges test their final value before incrementing. Break and continue target the nearest loop in the same function and leave deferred cleanup registered until function exit. Map iteration follows insertion order and yields key/value tuples; list enumeration yields index/value tuples. With steps stop at the first error, each concrete handler preserves applicable source-arm order and requires exhaustiveness, and successful binders are unavailable in error handlers. Explicit error arms use Err(pattern) or an unbound wildcard; named catches use Err(name). Without else, errors propagate under the same enclosing Result constraint as postfix ?. No erased or fabricated Result payload types are introduced. The unavailable `/decision` skill is replaced by the established decision format.

### 53 Preserve abrupt control flow and function-exit cleanup
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will represent early termination explicitly through checking, typed IR and native emission. Deferred expressions become captured zero-argument Unit closures registered dynamically on a function-owned LIFO stack. Every normal return, explicit return and propagated Result error saves its value before draining that stack.
* **Context**: Conditionals and match arms can leave a function without producing a value for their enclosing expression. Fabricated operands would execute skipped effects or create invalid native joins. The dedicated DESIGN cleanup section requires function-exit semantics, including registrations in inner blocks, rather than lexical-block cleanup.
* **Consequences**: Only live control-flow predecessors contribute values. Let-else binds success values into the following scope and requires its failure branch to diverge. Deferred expressions capture immutable lexical values at registration but evaluate their code, including call arguments, at function exit. Cleanup must produce Unit and cannot return or propagate errors; a mandatory cleanup closure may handle captured Results. User lambdas have independent return and cleanup contexts. Interactive evaluation uses a separate bounded cleanup work budget so ordinary evaluation failures can still attempt cleanup while preserving the original failure. Actor cleanup remains outside this contract. The unavailable `/decision` skill is replaced by the established decision format.

### 52 Preserve immutable map and record-update semantics
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will implement maps with Int, Bool or String keys and arbitrary concrete values, using semantic key equality and immutable GC-managed pair storage. Entries iterate in insertion order; replacing a duplicate key keeps its position and the last value wins. Record updates evaluate their base and field expressions once in source order before constructing a fresh record.
* **Context**: Fern specifies Map literals and new/get/put/delete, but does not define key equality or ordering. The C runtime has no Map ABI, and its record-update emitter currently returns the unchanged base. Compiler-owned typed lowering gives native and interactive execution one explicit contract without inferring types from transport widths.
* **Consequences**: Map lookup and immutable updates initially take linear time; hashing is a later optimization behind the same semantics. Float and compound keys are rejected until an equality/hash contract is specified; values retain full-width Float, pointer, closure and Result representations. Deleting and reinserting a key appends it. Native tests use semantic expected output, including aliases, duplicate effects and record updates, rather than inheriting C's incomplete behavior. The source API also provides len/is_empty/contains/keys/values for inspection. Unknown or duplicate update fields are diagnostics. The unavailable `/decision` skill is replaced by the established decision format.

### 51 Lift typed closures with explicit environments
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will infer function values through semantic Function types, specialize generic functions before lifting closures, and use a uniform hidden environment argument for generated functions. Closures retain a code pointer and full-width captured values in GC-managed storage. Higher-order builtins execute typed calls through this convention.
* **Context**: Fern's documented anonymous functions and functional collection operations require captures that survive their defining call. The legacy C callback ABI has neither an environment parameter nor complete Float/Option transport. The persistent REPL recompiles source definitions, so numeric function IDs alone cannot identify code across entries.
* **Consequences**: Captures and arguments evaluate once in source order; native Float payloads preserve their bits. Builtin and runtime function values receive concrete typed wrappers. Interactive closures retain their originating immutable checked program. Each lambda has its own return/error context. Capturing already-produced Result-bearing values is temporarily rejected because delayed callbacks may never execute; lifting this restriction requires ownership/effect tracking across closures, aliases and containers. Functions returning Results are not themselves unhandled Result values. C remains the default until full migration gates pass.

### 50 Make directory listing failures explicit in the source API
* **Date**: 2026-09-05
* **Status**: Accepted for the unreleased frontend migration
* **Decision**: I will change `fs.list_dir` and its `File.list_dir` alias from `List(String)` to `Result(List(String), Int)` in both frontends. Empty directories return `Ok([])`; filesystem failures return an error, never an empty or partial success.
* **Context**: The legacy native helper returned NULL for open failures even though the source type promised a List. This prevented safe Rust lowering and could cause invalid pointer access. The user authorized completing the pre-1.0 migration, including the implementation work needed to make failure handling reliable. The unavailable `/decision` skill is replaced by this established decision format.
* **Consequences**: Callers must match, propagate, or otherwise handle the Result. Error codes distinguish missing paths, permission failures, non-directories, and other IO failures. Enumeration is bounded to 1,048,576 entries; exceeding that limit returns IO failure. The legacy nullable `fern_list_dir` C ABI remains, while source calls use `fern_read_dir_result`; Rust copies successful native StringLists into ordinary Lists. This is a breaking source change for the unreleased migration and must be announced with migration guidance; it must not be published as a backward-compatible patch or minor release under the compatibility policy.

### 49 Execute argument vectors without shell reconstruction
* **Date**: 2026-09-05
* **Status**: Accepted
* **Decision**: I will implement `System.exec_args` with `posix_spawnp` and literal argv, capturing stdout/stderr in private unlinked files.
* **Context**: The previous runtime reconstructed a shell command and underallocated its buffer when escaping single quotes. It contradicted the documented no-shell API and could corrupt memory.
* **Consequences**: Empty or missing commands and signal termination produce exit code -1; normal exit statuses and both streams are retained. Argument bytes never become shell syntax. Temporary capture descriptors are normalized above standard streams and closed after waiting; the separate `System.exec` API retains explicit shell semantics.

### 48 Preserve IEEE Float values across native and payload boundaries
* **Date**: 2026-09-05
* **Status**: Accepted
* **Decision**: I will lower Float as QBE double values, bitcast their 64-bit representation at generic collection/sum/record boundaries, and keep integer and floating arithmetic explicitly separate.
* **Context**: Fern specifies IEEE 754 doubles. Treating generic payload bits as numerical integers would corrupt values; raw-bit equality would mishandle signed zero and NaN.
* **Consequences**: Decimal/exponent literals, arithmetic/comparisons and printing use double semantics. Numeric literals must remain finite; runtime operations may produce IEEE infinities/NaNs. Printing uses system printf with 17 significant digits. Float List.contains remains rejected until value-aware lowering exists. Integer-to-Float coercion is not implicit.

### 47 Specialize generic code and preserve nominal type layouts
* **Date**: 2026-09-05
* **Status**: Accepted for Rust migration completion
* **Decision**: I will represent user types nominally, retain generic parameters in source syntax, and specialize generic functions and layouts into concrete typed IR. Custom sum/record values use GC-allocated storage containing a full-width discriminant and full-width fields; nested patterns inspect tags before reading payloads.
* **Context**: The user authorized completing the remaining migration milestones. Generic definitions and user types must scale beyond the initial built-in List/Option/Result cases while preserving the emitter's concrete-type boundary. The existing C runtime already exposes GC allocation.
* **Consequences**: Specialization, type expansion, and recursive matching are bounded and produce diagnostics when limits are exceeded. Runtime representations remain independent of source names; records use one constructor with named fields. Subsequent module loading must qualify declarations before type resolution. C remains available until executable-feature and tooling parity is verified. The unavailable `/decision` skill is replaced by this established decision format.

### 46 Extend Rust through typed collections and built-in sum types
* **Date**: 2026-09-05
* **Status**: Accepted for the incremental Rust frontend
* **Decision**: I will extend the Rust pipeline with recursive concrete List/Option/Result types, checked constructor inference, immutable list operations, exhaustive pattern matching, and postfix Result propagation before adding user-defined generic types. Resolved IR must contain no inference variables.
* **Context**: The user authorized continuing the measured Rust migration. Compound values test the type/ABI boundary more meaningfully than adding isolated scalar syntax. Existing packed Option runtime functions truncate payloads to 32 bits and cannot safely carry Strings or full Fern Int values.
* **Consequences**: Rust Option values use the existing heap-backed Result allocation/tag/payload helpers internally (Some maps to Ok; None to Err with an unused zero payload). This preserves 64-bit payloads without changing the shipping C compiler or its packed Option ABI. Calls to C APIs returning packed Options remain unsupported until explicit adapters exist. Lists and Results reuse their existing runtime representations. Match checking initially supports scalar literals, catchalls, and built-in constructor patterns with binding/wildcard payloads; unsupported nested patterns/guards receive diagnostics. General custom types, generics, and wider tooling remain subsequent milestones. The unavailable `/decision` skill is replaced by this established decision format.

### 45 Evaluate a safe Rust frontend with typed IR and the existing native backend
* **Date**: 2026-09-05
* **Status**: Accepted experiment; shipping C frontend remains default
* **Decision**: I will implement an independent, dependency-free Rust 2021 frontend prototype in `compiler-rs`, carry resolved types and symbol IDs through a typed IR, and reuse the vendored QBE backend and C runtime through an isolated backend process. I will compare the supported subset against specification-grounded native-output fixtures and the C compiler before recommending broader migration.
* **Context**: The user authorized a measured Rust migration experiment, superseding decision 2's C-only restriction for this prototype. Recent type reconstruction and pointer-lifetime bugs justify evaluating stronger implementation guarantees without replacing working native features. The `/decision` skill is unavailable; the established decision format is used directly.
* **Consequences**: Rust uses standard owned types, enums, Vec, and Result; C-specific Datatype99/SDS/arena rules remain applicable to C. Safe Rust is required (`forbid(unsafe_code)`), runtime behavior and Fern syntax do not change, unsupported prototype constructs produce explicit diagnostics, and the old compiler remains available. Bounds and parser depth are enforced; idiomatic type-enforced invariants replace redundant Rust assertions. A small process boundary isolates QBE's global state/abort behavior and avoids Rust FFI ownership hazards. Benchmark frontend work separately from shared backend/link work.

### 44 Verify native diagnostics before replacing the reference checker
* **Date**: 2026-09-05
* **Status**: Accepted
* **Decision**: I will require exact diagnostic multisets and exit codes on pinned failing fixtures and repository source before claiming native checker diagnostic parity; Python remains the default until the complete build/git/CLI workflow is validated.
* **Context**: Comparing successful exit codes alone hid missing checks and a native main function that always exited successfully.
* **Consequences**: CI requires diagnostic parity; full checker replacement remains an explicit open task. String constants use bounded printable runs and numeric unsafe bytes to preserve assembler-independent content and avoid truncation.

### 43 Actor invariants and explicit executable-feature boundaries
* **Date**: 2026-09-05
* **Status**: Accepted
* **Decision**: I will enforce acyclic single-owner supervision, one replacement per dead PID, zero-safe restart windows, and stopped-sibling preservation. Native build/run will reject unimplemented spawn/receive execution with actionable diagnostics while parse/check can still inspect planned syntax.
* **Context**: Runtime defects violated existing lifecycle promises; code generation previously created actor records without executing functions and evaluated receive arms without receiving messages. Those successful compilations concealed incorrect behavior.
* **Consequences**: Mailbox APIs remain executable, `send` preserves its real Result, and unsupported actor execution fails clearly. Full scheduling and descendant supervision are still required. Rejected registrations leave state unchanged; normal/shutdown children stay stopped unless explicitly restarted.

### 42 Relocatable compiler bundles and isolated run artifacts
* **Date**: 2026-09-05
* **Status**: Accepted
* **Decision**: I will install the runtime archive beside the compiler, resolve the actual executable location for runtime lookup, quote filesystem paths passed to the system toolchain, and create a private temporary directory for each `fern run` invocation.
* **Context**: Installing only the compiler and resolving argv[0] failed outside the checkout; unquoted paths broke ordinary directory names; predictable run paths could overwrite unrelated files. The `/decision` skill is unavailable in this checkout/session, so this entry follows the existing decision format directly.
* **Consequences**: Bundles remain relocatable, `PREFIX` supports local installation, and simultaneous runs have separate artifacts. Native compilation still requires the documented host compiler and libraries.

### 41 Deterministic terminal UI composition and interactive editing
* **Date**: 2026-09-05
* **Status**: Accepted
* **Decision**: I will reuse vendored linenoise for interactive prompt editing, preserve plain line reads for pipes, compose immutable trees with `new`, `add`, and `render`, expose deterministic log formatters, and emit cursor controls only on terminals.
* **Context**: Existing terminal modules need structured output and usable editing without another dependency or timing-dependent tests. The `/decision` skill is unavailable; this entry follows the existing format directly.
* **Consequences**: PTY tests cover interactive behavior and deterministic output fixtures cover trees/logs. Redirected output remains suitable for scripts.

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
