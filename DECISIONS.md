# Fern Language - Decision Log

This document tracks major architectural and technical decisions made during the development of the Fern programming language and compiler.

## Project Decision Log

### 10 Using <- operator instead of ?
* **Status**: ✅ Adopted
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
* **Context**: While `<-` handles simple error propagation, sometimes you need to handle different errors differently (e.g., return 404 for NotFound, 403 for PermissionDenied, 401 for AuthError). The `with` expression allows binding multiple Results and pattern matching on different error types in an `else` clause, similar to Gleam's `use` but with explicit error handling.
* **Consequences**: The parser must support `with`/`do`/`else` syntax. The type checker must verify all error types are handled in the else clause.

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
