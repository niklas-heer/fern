# Fern Implementation Roadmap

**Strategy:** Test-Driven Development with AI Assistance

Each milestone follows this pattern:
1. Write tests first (defines expected behavior)
2. AI implements against tests
3. Iterate until tests pass
4. Move to next milestone

## Milestone 0: Project Setup ✓ COMPLETE

**Status:** ✅ Complete - All infrastructure in place
**Completed:** 2024-01-28

**Goal:** Set up build system and testing infrastructure

### Tasks

- [x] Create project structure
  ```
  fern/
  ├── src/
  │   ├── main.c
  │   ├── lexer/
  │   ├── parser/
  │   ├── typechecker/
  │   ├── codegen/
  │   └── runtime/
  ├── lib/           # Safety libraries
  ├── tests/
  ├── examples/
  └── Makefile
  ```

- [x] Set up safety libraries
  - [x] Arena allocator (custom implementation)
  - [x] Result type macros (custom)
  - [x] String handling (custom String type)
  - [x] Vec macros for dynamic arrays

- [x] Set up build system
  - [x] Makefile with debug/release targets
  - [x] Strict compiler warnings (-Wall -Wextra -Wpedantic -Werror)
  - [x] Test target with colored output

- [x] Set up test framework
  - [x] Simple C test runner with assertions
  - [x] Color-coded test output
  - [x] Test organization structure

- [x] Development workflow
  - [x] Pre-commit hooks (compile + test)
  - [x] TDD guidelines in CLAUDE.md
  - [x] CI/CD pipeline (GitHub Actions)

**Success Criteria:** ✅ All Met
- ✅ `make` builds successfully
- ✅ `make test` runs with 23/23 tests passing
- ✅ `make debug` builds with debug symbols
- ✅ Pre-commit hooks prevent bad commits
- ✅ All documentation in place

---

## 🚨 CI Fix (BLOCKING)

**Status:** ✅ Fixed
**Completed:** 2026-01-28

**Issue:** Missing `#include <stdint.h>` caused `int64_t` to be undefined on Linux CI.

### Tasks

- [x] Add `#include <stdint.h>` to `tests/test_arena.c` (actual fix location)
- [x] Fix arena memory corruption bug in `arena_alloc_aligned` (alignment padding)
- [x] CI passes on both ubuntu-latest and macos runners

**Root Cause:** macOS includes `stdint.h` transitively through other headers, but Linux (ubuntu) requires explicit includes. Additionally, the arena allocator had a memory corruption bug when allocating blocks larger than the default size without accounting for alignment padding.

---

## Milestone 0.5: FERN_STYLE Enforcement

**Status:** 🚧 In Progress - Style checker implemented, audit needed
**Priority:** High - Should be implemented before significant code is written

**Goal:** Automated enforcement of FERN_STYLE.md coding standards

### Why This Matters

FERN_STYLE rules (inspired by TigerBeetle) prevent bugs and improve AI-assisted development:
- **2+ assertions per function** catches bugs early, documents invariants
- **70-line limit** keeps functions in AI context windows
- **Explicit bounds** prevents infinite loops and overflows
- **Pair assertions** catches serialization bugs

### Tasks

- [x] **Create `scripts/check_style.py`** - FERN_STYLE linter ✅
  - [x] Count assertions per function (min 2)
  - [x] Check function line count (max 70)
  - [x] Detect unbounded loops
  - [x] Check for `malloc`/`free` usage (should use arena)
  - [x] Report violations with file:line references

- [ ] **Create `scripts/check_assertions.sh`** - Quick assertion density check (optional)
  ```bash
  # Count functions vs assertions in a file
  # Flag any function with < 2 assertions
  ```

- [x] **Add Makefile targets** ✅
  - [x] `make style` - Check FERN_STYLE compliance
  - [x] `make style-strict` - Treat warnings as errors

- [ ] **Integrate with CI**
  - [ ] Add style check to GitHub Actions
  - [ ] Block PRs that violate FERN_STYLE
  - [ ] Generate style report on each build

- [ ] **Audit existing code** (many violations found)
  - [x] Run style checker on all existing code ✅
  - [ ] Add missing assertions to existing functions
  - [ ] Split functions over 70 lines
  - [ ] Add explicit bounds to all loops

- [ ] **Pre-commit hook**
  - [ ] Add style check to pre-commit hooks
  - [ ] Fail commit if style violations found

### Style Checker Output Example

```
$ make style
Checking FERN_STYLE compliance...

lib/lexer.c:
  ✗ lexer_next() - 0 assertions (need 2+)
  ✗ scan_string() - 85 lines (max 70)
  ✓ lexer_new() - 3 assertions

lib/parser.c:
  ✗ parse_expr() - 1 assertion (need 2+)
  ✗ Unbounded loop at line 234

Summary: 4 violations in 2 files
```

### Success Criteria

- [x] `make style` runs and reports violations ✅
- [ ] All existing code passes style check (or has TODO comments)
- [ ] CI blocks PRs with style violations
- [ ] Pre-commit hook catches violations locally
- [ ] New code automatically checked

### Current Status (2026-01-28)

Style checker implemented. Initial audit found violations:
- **Assertion density**: Most functions have 0-1 assertions (need 2+)
- **Function length**: Several functions exceed 70 lines (ast_print_expr: 430 lines, checker_infer_expr: 428 lines)
- **malloc/free**: Only in arena.c (expected, it implements the arena)

Run `make style` to see current violations.
- [ ] New code automatically checked

---

## Milestone 1: Lexer

**Status:** ✅ Complete - All lexer features implemented, 36 tests passing

**Goal:** Tokenize Fern source code

### Test Files

Create `tests/lexer/` with:

```
tests/lexer/
├── test_keywords.fn       # fn, let, if, match, etc.
├── test_operators.fn      # <-, ->, |>, +, -, etc.
├── test_literals.fn       # 42, 3.14, "hello", true
├── test_identifiers.fn    # variable_name, CamelCase
├── test_comments.fn       # #, /* */, @doc
├── test_indentation.fn    # Track indent/dedent
├── test_strings.fn        # "hello {name}", """multi"""
└── test_edge_cases.fn     # Empty file, Unicode, errors
```

### Tasks

- [x] **Write lexer tests first** (11 tests in tests/test_lexer.c)
  ```c
  // tests/lexer/test_keywords.c
  void test_lex_fn_keyword() {
      char* source = "fn main():";
      Token* tokens = lex(source);
      assert_token_type(tokens[0], TOKEN_FN);
      assert_token_type(tokens[1], TOKEN_IDENT);
      assert_token_value(tokens[1], "main");
      assert_token_type(tokens[2], TOKEN_LPAREN);
      // ...
  }
  ```

- [x] Implement token types (include/token.h, lib/token.c)
  ```c
  datatype(Token,
      (TokKeyword, sds keyword),
      (TokIdent, sds name),
      (TokInt, int64_t value),
      (TokFloat, double value),
      (TokString, sds value),
      (TokOperator, sds op),
      (TokIndent),
      (TokDedent),
      (TokNewline),
      (TokEof)
  );
  ```

