# Fern Language - Decision Log

This document tracks major architectural and technical decisions made during the development of the Fern programming language and compiler.

## Project Decision Log

### 15 No named tuples (use records for named fields)
* **Status**: ✅ Adopted
* **Decision**: I will not support named tuple syntax `(x: 10, y: 20)`. Use positional tuples `(10, 20)` or declared records for named fields.
* **Context**: Named tuples create confusion because they look like records but aren't declared types. Users wouldn't know when to choose named tuples vs records. Keeping a clear distinction simplifies the mental model: tuples are positional and anonymous `(a, b, c)`, records are declared with `type` and have named fields. If you need named fields, declare a type.
* **Consequences**: Tuple syntax is positional only: `(10, 20)`. Named fields require a `type` declaration. Simpler grammar, clearer semantics, no ambiguity about tuple vs record.

### 14 No unless keyword
* **Status**: ✅ Adopted
* **Decision**: I will not include `unless` as a keyword. Use `if not` for negated conditions.
* **Context**: `unless` (from Ruby) is redundant with `if not` and adds cognitive overhead. Developers must mentally negate the condition to understand `unless`. It's especially confusing with already-negated conditions: `unless not ready`. Most Ruby style guides recommend avoiding `unless` with negations. Having one way to express conditionals (`if`) keeps the language simpler.
* **Consequences**: Only `if` for conditionals. Postfix conditionals use `if`: `return early if condition`. Negation uses `if not condition`. One less keyword to parse and teach.

### 13 No while or loop constructs (Gleam-style)
* **Status**: ✅ Adopted
* **Decision**: I will not include `while` or `loop` constructs. Stateful iteration uses recursion with tail-call optimization.
* **Context**: `while` and `loop` require mutable state between iterations, which conflicts with immutability. The semantics of rebinding inside loops are unclear and error-prone. Gleam takes the same approach: no loops, use recursion. Tail-call optimization makes recursion efficient. `for` loops over collections are kept since they don't require mutation - they're just iteration. Functional combinators (`fold`, `map`, `filter`, `find`) handle most cases elegantly.
* **Consequences**: Remove `while` and `loop` from grammar. Keep `for` for collection iteration. Recursion is the primary mechanism for stateful iteration. The lexer doesn't need TOKEN_WHILE or TOKEN_LOOP. Simplifies the language considerably.

### 12 Elixir-style record update syntax
* **Status**: ✅ Adopted
* **Decision**: I will use `%{ record | field: value }` syntax for record updates instead of `{ record | field: value }`.
* **Context**: The original `{ record | field: value }` syntax conflicts with the "no braces" philosophy - Fern uses indentation, not braces, for control flow. Using `%{...}` for record updates matches map literal syntax `%{"key": value}` and is inspired by Elixir. This creates consistency: both maps and record updates use `%{...}`.
* **Consequences**: Record update syntax is `%{ user | age: 31 }`. Map literals are `%{"key": value}`. Braces without `%` are not used.

### 11 Using ? operator for Result propagation
* **Status**: ✅ Adopted ⬆️ Supersedes [10]
* **Decision**: I will use the `?` operator (Rust-style, postfix) for Result propagation, keeping `<-` only inside `with` expressions.
* **Context**: After writing real examples, the postfix `?` works better than prefix `<-` because: (1) you see WHAT might fail before the `?`, not after, (2) it's familiar from Rust which is widely known, (3) it chains naturally `foo()?.bar()?.baz()?`, (4) it integrates cleanly with `let` bindings: `let x = fallible()?`. The `<-` syntax is preserved only inside `with` blocks for complex error handling, similar to Haskell's do-notation where `<-` is scoped.
* **Consequences**: The lexer needs `?` as TOKEN_QUESTION. The `<-` token (TOKEN_BIND) is only valid inside `with` blocks. Simple error propagation uses `let x = f()?`, complex handling uses `with x <- f(), ...`.

### 10 Using <- operator instead of ?
* **Status**: ⛔ Deprecated by [11]
* **Decision**: I will use the `<-` operator for Result binding instead of the `?` operator.
* **Context**: Initially considered Rust's `?` operator (postfix), but this has clarity issues: (1) it comes at the END of the expression, so you don't immediately see that an operation can fail, (2) `?` is overloaded in many languages (ternary, optional, etc.), making it less obvious. The `<-` operator (from Gleam/Roc) addresses both issues: it comes FIRST so failure is immediately visible, it reads naturally as "content comes from read_file", and it's not overloaded with other meanings.
* **Consequences**: All error handling examples use `<-` syntax. The lexer must recognize `<-` as a distinct token. Error messages reference `<-` in explanations.

### 9 No panics or crashes
* **Status**: ✅ Adopted
* **Decision**: I will eliminate all panic mechanisms from Fern - programs never crash from error conditions.
* **Context**: Many languages (Rust, Go, Swift) include panic/crash mechanisms for "impossible" errors. However, panics are the worst possible behavior: they're unpredictable, lose all error context, and can't be recovered from. In server scenarios, a panic can take down the entire application. Instead, ALL errors must be represented as `Result` types that force handling. There is no `.unwrap()`, no `panic()`, no `assert()` in production code.
* **Consequences**: The compiler must enforce that all `Result` values are handled. Error types must be comprehensive enough to represent all failure modes. Standard library functions that might fail must return `Result`. Debug builds can use `debug_assert()` for development.

### 8 Actor-based concurrency model
* **Status**: ✅ Adopted
* **Decision**: I will use an actor-based concurrency model (Erlang/Elixir style) instead of threads, async/await, or channels.
* **Context**: Considered several options: (1) OS threads - too heavy, difficult to reason about shared state, (2) async/await - complex, color functions, can't block, (3) Go-style goroutines with channels - better but still allows shared memory bugs, (4) Actor model - isolated processes with message passing only, no shared memory, supervisor trees for fault tolerance. The actor model provides the best balance of safety and expressiveness. Even though it adds ~500KB to binary size, this is acceptable given the safety and capability benefits (can replace Redis, RabbitMQ in many cases).
* **Consequences**: Need to implement a lightweight process scheduler, message queues, and supervision trees. All concurrent code uses message passing. The runtime will be slightly larger but provides superior reliability.

### 7 Labeled arguments for clarity
* **Status**: ✅ Adopted
* **Decision**: I will require labeled arguments for same-type parameters and all Boolean parameters.
* **Context**: Function calls like `connect("localhost", 8080, 5000, true, false)` are impossible to understand without checking the definition. Which number is the port? What do those booleans mean? Labeled arguments solve this: `connect(host: "localhost", port: 8080, timeout: 5000, retry: true, async: false)` is immediately clear. The compiler enforces labels when (1) multiple parameters have the same type, or (2) any parameter is a Boolean, preventing ambiguous calls.
* **Consequences**: Function calls are more verbose but dramatically more readable. The parser must support labeled argument syntax. The type checker must enforce label requirements.

### 6 with expression for complex error handling
* **Status**: ✅ Adopted
* **Decision**: I will provide a `with` expression for complex error handling scenarios where different error types need different responses.
* **Context**: While `?` handles simple error propagation, sometimes you need to handle different errors differently (e.g., return 404 for NotFound, 403 for PermissionDenied, 401 for AuthError). The `with` expression allows binding multiple Results using `<-` and pattern matching on different error types in an `else` clause, similar to Haskell's do-notation.
* **Consequences**: The parser must support `with`/`do`/`else` syntax. The `<-` operator is only valid inside `with` blocks. The type checker must verify all error types are handled in the else clause.

### 5 defer statement for resource cleanup
* **Status**: ✅ Adopted
* **Decision**: I will add a `defer` statement (from Zig) for guaranteed resource cleanup.
* **Context**: Resource cleanup (closing files, freeing locks, etc.) must be reliable even when errors occur. Considered: (1) try/finally blocks - verbose and easy to forget, (2) RAII/destructors - implicit, hard to see cleanup order, (3) `defer` statement - explicit, clear cleanup order (reverse of declaration), always runs on scope exit. Defer makes cleanup visible and guaranteed without ceremony.
* **Consequences**: The compiler must track defer statements and ensure they execute on all exit paths (return, error, normal). Deferred calls execute in reverse order of declaration.

### 4 Doc tests for reliability
* **Status**: ✅ Adopted
* **Decision**: I will support doc tests where examples in `@doc` comments are automatically tested.
* **Context**: Documentation often becomes stale because examples aren't verified. Rust's doc tests solve this by making documentation runnable and testable. This ensures examples always work and documentation stays current. It's especially valuable for AI-assisted development where examples serve as additional test cases.
* **Consequences**: The test runner must extract code blocks from `@doc` comments and execute them. Examples must be valid Fern code. Failed doc tests fail the build.

### 3 Python-style indentation syntax
* **Status**: ✅ Adopted
* **Decision**: I will use significant indentation (Python-style) instead of braces or `end` keywords.
* **Context**: Readability is a primary goal. Compared options: (1) Braces `{}` - familiar but add visual noise, (2) `end` keywords - clear but verbose, (3) Significant whitespace - clean and minimal. Python proves indentation works at scale. Modern editors handle indentation well. The reduced visual noise improves readability significantly.
* **Consequences**: The lexer must track indentation levels and emit INDENT/DEDENT tokens. Mixed tabs/spaces must be rejected. Error messages must handle indentation errors clearly.

### 2 Implementing compiler in C with safety libraries
* **Status**: ✅ Adopted
* **Decision**: I will implement the Fern compiler in C11 with safety libraries (arena allocator, SDS strings, stb_ds collections, Result macros) instead of Rust, Zig, or C++.
* **Context**: Considered several implementation languages: (1) Rust - safe but complex, steep learning curve, slower compile times, (2) Zig - interesting but immature, fewer AI training examples, (3) C++ - too complex, many ways to do things wrong, (4) C with safety libraries - simple, well-understood by AI, fast compilation, full control. Using arena allocation eliminates use-after-free and memory leaks. Using SDS eliminates buffer overflows. Using Result macros eliminates unchecked errors. C is also extremely well-represented in AI training data, making AI-assisted development highly effective.
* **Consequences**: Must use arena allocator exclusively (no malloc/free). Must use SDS for all strings. Must use stb_ds for collections. Must use Result types for fallible operations. Compiler warnings must be treated as errors.

### 1 Compiling to C via QBE
* **Status**: ✅ Adopted
* **Decision**: I will compile Fern to C using `QBE` as an intermediate representation.
* **Context**: I considered three approaches: (1) `LLVM` - powerful but extremely complex with 20+ million lines of code and steep learning curve, (2) Direct machine code generation - too low-level and platform-specific, requiring separate backends for each architecture, (3) `QBE` - a simple SSA-based IL that compiles to C. QBE hits the sweet spot: it's only ~10,000 lines of code (AI can understand it fully), generates efficient C code, handles register allocation and optimization, and lets me target any platform C supports. The C output can then be compiled with any C compiler (gcc, clang, tcc) for maximum portability.
* **Consequences**: I need to generate QBE IL from Fern AST, then invoke QBE to produce C code, and finally compile the C code to native binaries. This adds an extra compilation step but dramatically simplifies the compiler implementation and ensures broad platform support. Single binaries under 1MB are still achievable.