- [x] Implement lexer.c (lib/lexer.c)
  - [x] Keyword recognition (let, fn, if, match, true, false, etc.)
  - [x] Operator recognition (<-, ->, ==, !=, <, <=, >, >=, etc.)
  - [x] Add `?` operator (TOKEN_QUESTION) for Result propagation
  - [x] Numeric literals (Int, Float)
  - [x] String literals with interpolation ("Hello, {name}!")
  - [x] Comment handling (# line comments, /* */ block comments)
  - [x] Indentation tracking (NEWLINE, INDENT, DEDENT tokens) - Python-style
  - [x] Error reporting with line/column
  - [x] **Unicode/emoji identifiers** (Decision #17)
    - [x] Support Unicode XID_Start/XID_Continue for identifier chars
    - [x] Support emoji codepoints in identifiers
    - [x] Tests: `let π = 3.14159`, `let 日本語 = "Japanese"`, `let 🚀 = launch()`
  - [x] **Emoji file extension** 🌿
    - [x] Accept `.🌿` as alternative to `.fn` file extension
    - [x] CLI documents both extensions in usage message

- [ ] Lexer utilities
  - [ ] Position tracking (file, line, column)
  - [ ] Error messages
  - [ ] Token pretty-printing (for debugging)

**Success Criteria:**
- All lexer tests pass
- `fern lex file.fn` prints tokens
- Error messages show correct position
- Handles all syntax elements from DESIGN.md

**Test Examples:**

```c
// Test: Basic function
void test_lex_basic_function() {
    char* source = 
        "fn greet(name: String) -> String:\n"
        "    \"Hello, {name}!\"";
    
    Token* tokens = lex(source);
    assert_token_sequence(tokens,
        TOKEN_FN, TOKEN_IDENT, TOKEN_LPAREN, TOKEN_IDENT, TOKEN_COLON,
        TOKEN_IDENT, TOKEN_RPAREN, TOKEN_ARROW, TOKEN_IDENT, TOKEN_COLON,
        TOKEN_INDENT, TOKEN_STRING, TOKEN_DEDENT, TOKEN_EOF
    );
}

// Test: ? operator (Result propagation)
void test_lex_question_operator() {
    char* source = "read_file(\"config.txt\")?";
    Token* tokens = lex(source);
    // Should have: CALL ... TOKEN_QUESTION
    assert_token_at(tokens, /* last */, TOKEN_QUESTION);
}

// Test: <- operator (only in with blocks)
void test_lex_bind_operator() {
    char* source = "x <- read_file(\"config.txt\")";
    Token* tokens = lex(source);
    assert_token_at(tokens, 1, TOKEN_BIND);  // <-
}

// Test: String interpolation
void test_lex_string_interpolation() {
    char* source = "\"Hello, {name}!\"";
    Token* tokens = lex(source);
    // Should produce: STRING_START, INTERP_START, IDENT, INTERP_END, STRING_END
}
```

---

## Milestone 2: Parser

**Status:** ✅ Complete - Core parser complete, 147 tests passing

**Goal:** Build AST from tokens

### Test Files

```
tests/parser/
├── test_expressions.fn    # 1 + 2, f(x), x |> f()
├── test_functions.fn      # fn definitions, clauses
├── test_types.fn          # Type annotations, definitions
├── test_patterns.fn       # match, destructuring
├── test_modules.fn        # import, module
├── test_error_handling.fn # <-, with, defer
└── test_parse_errors.fn   # Invalid syntax
```

### Tasks

- [x] **Write parser tests first** (13 tests in tests/test_parser.c)
  - [x] Integer, string, boolean literals
  - [x] Identifiers
  - [x] Binary operators with precedence
  - [x] Unary operators (-, not)
  - [x] Function calls (with/without arguments)
  - [x] Comparison operators
  - [x] Let statements
  - [x] Return statements

- [x] Define AST types (include/ast.h, lib/ast.c)
  - [x] Expression types (literals, identifiers, binary, unary, call, if, match, block, list, bind)
  - [x] Statement types (let, return, expression)
  - [x] Pattern types (identifier, wildcard, literal)
  - [x] Binary operators (arithmetic, comparison, logical, pipe)
  - [x] Unary operators (negation, not)
  - [x] Helper functions for creating AST nodes

- [x] Implement parser.c (lib/parser.c)
  - [x] Expression parsing (precedence climbing)
  - [x] Primary expressions (literals, identifiers, grouping)
  - [x] Binary operators with proper precedence
  - [x] Unary operators
  - [x] Function calls
  - [x] Let statements
  - [x] Return statements
  - [x] Pattern parsing (identifier, wildcard, int/string/bool literals)
  - [x] Type parsing (simple, parameterized, function types)
  - [x] Function definitions (basic — single clause with parameters and return type)
  - [x] Module declarations (module, import with path/items/alias)
  - [ ] Error recovery

- [ ] Parser utilities
  - [ ] Pretty-print AST
  - [ ] AST validation
  - [x] Error messages with context (basic implementation)

**Success Criteria:**
- ✅ Core parser tests pass (147/147)
- [ ] `fern parse file.fn` prints AST
- ✅ Error messages show parse issues (basic)
- [ ] Handles all syntax from DESIGN.md (partial - core expressions done)

**Test Examples:**

```c
// Test: <- operator
void test_parse_bind_operator() {
    char* source = "content <- read_file(\"config.txt\")";
    Expr* ast = parse(source);
    assert_expr_type(ast, EXPR_BIND);
    assert_string_eq(ast->bind.name, "content");
    assert_expr_type(ast->bind.value, EXPR_CALL);
}

// Test: with expression
void test_parse_with_expression() {
    char* source = 
        "with\n"
        "    x <- f(),\n"
        "    y <- g(x)\n"
        "do\n"
        "    Ok(y)\n"
        "else\n"
        "    Err(e) -> handle(e)";
    
    Expr* ast = parse(source);
    assert_expr_type(ast, EXPR_WITH);
    assert_int_eq(ast->with.bindings.len, 2);
}

// Test: Function clauses must be adjacent
void test_parse_function_clauses_adjacent() {
    char* source = 
        "fn factorial(0) -> 1\n"
        "fn other() -> 42\n"
        "fn factorial(n) -> n * factorial(n - 1)";
    
    ParseResult result = try_parse(source);
    assert(result.is_err);
    assert_error_contains(result.err, "clauses must be adjacent");
}
```

---

## Milestone 3: Type System ✓ COMPLETE

**Status:** ✅ Complete - Type checking and inference implemented
**Completed:** 2026-01-28
**Tests:** 249 total (66 type system tests)

**Goal:** Type checking and inference

### Completed Implementation

**Files Created:**
- `include/type.h` - Type representation
- `lib/type.c` - Type creation and utilities
- `include/type_env.h` - Type environment (symbol table)
- `lib/type_env.c` - Scoped type bindings
- `include/checker.h` - Type checker interface
- `lib/checker.c` - Type inference and checking
- `tests/test_type.c` - Type system unit tests
- `tests/test_checker.c` - Type checker integration tests

### Tasks

- [x] **Type representation** (type.h/type.c)
  - [x] Primitive types: Int, Float, String, Bool, Unit
  - [x] Type variables for generics (TYPE_VAR)
  - [x] Constructed types: List, Map, Option, Result
  - [x] Function types with params and result
  - [x] Tuple types
  - [x] Type equality and assignability checks
  - [x] Type to string conversion for error messages

- [x] **Type environment** (type_env.h/type_env.c)
  - [x] Scoped variable bindings (name → type)
  - [x] Type definitions for user-defined types
  - [x] Push/pop scope for blocks and functions
  - [x] Lookup with proper shadowing semantics

- [x] **Type checker** (checker.h/checker.c)
  - [x] Literal type inference (Int, Float, String, Bool)
  - [x] Binary operator checking (+, -, *, /, %, **, <, <=, >, >=, ==, !=, and, or)
  - [x] String concatenation with +
  - [x] Unary operator checking (-, not)
  - [x] Function call type checking with argument validation
  - [x] If expression checking (condition Bool, branches match)
  - [x] Block expression checking with scoping
  - [x] Let statement type inference and annotation validation
  - [x] Match expression checking (all arms same type)
  - [x] Pattern binding (identifier, wildcard, tuple, literal)
  - [x] `?` operator (Result unwrapping)
  - [x] Generic type instantiation with unification

- [x] **Type unification**
  - [x] Bind unbound type variables to concrete types
  - [x] Occurs check prevents infinite types
  - [x] Structural comparison for constructed types
  - [x] Fresh type variables for polymorphic function calls
  - [x] Substitution of bound variables

- [x] **Error messages**
  - [x] Type mismatch errors with expected vs got
  - [x] Undefined variable errors
  - [x] Cannot call non-function errors
  - [x] Argument count/type mismatch errors
  - [x] ? operator requires Result type errors

**Success Criteria:** ✅ All Met
- ✅ All type checker tests pass (97 tests)
- ✅ Type inference works for literals and expressions
- ✅ Generic functions instantiate correctly
- ✅ Error messages are clear and helpful

### Remaining Work (Future Enhancements)
- [ ] Trait checking (Show, Eq, Ord)
- [ ] Exhaustiveness checking for match
- [ ] Unhandled Result value detection
- [ ] Type inference across function boundaries

**Test Examples:**

```c
// Test: Unhandled Result
void test_unhandled_result_error() {
    char* source = 
        "fn main() -> Result((), Error):\n"
        "    let data = read_file(\"config.txt\")\n"  // Missing <-
        "    Ok(())";
    
    TypeResult result = typecheck(source);
    assert_error_type(result.err, ERROR_UNHANDLED_RESULT);
}

// Test: <- only in Result functions
void test_bind_requires_result_return() {
    char* source = 
        "fn process() -> ():\n"  // Returns (), not Result
        "    content <- read_file(\"test\")";
    
    TypeResult result = typecheck(source);
    assert(result.is_err);
    assert_error_contains(result.err, "can only be used in functions returning Result");
}

// Test: Type inference
void test_infer_generic_list() {
    char* source = "let numbers = [1, 2, 3]";
    Type* type = infer_type(source, "numbers");
    assert_type_eq(type, type_app(TYPE_LIST, TYPE_INT));
}
```

---

## Milestone 4: QBE Code Generation

**Status:** 🚧 In Progress - Core codegen complete, 32 tests passing
**Tests:** 331 total (32 codegen tests)

**Goal:** Generate QBE IR from AST

### Test Files

```
tests/test_codegen.c       # All codegen tests in single file
```

### Tasks

- [x] **Write codegen tests first** (TDD approach followed)

- [x] Implement QBE generation
  - [x] Basic expressions (int, float, bool, string literals)
  - [x] Binary operations (+, -, *, /, %, ==, !=, <, <=, >, >=)
  - [x] Unary operations (-, not)
  - [x] Function definitions with parameters
  - [x] Function calls
  - [x] If/else expressions with branches
  - [x] Let statements with pattern binding
  - [x] Match expressions (literal, wildcard, identifier patterns)
  - [x] Lambda expressions (compiled as anonymous functions)
  - [x] Tuple expressions (element generation)
  - [x] List expressions (runtime integration)
  - [x] Index expressions (list[i] via fern_list_get)
  - [x] ? operator (Result unwrapping with early return)
  - [x] Ok/Err constructors (via runtime functions)
  - [x] for loops (iteration over lists)
  - [x] defer statements (LIFO cleanup before returns)
  - [x] with expressions (Result binding with error paths)
  - [ ] Actor primitives (spawn, send, receive)

- [x] Runtime library
  - [x] Result type implementation (FernResult with tag/value)
  - [x] Result functions (fern_result_ok, fern_result_err, fern_result_is_ok, fern_result_unwrap)
  - [x] String type (FernString with length-prefixed data)
  - [x] String functions (fern_string_new, fern_string_concat, fern_string_eq)
  - [x] List type (FernList with dynamic capacity)
  - [x] List functions (fern_list_new, fern_list_push, fern_list_get)
  - [ ] Actor runtime (if spawn used)
  - [ ] libSQL bindings (if sql.open used)

**Success Criteria:**
- [x] Core codegen tests pass (38/38)
- [ ] `fern build file.fn` creates executable
- [ ] Generated code runs correctly
- [ ] Binary sizes match targets (CLI <1MB, server <4MB)

**Test Examples:**

```c
// Test: <- operator generates early return
void test_codegen_bind_operator() {
    char* source = 
        "fn load() -> Result(String, Error):\n"
        "    content <- read_file(\"test\")\n"
        "    Ok(content)";
    
    char* qbe = generate_qbe(source);
    // Should generate: if error, return Err
    assert_contains(qbe, "jnz");  // Jump if error
}

// Test: defer generates cleanup
void test_codegen_defer() {
    char* source = 
        "fn process() -> Result((), Error):\n"
        "    file <- open_file(\"test\")\n"
        "    defer close_file(file)\n"
        "    Ok(())";
    
    char* qbe = generate_qbe(source);
    // close_file should be at end of all paths
    assert_contains(qbe, "@close_file");
}
```

---

## Milestone 5: Standard Library Core

**Goal:** Implement core stdlib modules

### Priority Modules

```
stdlib/
├── core/
│   ├── result.fn      # Result type operations
│   ├── option.fn      # Option type operations
│   ├── list.fn        # List operations
│   ├── string.fn      # String operations
│   └── int.fn         # Integer operations
└── io/
    ├── file.fn        # File I/O
    └── stdio.fn       # stdin/stdout
```

### Tasks

- [ ] **Write stdlib tests first**
  ```fern
  # tests/stdlib/test_result.fn
  @doc """
  # Examples
  
  ```fern
  Ok(5) |> map((x) -> x * 2)  # => Ok(10)
  Err("fail") |> map((x) -> x * 2)  # => Err("fail")
  ```
  """
  ```

- [ ] Implement core modules
  - [ ] result.fn (map, and_then, unwrap_or, etc.)
  - [ ] option.fn (Some, None operations)
  - [ ] list.fn (map, filter, fold, etc.)
  - [ ] string.fn (split, join, trim, etc.)
  - [ ] file.fn (read_file, write_file, etc.)

- [ ] Test each module with doc tests
  - [ ] `fern test --doc` passes for all modules

**Success Criteria:**
- Core modules implemented
- All doc tests pass
- Can write basic Fern programs

---

## Milestone 6: CLI Tool

**Goal:** Complete `fern` command-line interface

### Tasks

- [ ] Implement CLI commands
  ```bash
  fern build file.fn        # Compile to binary
  fern run file.fn          # Compile and run
  fern check file.fn        # Type check only
  fern test                 # Run tests
  fern test --doc           # Run doc tests
  fern fmt file.fn          # Format code
  fern lex file.fn          # Debug: show tokens
  fern parse file.fn        # Debug: show AST
  ```

- [ ] Implement helpful error messages
  - [ ] All error types from DESIGN.md
  - [ ] Color output
  - [ ] Code snippets with indicators

- [ ] End-to-end tests
  - [ ] Test complete programs
  - [ ] Test error scenarios
  - [ ] Test all CLI commands

**Success Criteria:**
- Can compile and run Fern programs
- Error messages are helpful
- All examples from DESIGN.md work

---

## Milestone 7: Extended Standard Library

**Goal:** Implement commonly-needed stdlib modules

### Modules

```
stdlib/
├── json.fn           # JSON parsing
├── http/
│   ├── client.fn     # HTTP client
│   └── server.fn     # HTTP server
├── db/
│   └── sql.fn        # libSQL wrapper
└── test/
    └── assert.fn     # Test assertions
```

**Success Criteria:**
- Can build real applications
- HTTP server works
- Database integration works

---

## Milestone 8: Actor Runtime

**Goal:** Implement BEAM-inspired actor-based concurrency with supervision trees

### Tasks

- [ ] **Core Actor Runtime**
  - [ ] spawn() - Create lightweight processes
  - [ ] send() - Async message passing
  - [ ] receive - Pattern matching on mailbox
  - [ ] self() - Get current actor's PID
  - [ ] Message queues (mailboxes)
  - [ ] Cooperative scheduler (no OS threads per actor)

- [ ] **Supervision Trees**
  - [ ] Supervisor behaviour
  - [ ] Restart strategies:
    - [ ] `:one_for_one` - Restart only failed child
    - [ ] `:one_for_all` - Restart all children
    - [ ] `:rest_for_one` - Restart failed + subsequent children
  - [ ] Restart intensity (max_restarts, max_seconds)
  - [ ] Child specifications
  - [ ] Dynamic child management (start_child, terminate_child)

- [ ] **Process Linking & Monitoring**
  - [ ] link() / unlink() - Bidirectional failure propagation
  - [ ] monitor() / demonitor() - Unidirectional death notification
  - [ ] trap_exit flag - Convert exits to messages
  - [ ] Exit reasons (normal, shutdown, custom)

- [ ] **Testing with FernSim** (see FernSim section)
  - [ ] Deterministic scheduler for simulation
  - [ ] Fault injection hooks
  - [ ] Supervision strategy verification
  - [ ] Property-based actor tests

- [ ] **Example Patterns**
  - [ ] GenServer-style request/response
  - [ ] Cache actor with TTL
  - [ ] Job queue with workers
  - [ ] Pub/sub broker

**Success Criteria:**
- Actor examples from DESIGN.md work correctly
- Supervision trees handle failures per BEAM semantics
- FernSim finds no invariant violations after 1M simulated steps
- Binary includes runtime only when actors are used
- Performance: 100K+ actors, <1ms message latency

---

## Milestone 9: Polish & Optimization

**Goal:** Production-ready compiler

### Tasks

- [ ] Performance optimization
  - [ ] Compilation speed
  - [ ] Binary size optimization
  - [ ] Runtime performance

- [ ] Tooling improvements
  - [ ] LSP implementation
  - [ ] Formatter
  - [ ] Linter

- [ ] Documentation
  - [ ] Language guide
  - [ ] Standard library docs
  - [ ] Tutorial

**Success Criteria:**
- Binary size targets met
- Performance targets met
- Documentation complete
- v0.1 release ready

---

## Testing Strategy

### Unit Tests (C)
```c
// tests/lexer/test_keywords.c
void test_lex_fn_keyword() {
    Token* tokens = lex("fn main():");
    assert_token_type(tokens[0], TOKEN_FN);
}
```

### Integration Tests (Fern)
```fern
# tests/integration/test_http_server.fn
fn test_http_server():
    # Test actual Fern program
```

### Doc Tests (Automatic)
```fern
@doc """
# Examples

```fern
add(2, 3)  # => 5
```
"""
pub fn add(a: Int, b: Int) -> Int:
    a + b
```

### Performance Tests
```c
void benchmark_lexer() {
    // Measure tokens/second
}
```

---

## Development Workflow

For each feature:

1. **Write tests first**
   - Define expected behavior
   - Cover edge cases
   - Include error cases

2. **AI implements**
   - Provide test file
   - AI writes implementation
   - Run tests

3. **Iterate**
   - Fix failures
   - Add missing tests
   - Refactor

4. **Document**
   - Add examples
   - Update docs
   - Write changelog

5. **Move to next feature**

---

## Success Metrics

### Milestone 1-3 (Foundation)
- [ ] All tests pass
- [ ] Can lex, parse, typecheck Fern code
- [ ] Error messages are helpful

### Milestone 4-6 (Compiler)
- [ ] Can compile to binary
- [ ] Binary size < 1MB (CLI mode)
- [ ] All examples from DESIGN.md work

### Milestone 7-9 (Complete)
- [ ] Standard library complete
- [ ] Actor runtime working
- [ ] Binary size targets met
- [ ] Ready for v0.1 release

---

## Getting Started

To begin implementation:

```bash
# 1. Start with Milestone 0
cd fern
mkdir -p src/{lexer,parser,typechecker,codegen}
mkdir -p tests/{lexer,parser,typechecker,codegen}
mkdir -p lib

# 2. Write first test
cat > tests/lexer/test_keywords.c << 'EOF'
#include "test.h"

void test_lex_fn_keyword() {
    Token* tokens = lex("fn");
    assert_token_type(tokens[0], TOKEN_FN);
}
EOF

# 3. Run test (will fail)
make test

# 4. Implement until it passes
# 5. Move to next test
```

---

## Notes for AI Implementation

**Required Reading:**
- **FERN_STYLE.md** — Coding standards (assertion density, function limits, etc.)
- **CLAUDE.md** — TDD workflow and safety rules
- **DESIGN.md** — Language specification

**FERN_STYLE Requirements (TigerBeetle-inspired):**
- Minimum **2 assertions per function**
- Maximum **70 lines per function**
- **Pair assertions** for critical operations (validate before write AND after read)
- **Explicit bounds** on all loops and buffers
- **Compile-time assertions** for type sizes and constants
- Comments explain WHY, not just WHAT

**When implementing:**
- Always run tests after changes
- Use AddressSanitizer to catch memory errors
- Follow FERN_STYLE.md guidelines
- Keep functions under 70 lines
- Add assertions for all invariants

**Error handling:**
- All functions return Result types
- Check pointers before dereferencing
- Use arena allocation, never malloc/free directly

**Code style:**
- Keep C code simple
- Use Datatype99 for tagged unions
- Use SDS for all strings
- Use stb_ds for hash maps and arrays

---

## FernFuzz - Grammar-Based Fuzzer (Planned)

**Status**: Planned for after Parser milestone

Inspired by TigerBeetle's VOPR, FernFuzz will test the compiler with random programs.

### Components

1. **Random Program Generator**
   - Generate valid Fern programs from the grammar
   - Deterministic from seed (reproducible failures)
   - Configurable depth and complexity

2. **Property Tests**
   - **No crashes**: Any input must not crash the compiler
   - **Round-trip**: `parse(print(parse(x))) == parse(x)`
   - **Error messages**: Invalid input produces helpful errors

3. **Differential Testing**
   - Compare behavior across compiler versions
   - Detect regressions automatically

### Usage (Planned)

```bash
# Run 10,000 random programs
make fuzz

# Reproduce a failure
make fuzz SEED=0x1234567890abcdef

# Run continuously
make fuzz-forever
```

### Implementation Tasks

- [ ] Create `tests/fuzz/fuzz_generator.c` — Generate random ASTs
- [ ] Create `tests/fuzz/fuzz_runner.c` — Run fuzzer with seeds
- [ ] Add round-trip property tests
- [ ] Add crash detection
- [ ] Integrate with CI (run on every PR)

---

## FernSim - Deterministic Actor Simulation (Planned)

**Status**: Planned for Milestone 8 (Actor Runtime)
**Inspiration**: TigerBeetle's VOPR, FoundationDB's simulation testing, Erlang/BEAM

To achieve BEAM-level reliability for the actor system and supervision trees,
we need deterministic simulation testing that can explore edge cases impossible
to hit in real-world testing.

### Why Deterministic Simulation?

The BEAM's legendary reliability comes from decades of battle-testing. We can
accelerate this by using simulation:

- **Explore interleavings**: Test millions of process scheduling orders
- **Inject failures**: Simulate crashes, timeouts, network issues deterministically
- **Reproduce bugs**: Any failure can be replayed with its seed
- **Time travel**: Control virtual clocks to test timeout behavior
- **Find rare bugs**: Edge cases that would take years to hit naturally

### Components

1. **Deterministic Scheduler**
   - Seed-based random scheduling of actor execution
   - Reproducible message delivery ordering
   - Virtual time (no real sleeps, instant "timeout" testing)

2. **Fault Injection**
   - **Process crashes**: Random actor deaths at any point
   - **Supervisor restarts**: Verify restart strategies work
   - **Message loss**: Simulate network unreliability
   - **Slow actors**: Mailbox backpressure scenarios
   - **Resource exhaustion**: Memory limits, mailbox overflow

3. **Supervision Tree Verification**
   - `:one_for_one` - Only crashed child restarts
   - `:one_for_all` - All children restart on any crash
   - `:rest_for_one` - Crashed child and those after it restart
   - Restart intensity limits (max restarts per period)
   - Cascading failure handling

4. **Property Tests**
   - **No orphan messages**: All sent messages are eventually received or actor is dead
   - **Supervisor invariants**: Children always restarted according to strategy
   - **No deadlocks**: System always makes progress
   - **Graceful degradation**: Failures don't cascade unexpectedly

### Example Simulation Scenarios

```c
// Scenario: Supervisor with 3 workers, one_for_one strategy
// Inject: Random worker crashes
// Verify: Only crashed worker restarts, others unaffected

SimConfig config = {
    .seed = 0xDEADBEEF,
    .max_steps = 100000,
    .fault_injection = {
        .process_crash_probability = 0.01,
        .message_delay_probability = 0.05,
    },
};

SimResult result = fernsim_run(config, supervisor_scenario);
assert(result.invariant_violations == 0);
assert(result.deadlocks == 0);
```

### Usage (Planned)

```bash
# Run actor simulation with random seed
make sim

# Reproduce a specific failure
make sim SEED=0xDEADBEEF

# Run overnight with many seeds
make sim-overnight ITERATIONS=1000000

# Test specific supervision strategy
make sim SCENARIO=one_for_all_stress
```

### Implementation Tasks

**Phase 1: Core Infrastructure**
- [ ] Deterministic PRNG with seed support
- [ ] Virtual clock implementation
- [ ] Simulated actor scheduler (no OS threads)
- [ ] Event trace recording for replay

**Phase 2: Fault Injection**
- [ ] Process crash injection
- [ ] Message delay/loss simulation
- [ ] Mailbox overflow scenarios
- [ ] Timeout simulation via virtual time

**Phase 3: Supervision Testing**
- [ ] one_for_one strategy verification
- [ ] one_for_all strategy verification
- [ ] rest_for_one strategy verification
- [ ] Restart intensity limit testing
- [ ] Cascading failure scenarios

**Phase 4: Property-Based Testing**
- [ ] Invariant checkers (no orphans, no deadlocks)
- [ ] Shrinking (find minimal failing case)
- [ ] Coverage tracking (which interleavings tested)
- [ ] CI integration (run on every PR)

### Success Criteria

- Can reproduce any actor bug with a seed
- Finds bugs that real-world testing misses
- Supervision trees verified against all failure modes
- Confidence to claim "BEAM-like reliability"

---

## FernDoc - Documentation Generation (Planned)

**Status**: Planned (Decision #18)
**Inspiration**: HexDocs (Elixir), rustdoc, godoc

Two documentation systems sharing a unified HTML template:

### 1. Fern Language Documentation (`fern doc`)

Built-in command for generating documentation from Fern source code.

**Features:**
- Parse `@doc` comments with markdown support
- Extract function signatures, type definitions, module structure
- Generate searchable HTML with syntax highlighting
- Support doc tests (examples that are automatically tested)
- Cross-reference linking between modules

**Usage (Planned):**
```bash
# Generate docs for current project
fern doc

# Generate and open in browser
fern doc --open

# Generate docs for specific module
fern doc src/mymodule.fn
```

**Example Fern documentation:**
```fern
@doc """
Reads the contents of a file and returns them as a String.

## Examples

    let content = read_file("config.txt")?
    print(content)

## Errors

Returns `Err(IoError)` if the file cannot be read.
"""
fn read_file(path: String) -> Result[String, IoError]:
    # implementation
```

### 2. Compiler Documentation (C code)

Custom doc generator for the Fern compiler source.

**Features:**
- Parse `/** */` doc comments in C code
- Extract function signatures, struct definitions, enums
- Understand FERN_STYLE conventions (assertions as contracts)
- Generate HTML matching Fern doc style
- Document the compiler architecture

**Comment format:**
```c
/**
 * @brief Tokenize Fern source code into a stream of tokens.
 *
 * @param arena Memory arena for allocations
 * @param source Source code string to tokenize
 * @return Lexer instance ready for iteration
 *
 * @example
 *   Lexer* lex = lexer_new(arena, "let x = 42");
 *   Token tok = lexer_next(lex);
 */
Lexer* lexer_new(Arena* arena, const char* source);
```

**Usage (Planned):**
```bash
# Generate compiler docs
make docs

# Generate and serve locally
make docs-serve
```

### Shared Infrastructure

- **HTML Templates**: Responsive, searchable, dark-mode support
- **Search Index**: Client-side search (lunr.js or similar)
- **Syntax Highlighting**: Fern and C code highlighting
- **CI Integration**: Auto-publish to GitHub Pages on release

### Implementation Tasks

**Phase 1: Fern Language Docs**
- [ ] Define `@doc` comment syntax in DESIGN.md
- [ ] Implement doc comment parsing in lexer/parser
- [ ] Create `fern doc` CLI command
- [ ] Build HTML template with search
- [ ] Add syntax highlighting for Fern code
- [ ] Implement doc test extraction and running

**Phase 2: Compiler Docs**
- [ ] Create `scripts/generate_docs.py` — C doc extractor
- [ ] Parse `/** */` comments and function signatures
- [ ] Reuse HTML templates from Phase 1
- [ ] Add C syntax highlighting
- [ ] Add `make docs` target

**Phase 3: Polish**
- [ ] Cross-reference linking
- [ ] Version selector (multiple doc versions)
- [ ] GitHub Pages deployment in CI
- [ ] Dark mode toggle
- [ ] Mobile-responsive design

---

## Iteration 9: Function Definition Parsing

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement function definition parsing with parameters and return types

### Completed Task

- [x] Implement function definition parsing ✅ VERIFIED

**Tests Written**:
- test_parse_function_no_params() - Parse: `fn main() -> (): Ok(())` ✓
- test_parse_function_with_params() - Parse: `fn add(x: Int, y: Int) -> Int: x + y` ✓
- test_parse_function_with_body() - Parse function with block body ✓

**Files Modified**:
- tests/test_parser.c (added 3 new tests)
- include/ast.h (added FunctionDef, Parameter structs)
- lib/ast.c (added stmt_fn helper)
- lib/parser.c (added function definition parsing, updated parse_type for unit type)

**Success Criteria Met**:
- [x] All three new tests pass
- [x] No regression in existing tests (61 → 64 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 3 failing tests for function definitions (602a1b1)
2. GREEN phase: Implemented function definition parsing (14ca62e)

**AST Changes:**
- Added `Parameter` struct (name + type annotation) and `ParameterVec`
- Added `FunctionDef` struct (name, params, return_type, body)
- Added `STMT_FN` statement type
- Added `stmt_fn()` helper function

**Parser Changes:**
- Added function definition parsing in `parse_stmt()` — triggered by `TOKEN_FN`
- Parses syntax: `fn name(param: Type, param: Type) -> ReturnType: body_expr`
- Parameters are comma-separated `name: Type` pairs
- Return type annotation is optional (after `->`)
- Body is a single expression after `:`
- Updated `parse_type()` to handle `()` as unit type (not just function types)

**Key Design Decision:**
The `()` unit type is handled in `parse_type()` — when `()` is not followed by `->`, it's treated as a named type "()" rather than a function type. This allows `fn main() -> (): ...` to parse correctly.

Test Results:
```
Total:  64
Passed: 64
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28

✅ ACCEPTED - Function definition parsing implementation

Verification Results:
- Tests: 64/64 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Follows existing parser patterns ✓
- TDD workflow followed correctly ✓

Success Criteria Met:
- [x] All three new tests pass
- [x] No regression (61 → 64 tests, all passing)
- [x] No compiler warnings
- [x] Clean AST design with proper structs

Code Review:
- Parameter and FunctionDef structs: Clean design ✓
- stmt_fn() helper: Follows existing patterns ✓
- parse_type() enhancement: Smart handling of unit type ✓
- Function parsing logic: Clear and correct ✓

Commits reviewed:
- 602a1b1: Tests (RED phase) ✓
- 14ca62e: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
Completed 9 iterations with 64/64 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let, return, expression statements)
- Result handling (← bind operator)
- Function composition (|> pipe operator)
- Type annotations (simple, parameterized, function types)
- **NEW**: Function definitions with parameters, return types, and bodies

Ready for next task.

---

## Iteration 10: Pattern Parsing Enhancement

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Enhance pattern parsing beyond basic identifiers

### Task Requirements

Implement comprehensive pattern parsing for match expressions:
```fern
match value:
    42 -> "int literal"
    "hello" -> "string literal"
    true -> "bool literal"
    [1, 2, 3] -> "list pattern"
    (x, y) -> "tuple pattern"
    _ -> "wildcard"
```

**Tests to Write** (TDD - RED phase first):
- test_parse_pattern_int_literal() - Parse: `match x: 42 -> "found"`
- test_parse_pattern_string_literal() - Parse: `match x: "test" -> "found"`
- test_parse_pattern_bool_literal() - Parse: `match x: true -> "yes", false -> "no"`
- test_parse_pattern_wildcard() - Parse: `match x: _ -> "anything"` (already passes, verify)
- test_parse_pattern_identifier() - Parse: `match x: value -> value` (binding pattern)

**Expected Changes**:
- tests/test_parser.c (add 5+ new tests)
- include/ast.h (expand Pattern struct if needed)
- lib/parser.c (enhance parse_pattern to handle all literal types)

**Success Criteria**:
- [x] All new pattern parsing tests pass
- [x] No regression in existing 64 tests (64 → 69 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN → update ROADMAP)

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 5 tests for pattern parsing; 4 passed immediately, 1 failed (identifier pattern)
2. GREEN phase: Enhanced match pattern parsing to distinguish identifiers from literals

**Tests Written**:
- test_parse_pattern_int_literal() - Parse: `match x: 42 -> "found"` ✓
- test_parse_pattern_string_literal() - Parse: `match x: "test" -> "found"` ✓
- test_parse_pattern_bool_literal() - Parse: `match x: true -> "yes", false -> "no"` ✓
- test_parse_pattern_wildcard() - Parse: `match x: _ -> "anything"` ✓
- test_parse_pattern_identifier() - Parse: `match x: value -> value` ✓

**Files Modified**:
- tests/test_parser.c (added 5 new tests)
- lib/parser.c (enhanced pattern parsing in match expression)

**Parser Change**:
The match pattern parser previously used `parse_primary_internal()` for all non-wildcard patterns, wrapping the result as `PATTERN_LIT`. This incorrectly treated identifiers as literal patterns. The fix adds an explicit `TOKEN_IDENT` check before the general literal fallback:
- `TOKEN_UNDERSCORE` → `PATTERN_WILDCARD` (unchanged)
- `TOKEN_IDENT` → `PATTERN_IDENT` (NEW: binding pattern)
- Everything else → `PATTERN_LIT` via `parse_primary_internal()` (int, string, bool literals)

This correctly distinguishes binding patterns (`value -> ...`) from literal patterns (`42 -> ...`).

Test Results:
```
Total:  69
Passed: 69
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28

✅ ACCEPTED - Pattern parsing enhancement

Verification Results:
- Tests: 69/69 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Minimal changes for maximum impact ✓
- Correct semantic distinction (binding vs literal) ✓

Success Criteria Met:
- [x] All 5 new tests pass
- [x] No regression (64 → 69 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

Code Review:
- Parser enhancement: Clean 5-line fix to distinguish identifier patterns ✓
- Test coverage: Comprehensive (int, string, bool literals + wildcard + identifier) ✓
- Semantic correctness: Properly distinguishes binding patterns from literal patterns ✓
- Implementation quality: Minimal, focused change ✓

Commits reviewed:
- 3844cad: Tests (RED phase) ✓
- 406ffb3: ROADMAP verification ✓

**Parser Milestone Progress:**
Completed 10 iterations with 69/69 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with comprehensive patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let, return, expression statements)
- Result handling (← bind operator)
- Function composition (|> pipe operator)
- Type annotations (simple, parameterized, function types)
- Function definitions (parameters, return types, bodies)
- **ENHANCED**: Pattern matching (literals, wildcards, identifier bindings)

Parser Milestone 2 is nearly complete. Remaining work:
- Type parsing for function parameters (integrate parse_type)
- Module declarations
- Error recovery

---

## Iteration 11: Let Statements with Type Annotations

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Add optional type annotations to let statements

### Task Requirements

Enhance let statement parsing to support optional type annotations:
```fern
let x: Int = 42
let name: String = "Alice"
let items: List(Int) = [1, 2, 3]
let callback: (Int) -> Bool = is_even
```

**Tests to Write** (TDD - RED phase first):
- test_parse_let_with_type_int() - Parse: `let x: Int = 42`
- test_parse_let_with_type_string() - Parse: `let name: String = "test"`
- test_parse_let_with_type_parameterized() - Parse: `let items: List(Int) = [1, 2]`
- test_parse_let_with_type_function() - Parse: `let f: (Int) -> Int = double`
- test_parse_let_without_type() - Parse: `let x = 42` (verify existing behavior)

**Expected Changes**:
- tests/test_parser.c (add 5 new tests)
- include/ast.h (add optional `TypeExpr* type` field to LetStmt)
- lib/parser.c (enhance parse_stmt for let to optionally parse type annotation after colon)

**Success Criteria**:
- [x] All 5 new tests pass
- [x] No regression in existing 69 tests (69 → 74 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN → update ROADMAP)

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 5 tests for let type annotations; 4 failed, 1 passed (55ee1de)
2. GREEN phase: Enhanced let statement parsing to optionally parse type annotations (b9928f6)

**Tests Written**:
- test_parse_let_with_type_int() - Parse: `let x: Int = 42` ✓
- test_parse_let_with_type_string() - Parse: `let name: String = "test"` ✓
- test_parse_let_with_type_parameterized() - Parse: `let items: List(Int) = [1, 2]` ✓
- test_parse_let_with_type_function() - Parse: `let f: (Int) -> Int = double` ✓
- test_parse_let_without_type() - Parse: `let x = 42` (existing behavior verified) ✓

**Files Modified**:
- tests/test_parser.c (added 5 new tests)
- lib/parser.c (enhanced let statement parsing to optionally parse `: Type` before `=`)

**Parser Change**:
The let statement parser previously skipped type annotations (always set `type_ann = NULL`).
The fix adds a check for TOKEN_COLON after the variable name. If found, it calls
`parse_type()` to parse the type annotation before consuming `=` and the value expression.
This leverages the existing `parse_type()` infrastructure that already handles simple types
(Int, String), parameterized types (List(Int), Result(String, Error)), and function types
((Int) -> Int).

Syntax: `let <name> [: <type>] = <expr>`

Test Results:
```
Total:  74
Passed: 74
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28

✅ ACCEPTED - Let statement type annotation implementation

Verification Results:
- Tests: 74/74 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Minimal, focused change ✓
- Proper integration with existing parse_type() ✓
- TDD workflow followed correctly ✓

Success Criteria Met:
- [x] All 5 new tests pass
- [x] No regression (69 → 74 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

Code Review:
- Parser enhancement: Clean 7-line change to add optional type annotation ✓
- Test coverage: Comprehensive (simple, parameterized, function types + backward compat) ✓
- Implementation quality: Minimal, correct, leverages existing infrastructure ✓
- Excellent commit message with details ✓

Commits reviewed:
- 55ee1de: Tests (RED phase) ✓
- b9928f6: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
Completed 11 iterations with 74/74 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with comprehensive patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let with optional type annotations, return, expression statements)
- Result handling (← bind operator)
- Function composition (|> pipe operator)
- Type annotations (simple, parameterized, function types)
- Function definitions (parameters, return types, bodies)
- Pattern matching (literals, wildcards, identifier bindings)

Parser Milestone 2 is nearly complete. Remaining work:
- Module declarations
- Error recovery
- Indentation tracking (for production-ready code)

---

## Iteration 12: Multi-Clause Function Definitions

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement multi-clause function definitions (pattern-based dispatch)

### Task Requirements

Implement parsing for functions with multiple clauses using pattern matching:
```fern
fn factorial(0) -> 1
fn factorial(n) -> n * factorial(n - 1)

fn fibonacci(0) -> 0
fn fibonacci(1) -> 1
fn fibonacci(n) -> fibonacci(n - 1) + fibonacci(n - 2)
```

**Tests to Write** (TDD - RED phase first):
- test_parse_function_multi_clause_simple() - Parse: `fn fact(0) -> 1` then `fn fact(n) -> n * fact(n - 1)`
- test_parse_function_multi_clause_fibonacci() - Parse fibonacci with 3 clauses
- test_parse_function_clauses_must_be_adjacent() - Parse error when clauses are separated by other definitions
- test_parse_function_pattern_params() - Parse: `fn greet("Alice") -> "Hi Alice"`, `fn greet(name) -> "Hello {name}"`

**Expected Changes**:
- tests/test_parser.c (add 4+ new tests)
- include/ast.h (modify FunctionDef to support multiple clauses, add clause grouping)
- lib/parser.c (enhance function parsing to collect adjacent clauses by name)

**Success Criteria**:
- [x] All 4 new tests pass
- [x] No regression in existing 74 tests (74 → 78 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN → update ROADMAP)

**Key Design Considerations**:
- Function clauses with the same name must be adjacent (no interleaving)
- Each clause can have different parameter patterns
- Parser should group clauses into a single FunctionDef with multiple clauses
- Error if clauses are not adjacent

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 4 failing tests for multi-clause function definitions (bb00f66)
2. GREEN phase: Implemented multi-clause function parsing

**Tests Written**:
- test_parse_function_multi_clause_simple() - Parse: `fn fact(0) -> 1` then `fn fact(n) -> n * fact(n - 1)` ✓
- test_parse_function_multi_clause_fibonacci() - Parse fibonacci with 3 clauses ✓
- test_parse_function_clauses_must_be_adjacent() - Parse error when clauses are separated ✓
- test_parse_function_pattern_params() - Parse: `fn greet("Alice") -> "Hi Alice"`, `fn greet(name) -> "Hello"` ✓

**Files Modified**:
- tests/test_parser.c (added 4 new tests)
- include/ast.h (added FunctionClause, FunctionClauseVec, PatternVec; extended FunctionDef with clauses field)
- include/parser.h (added parse_stmts declaration)
- lib/ast.c (initialize clauses to NULL in stmt_fn)
- lib/parser.c (added is_typed_params, parse_pattern, multi-clause fn parsing, parse_stmts with clause grouping)

**AST Changes:**
- Added `PatternVec` (vector of Pattern*)
- Added `FunctionClause` struct (params: PatternVec*, return_type: TypeExpr*, body: Expr*)
- Added `FunctionClauseVec` (vector of FunctionClause)
- Extended `FunctionDef` with optional `clauses` field (NULL for single-clause typed functions)

**Parser Changes:**
- Added `is_typed_params()` — uses `lexer_peek` to detect whether fn parameters are typed (name: Type) or pattern-based. IDENT followed by COLON means typed; everything else means pattern.
- Added `parse_pattern()` — reusable pattern parser for wildcard, identifier, and literal patterns (extracted from match expression parsing logic).
- Enhanced `parse_stmt()` for `TOKEN_FN` — now branches into single-clause (typed params, colon body) or multi-clause (pattern params, arrow body).
- Added `parse_stmts()` — parses multiple statements, groups adjacent `fn` clauses with same name into single FunctionDef, and emits error for non-adjacent duplicate clauses.

**Key Design Decision:**
The two function forms are distinguished syntactically:
- Single-clause: `fn name(param: Type) -> RetType: body` (colon before body)
- Multi-clause: `fn name(pattern) -> body` (arrow before body, no colon)
Detection uses `lexer_peek` for 2-token lookahead (IDENT + COLON = typed params).

Test Results:
```
Total:  78
Passed: 78
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28

✅ ACCEPTED - Multi-clause function definition implementation

Verification Results:
- Tests: 78/78 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Complex feature with proper architecture ✓
- TDD workflow followed correctly ✓
- Adjacent clause validation working ✓

Success Criteria Met:
- [x] All 4 new tests pass
- [x] No regression (74 → 78 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

Code Review:
- AST changes (FunctionClause, PatternVec): Clean design ✓
- parse_pattern() extraction: Good refactoring ✓
- is_typed_params() lookahead: Smart disambiguation ✓
- parse_stmts() clause grouping: Correct implementation ✓
- Adjacent clause validation: Proper error handling ✓
- 2-token lookahead with lexer_peek: Efficient ✓

Commits reviewed:
- bb00f66: Tests (RED phase) ✓
- 33c86e2: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
Completed 12 iterations with 78/78 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with comprehensive patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let with optional type annotations, return, expression statements)
- Result handling (← bind operator)
- Function composition (|> pipe operator)
- Type annotations (simple, parameterized, function types)
- Function definitions (single-clause typed, multi-clause pattern-based)
- Pattern matching (literals, wildcards, identifier bindings)

This completes a major milestone: **full function definition support** including both typed single-clause functions and pattern-based multi-clause functions. The parser architecture is now robust enough for more advanced features.

Ready for next task.

---

## Iteration 13: With Expression Parsing

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement with expression parsing for chained Result handling

### Task Requirements

Implement parsing for `with` expressions that handle multiple Result-returning operations:
```fern
with
    x <- f(),
    y <- g(x)
do
    Ok(y)
else
    Err(e) -> handle(e)
```

**Tests to Write** (TDD - RED phase first):
- test_parse_with_simple() - Parse: `with x <- f() do Ok(x)`
- test_parse_with_multiple_bindings() - Parse: `with x <- f(), y <- g(x) do Ok(y)`
- test_parse_with_else_clause() - Parse: `with x <- f() do Ok(x) else Err(e) -> e`
- test_parse_with_in_block() - Parse: `{ let z = with x <- f() do Ok(x), z }`

**Expected Changes**:
- tests/test_parser.c (add 4 new tests)
- include/ast.h (add WithExpr struct, BindingVec for with bindings)
- lib/ast.c (add expr_with helper)
- lib/parser.c (add with expression parsing in parse_primary_internal)

**Success Criteria**:
- [x] All 4 new tests pass
- [x] No regression in existing 78 tests (78 → 82 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN → update ROADMAP)

**Key Design Considerations**:
- `with` is followed by one or more comma-separated bindings (`name <- expr`)
- `do` keyword separates bindings from success body
- `else` keyword is optional and introduces error handling patterns
- Syntax: `with <binding>, <binding> do <expr> [else <pattern> -> <expr>]`

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 4 failing tests for with expression parsing (74bfe5a)
2. GREEN phase: Implemented with expression AST and parser

**Tests Written**:
- test_parse_with_simple() - Parse: `with x <- f() do Ok(x)` ✓
- test_parse_with_multiple_bindings() - Parse: `with x <- f(), y <- g(x) do Ok(y)` ✓
- test_parse_with_else_clause() - Parse: `with x <- f() do Ok(x) else Err(e) -> e` ✓
- test_parse_with_in_block() - Parse: `{ let z = with x <- f() do Ok(x), z }` ✓

**Files Modified**:
- tests/test_parser.c (added 4 new tests)
- include/ast.h (added EXPR_WITH, WithBinding, WithBindingVec, WithExpr structs)
- lib/ast.c (added expr_with helper)
- lib/parser.c (added with expression parsing in parse_primary_internal)

**AST Changes:**
- Added `WithBinding` struct (name: String*, value: Expr*)
- Added `WithBindingVec` (vector of WithBinding)
- Added `WithExpr` struct (bindings: WithBindingVec*, body: Expr*, else_arms: MatchArmVec*)
- Added `EXPR_WITH` expression type
- Added `expr_with()` helper function

**Parser Changes:**
- Added `with` expression parsing in `parse_primary_internal()`, triggered by `TOKEN_WITH`
- Parses comma-separated bindings: `name <- expr, name <- expr`
- Consumes `do` keyword, then parses the success body expression
- Optionally parses `else` clause with pattern-matched arms (reuses MatchArm)
- Else clause supports constructor patterns like `Err(e)` by detecting `IDENT(` sequences
  and parsing them as literal patterns wrapping call expressions

**Key Design Decision:**
The `else` clause reuses the existing `MatchArmVec` type since its structure is identical
to match expression arms (pattern -> body). Constructor patterns like `Err(e)` are parsed
by detecting an identifier followed by `(` and constructing a call expression wrapped in
a PATTERN_LIT. This avoids adding a separate constructor pattern type to the AST.

Test Results:
```
Total:  82
Passed: 82
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28

✅ ACCEPTED - With expression parsing implementation

Verification Results:
- Tests: 82/82 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- TDD workflow followed correctly ✓
- Smart reuse of MatchArm infrastructure ✓

Success Criteria Met:
- [x] All 4 new tests pass
- [x] No regression (78 → 82 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

Code Review:
- AST changes (WithBinding, WithBindingVec, WithExpr): Clean design ✓
- expr_with() helper: Proper null handling for optional else clause ✓
- Parser implementation: Clear parsing logic with proper error messages ✓
- Constructor pattern handling (Err(e)): Creative solution using PATTERN_LIT + call expr ✓
- Reuses MatchArmVec for else clause: Excellent architectural decision ✓

Commits reviewed:
- 74bfe5a: Tests (RED phase) ✓
- b744b75: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
Completed 13 iterations with 82/82 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with comprehensive patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let with optional type annotations, return, expression statements)
- Result handling (← bind operator, **with expressions**)
- Function composition (|> pipe operator)
- Type annotations (simple, parameterized, function types)
- Function definitions (single-clause typed, multi-clause pattern-based)
- Pattern matching (literals, wildcards, identifier bindings)

**With expression** adds powerful chained error handling:
```fern
with
    x <- f(),
    y <- g(x)
do
    Ok(y)
else
    Err(e) -> handle(e)
```

This completes the Result handling story: single bind (`<-`) for inline error propagation,
and `with` expressions for multiple chained operations with centralized error handling.

Ready for next task.

---

## Iteration 14: Module and Import Declarations

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement module and import declaration parsing

### Completed Task

- [x] Implement module and import declaration parsing ✅ VERIFIED

**Tests Written**:
- test_parse_pub_function() - Parse: `pub fn add(x: Int, y: Int) -> Int: x + y` ✓
- test_parse_private_function() - Parse: `fn helper() -> Int: 42` (verify no pub) ✓
- test_parse_import_module() - Parse: `import math.geometry` ✓
- test_parse_import_items() - Parse: `import http.server.{cors, auth}` ✓
- test_parse_import_alias() - Parse: `import math.geometry as geo` ✓

**Files Modified**:
- tests/test_parser.c (added 5 new tests)
- include/token.h (added TOKEN_AS, TOKEN_MODULE keywords)
- lib/lexer.c (added keyword recognition for 'as' and 'module')
- lib/token.c (added token name strings)
- include/ast.h (added StringVec, STMT_IMPORT, ImportDecl, is_public to FunctionDef)
- lib/ast.c (updated stmt_fn with is_public param, added stmt_import helper)
- lib/parser.c (added import statement parsing, pub fn prefix handling)

**Success Criteria Met**:
- [x] All 5 new tests pass
- [x] No regression in existing 82 tests (82 → 87 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 5 failing tests for module and import declarations (2a1c611)
2. GREEN phase: Implemented parsing for pub keyword and import statements (307f2be)

**AST Changes:**
- Added `StringVec` (VEC_TYPE for String*)
- Added `STMT_IMPORT` statement type with `ImportDecl` struct (path, items, alias)
- Extended `FunctionDef` with `is_public` boolean field
- Updated `stmt_fn()` signature to include `is_public` parameter

**Lexer Changes:**
- Added TOKEN_AS keyword for `as` (import aliasing)
- Added TOKEN_MODULE keyword for `module` (future module declarations)

**Parser Changes:**
- Enhanced function parsing to check for optional `pub` keyword prefix
- Added import statement parsing in `parse_stmt()`, triggered by TOKEN_IMPORT
- Import parsing supports three forms:
  - `import path.to.module` (entire module)
  - `import path.to.module.{item1, item2}` (selective imports)
  - `import path.to.module as alias` (aliased import)
- Module path is parsed as dot-separated identifiers stored in StringVec

Test Results:
```
Total:  87
Passed: 87
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28

✅ ACCEPTED - Module and import declaration parsing implementation

Verification Results:
- Tests: 87/87 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Follows existing parser patterns ✓
- TDD workflow followed correctly ✓
- Comprehensive import syntax support ✓

Success Criteria Met:
- [x] All 5 new tests pass
- [x] No regression (82 → 87 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

Code Review:
- AST changes (StringVec, ImportDecl, is_public): Clean design ✓
- stmt_import() helper: Follows existing patterns ✓
- Parser implementation: Clear parsing logic for all import forms ✓
- pub keyword handling: Simple and correct ✓
- Excellent commit messages with full details ✓

Commits reviewed:
- 2a1c611: Tests (RED phase) ✓
- 307f2be: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
Completed 14 iterations with 87/87 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with comprehensive patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let with optional type annotations, return, expression statements)
- Result handling (← bind operator, with expressions)
- Function composition (|> pipe operator)
- Type annotations (simple, parameterized, function types)
- Function definitions (single-clause typed, multi-clause pattern-based, pub/private visibility)
- Pattern matching (literals, wildcards, identifier bindings)
- **NEW**: Module system (import declarations with selective/aliased imports)

This completes the core parser functionality defined in Milestone 2. The parser can now handle:
- All expression types from DESIGN.md
- All statement types (let, return, function definitions, imports)
- Full type annotation system
- Module visibility and imports

**Milestone 2 Status**: Core parsing is complete. Remaining items:
- Error recovery (enhancement for production use)
- Indentation tracking (for production-ready code)
- Pretty-print AST utility (debugging tool)

Ready for next milestone or refinement tasks.

---

## Iteration 15: Defer Statement Parsing

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement defer statement parsing for resource cleanup

### Completed Task

- [x] Implement defer statement parsing ✅ VERIFIED

**Tests Written**:
- test_parse_defer_simple() - Parse: `defer close(file)` ✓
- test_parse_defer_with_call() - Parse: `defer cleanup_resource(handle)` ✓
- test_parse_defer_in_block() - Parse: `{ file <- open(), defer close(file), read(file) }` ✓
- test_parse_defer_multiple() - Parse: `{ defer release1(r1), defer release2(r2), compute() }` ✓

**Files Modified**:
- tests/test_parser.c (added 4 new tests)
- include/ast.h (added STMT_DEFER, DeferStmt struct, defer_stmt in Stmt union)
- lib/ast.c (added stmt_defer helper)
- lib/parser.c (added defer statement parsing in parse_stmt, added TOKEN_DEFER to block statement detection)

**Success Criteria Met**:
- [x] All 4 new tests pass
- [x] No regression in existing 87 tests (87 → 91 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 4 failing tests for defer statement parsing (64f92d4)
2. GREEN phase: Implemented defer statement AST and parser (c7911f7)

**AST Changes:**
- Added `DeferStmt` struct (expr: Expr*)
- Added `STMT_DEFER` statement type
- Added `defer_stmt` field to Stmt union
- Added `stmt_defer()` helper function

**Parser Changes:**
- Added `defer` statement parsing in `parse_stmt()`, triggered by `TOKEN_DEFER`
- Parses syntax: `defer <expression>` — consumes the defer keyword, then parses the following expression
- Added `TOKEN_DEFER` to the block parser's statement detection condition alongside `TOKEN_LET` and `TOKEN_RETURN`, so defer statements are recognized inside block expressions

Test Results:
```
Total:  91
Passed: 91
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28

✅ ACCEPTED - Defer statement parsing implementation

Verification Results:
- Tests: 91/91 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Follows existing parser patterns ✓
- TDD workflow followed correctly ✓

Success Criteria Met:
- [x] All 4 new tests pass
- [x] No regression (87 → 91 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

Code Review:
- DeferStmt struct: Simple, clean design ✓
- stmt_defer() helper: Follows existing patterns ✓
- Parser implementation: Straightforward parsing of `defer <expr>` ✓
- Block statement detection: Correctly added TOKEN_DEFER to condition ✓
- Excellent commit messages with details ✓

Commits reviewed:
- 64f92d4: Tests (RED phase) ✓
- c7911f7: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
Completed 15 iterations with 91/91 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with comprehensive patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let with optional type annotations, return, expression statements, **defer**)
- Result handling (← bind operator, with expressions)
- Function composition (|> pipe operator)
- Type annotations (simple, parameterized, function types)
- Function definitions (single-clause typed, multi-clause pattern-based, pub/private visibility)
- Pattern matching (literals, wildcards, identifier bindings)
- Module system (import declarations with selective/aliased imports)
- **NEW**: Resource cleanup (defer statements for automatic cleanup)

This completes the defer statement feature from DESIGN.md. The parser now supports all major language constructs:
- All expression types
- All statement types
- Full type system
- Module system
- Resource management

**Milestone 2 Status**: Core parsing is essentially complete! Remaining optional enhancements:
- Error recovery (for better IDE support)
- Indentation tracking (for production-ready code)
- Pretty-print AST utility (debugging tool)

Ready for Milestone 3 (Type System) or additional parser refinements.

---

---

## Iteration 16: Lexer Enhancement - Float Literals

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅
**Task**: Implement float literal lexing and parsing

### Task Requirements

Implement lexing and parsing for float literals:
```fern
3.14
0.5
123.456
1.0
```

**Tests to Write** (TDD - RED phase first):
- test_lex_float_simple() - Lex: `3.14` → TOKEN_FLOAT ✓
- test_lex_float_leading_zero() - Lex: `0.5` → TOKEN_FLOAT ✓
- test_lex_float_trailing_zero() - Lex: `1.0` → TOKEN_FLOAT ✓
- test_parse_float_literal() - Parse: `3.14` → EXPR_FLOAT_LIT ✓
- test_parse_float_in_expr() - Parse: `x + 3.14` → binary expression with float ✓

**Expected Changes**:
- tests/test_lexer.c (add 3 new float lexer tests)
- tests/test_parser.c (add 2 new float parser tests)
- lib/lexer.c (enhance lex_number to detect and lex floats)
- lib/parser.c (add EXPR_FLOAT case in parse_primary_internal)
- include/ast.h (add expr_float_lit declaration)
- lib/ast.c (add expr_float_lit helper)

**Success Criteria**:
- [x] All 5 new tests pass
- [x] No regression in existing 91 tests (91 → 96 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN → update ROADMAP)

**Key Design Considerations**:
- Float literals must contain a decimal point: `3.14`, not `3`
- Both sides of decimal can be digits: `123.456`
- Leading zero allowed: `0.5`
- Trailing zero allowed: `1.0`
- No exponential notation for now (future enhancement)
- Lexer stores float text as String* in Token (same as other tokens)
- Parser converts text to double via strtod() and creates EXPR_FLOAT_LIT AST node

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 5 failing tests for float literal lexing and parsing (2832c3d)
2. GREEN phase: Implemented float lexing and parsing

**Tests Written**:
- test_lex_float_simple() - Lex: `3.14` → TOKEN_FLOAT ✓
- test_lex_float_leading_zero() - Lex: `0.5` → TOKEN_FLOAT ✓
- test_lex_float_trailing_zero() - Lex: `1.0` → TOKEN_FLOAT ✓
- test_parse_float_literal() - Parse: `3.14` → EXPR_FLOAT_LIT ✓
- test_parse_float_in_expr() - Parse: `x + 3.14` → binary with float ✓

**Files Modified**:
- tests/test_lexer.c (added 3 new float lexer tests)
- tests/test_parser.c (added 2 new float parser tests)
- lib/lexer.c (enhanced lex_number to detect decimal point and lex floats)
- lib/parser.c (added TOKEN_FLOAT case in parse_primary_internal using strtod)
- include/ast.h (added expr_float_lit declaration)
- lib/ast.c (added expr_float_lit helper function)

**Lexer Changes:**
Enhanced `lex_number()` to detect float literals. After consuming integer digits, checks for
a decimal point followed by at least one digit (`peek == '.' && isdigit(current[1])`). This
two-character lookahead prevents `42.method()` from being misread as a float. If a decimal
point with trailing digits is found, consumes the dot and fractional digits and returns
TOKEN_FLOAT instead of TOKEN_INT.

**Parser Changes:**
Added `TOKEN_FLOAT` handling in `parse_primary_internal()` right after the `TOKEN_INT` case.
Uses `strtod()` to convert the token text to a `double` value, then creates an EXPR_FLOAT_LIT
node via the new `expr_float_lit()` helper.

**AST Changes:**
- Added `expr_float_lit()` helper function (declaration in ast.h, implementation in ast.c)
- Uses existing `EXPR_FLOAT_LIT` type and `FloatLit` struct already defined in ast.h

Test Results:
```
Total:  96
Passed: 96
```

---

## Iteration 17: Type Definition Parsing

**Agent Turn**: CONTROLLER + IMPLEMENTER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement type definition parsing (sum types, record types, parameterized types)

### Completed Task

- [x] Implement type definition parsing ✅ VERIFIED

**Tests Written**:
- test_parse_type_def_simple() - Parse: `type Status: Active, Inactive` ✓
- test_parse_type_def_with_fields() - Parse: `type Shape: Circle(radius: Float), Rect(w: Int, h: Int)` ✓
- test_parse_type_def_parameterized() - Parse: `type Option(a): Some(a), None` ✓
- test_parse_type_def_record() - Parse: `type User: name: String, age: Int` ✓
- test_parse_type_def_pub() - Parse: `pub type Color: Red, Green, Blue` ✓

**Files Modified**:
- tests/test_parser.c (added 5 new tests)
- include/ast.h (added TypeField, TypeFieldVec, TypeVariant, TypeVariantVec, TypeDef, STMT_TYPE_DEF)
- lib/ast.c (added stmt_type_def helper)
- lib/parser.c (added type definition parsing, extended pub to accept type)

**Success Criteria Met**:
- [x] All 5 new tests pass
- [x] No regression in existing tests (96 → 101 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 5 failing tests for type definitions (dc0ffbf)
2. GREEN phase: Implemented type definition AST and parser (658fb3b)

**AST Changes:**
- Added `TypeField` struct (name: String*, type_ann: TypeExpr*) and `TypeFieldVec`
- Added `TypeVariant` struct (name: String*, fields: TypeFieldVec*) and `TypeVariantVec`
- Added `TypeDef` struct (name, is_public, type_params, variants, record_fields)
- Added `STMT_TYPE_DEF` statement type with `type_def` in Stmt union
- Added `stmt_type_def()` helper function

**Parser Changes:**
- Extended `pub` keyword handling to accept both `fn` and `type`
- Added type definition parsing in `parse_stmt()`, triggered by `TOKEN_TYPE`
- Uses heuristic to distinguish sum types from record types:
  - First identifier lowercase + followed by colon → record type
  - Otherwise → sum type with uppercase variant constructors
- Sum type variants support optional fields (named or positional)
- Type parameters parsed as comma-separated identifiers in parentheses

**Key Design Decision:**
Sum types vs record types are distinguished by checking the case of the first
member identifier and whether it's followed by a colon. Uppercase = variant
constructor (sum type), lowercase + colon = field (record type). This matches
Fern's convention from DESIGN.md where constructors are PascalCase and fields
are snake_case.

Test Results:
```
Total:  101
Passed: 101
```

---

## Iteration 18: Loop Constructs

**Agent Turn**: CONTROLLER + IMPLEMENTER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement loop constructs (for, while, loop, break, continue)

### Completed Task

- [x] Implement loop construct lexing and parsing ✅ VERIFIED

**Tests Written**:
- test_lex_loop_keywords() - Lex: for, while, loop, break, continue, in keywords ✓
- test_parse_for_loop() - Parse: `for item in items: process(item)` ✓
- test_parse_while_loop() - Parse: `while x < 10: process(x)` ✓
- test_parse_loop() - Parse: `loop: process()` ✓
- test_parse_break() - Parse: `break` (bare) ✓
- test_parse_break_with_value() - Parse: `break 42` (with value) ✓
- test_parse_continue() - Parse: `continue` ✓

**Files Modified**:
- tests/test_lexer.c (added 1 new test)
- tests/test_parser.c (added 6 new tests)
- include/token.h (added TOKEN_FOR, TOKEN_WHILE, TOKEN_LOOP, TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_IN)
- lib/token.c (added token name strings)
- lib/lexer.c (added keyword recognition for 6 new keywords)
- include/ast.h (added ForExpr, WhileExpr, LoopExpr, BreakStmt, EXPR_FOR/WHILE/LOOP, STMT_BREAK/CONTINUE)
- lib/ast.c (added expr_for, expr_while, expr_loop, stmt_break, stmt_continue helpers)
- lib/parser.c (added loop expression and break/continue statement parsing)

**Success Criteria Met**:
- [x] All 7 new tests pass
- [x] No regression in existing tests (101 → 108 tests, all passing)
- [x] No compiler warnings
- [x] Follows TDD workflow (RED → GREEN)

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 7 failing tests for loop constructs (6303edf)
2. GREEN phase: Implemented loop lexing and parsing (8dad7a8)

**Lexer Changes:**
- Added 6 new keyword tokens: for, while, loop, break, continue, in
- Added keyword recognition in identifier_type() for b/c/f/i/l/w cases

**AST Changes:**
- Added ForExpr (var_name, iterable, body), WhileExpr (condition, body), LoopExpr (body)
- Added BreakStmt (optional value) for break with value expressions
- Added EXPR_FOR, EXPR_WHILE, EXPR_LOOP expression types
- Added STMT_BREAK, STMT_CONTINUE statement types

**Parser Changes:**
- For/while/loop parsed as expressions in parse_primary_internal()
- Break/continue parsed as statements in parse_stmt()
- Added break/continue to block statement detection condition

Test Results:
```
Total:  108
Passed: 108
```

---

## Design Changes (2026-01-28)

The following design changes were made and affect implementation. See DECISIONS.md #11-15 for details.

### Lexer Changes Needed
- [ ] Add `TOKEN_QUESTION` for `?` operator (Result propagation)
- [x] `TOKEN_BIND` (`<-`) is kept but only valid inside `with` blocks
- [x] No `TOKEN_UNLESS` needed (keyword removed from design)
- [x] No `TOKEN_WHILE` or `TOKEN_LOOP` needed (constructs removed from design)

### Parser Changes Needed
- [ ] Parse `?` as postfix operator on expressions returning `Result`
- [ ] `<-` only valid inside `with` expressions (not standalone statements)
- [x] No `while` or `loop` parsing needed (removed from design)
- [x] No `unless` parsing needed (removed from design)
- [x] Named tuple syntax `(x: 10)` not supported - only positional tuples
- [ ] Record update uses `%{ record | field: value }` syntax

### Already Implemented (No Changes Needed)
- [x] `for` iteration over collections — keep as-is
- [x] `if` expressions — keep as-is
- [x] Match expressions — keep as-is
- [x] Block expressions — keep as-is
- [x] Positional tuples `(a, b)` — keep as-is

### Summary of Removed Features
These features were in earlier design drafts but are now explicitly NOT part of Fern:
- `unless` keyword — use `if not` instead
- `while` loops — use recursion or `list.fold`/`list.iterate`
- `loop` construct — use recursion
- Named tuples `(x: 10, y: 20)` — use `type` records instead
- Standalone `<-` operator — use `?` for propagation, `with`/`<-` for complex handling

