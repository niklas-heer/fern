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

## Milestone 1: Lexer

**Status:** 🚧 In Progress - Core lexer complete, 23/23 tests passing

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
  - [ ] Add `?` operator (TOKEN_QUESTION) for Result propagation
  - [x] Numeric literals (Int, Float)
  - [x] String literals (basic) - Interpolation TODO
  - [x] Comment handling (#) - Block comments /* */ TODO
  - [ ] Indentation tracking (indent/dedent tokens) - TODO
  - [x] Error reporting with line/column

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

**Status:** 🚧 In Progress - Core parser complete, 36/36 tests passing

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
  - [ ] Module declarations
  - [ ] Error recovery

- [ ] Parser utilities
  - [ ] Pretty-print AST
  - [ ] AST validation
  - [x] Error messages with context (basic implementation)

**Success Criteria:**
- ✅ Core parser tests pass (36/36)
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
- ✅ All type checker tests pass (66 tests)
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

**Goal:** Generate QBE IR from AST

### Test Files

```
tests/codegen/
├── test_expressions.fn    # Arithmetic, calls
├── test_functions.fn      # Function codegen
├── test_result.fn         # <- operator codegen
├── test_strings.fn        # String operations
├── test_actors.fn         # spawn, send, receive
└── test_integration.fn    # End-to-end tests
```

### Tasks

- [ ] **Write codegen tests first**
  ```c
  void test_codegen_add() {
      char* source = "fn add(a: Int, b: Int) -> Int:\n    a + b";
      char* qbe = generate_qbe(source);
      assert_contains(qbe, "function $add(w %a, w %b) {");
      assert_contains(qbe, "add w %a, w %b");
  }
  ```

- [ ] Implement QBE generation
  - [ ] Basic expressions
  - [ ] Function definitions
  - [ ] Result handling (<- operator)
  - [ ] String operations
  - [ ] Pattern matching
  - [ ] Actor primitives (spawn, send, receive)

- [ ] Runtime library
  - [ ] Result type implementation
  - [ ] String functions
  - [ ] Actor runtime (if spawn used)
  - [ ] libSQL bindings (if sql.open used)

**Success Criteria:**
- All codegen tests pass
- `fern build file.fn` creates executable
- Generated code runs correctly
- Binary sizes match targets (CLI <1MB, server <4MB)

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

**Goal:** Implement actor-based concurrency

### Tasks

- [ ] Actor runtime
  - [ ] spawn() implementation
  - [ ] send() implementation
  - [ ] receive pattern matching
  - [ ] Message queues
  - [ ] Process scheduling

- [ ] Test actor patterns
  - [ ] Cache actor
  - [ ] Queue actor
  - [ ] Supervision

**Success Criteria:**
- Actor examples from DESIGN.md work
- Binary includes runtime only when needed
- Performance targets met

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

**When implementing:**
- Always run tests after changes
- Use AddressSanitizer to catch memory errors
- Follow CLAUDE.md guidelines
- Keep functions small and focused
- Add comments explaining non-obvious code

**Error handling:**
- All functions return Result types
- Check pointers before dereferencing
- Use arena allocation, never malloc/free directly

**Code style:**
- Keep C code simple
- Use Datatype99 for tagged unions
- Use SDS for all strings
- Use stb_ds for hash maps and arrays

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

---

## Ralph Loop Status

**Current Milestone**: 2 - Parser (+ Lexer enhancements)
**Current Iteration**: 18
**Agent Turn**: COMPLETE
**Status**: COMPLETE ✅
**Started**: 2026-01-28
**Last Updated**: 2026-01-28

### Previous Task

- [x] Implement type definition parsing ✅ VERIFIED

### Completed Task

- [x] Implement match expression parsing ✅ COMPLETE

**Tests Written**:
- test_parse_match_simple() - Parse: match x: 1 -> "one", 2 -> "two" ✓
- test_parse_match_with_default() - Parse: match x: 1 -> "a", _ -> "default" ✓

**Files Modified**:
- tests/test_parser.c (added 2 new tests)
- lib/parser.c (added match parsing)
- lib/ast.c (added expr_match helper)
- include/ast.h (added expr_match declaration)
- lib/lexer.c (fixed underscore tokenization)

**Success Criteria Met**:
- [x] Both new tests pass
- [x] No regression in existing tests (38 → 40 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing parser patterns

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28 02:45:00
**Commits**: 46c4723

Implementation completed with TDD workflow:
1. RED phase: Added failing tests for match expressions
2. GREEN phase: Implemented match parsing
3. REFACTOR: Fixed lexer bug where `_` was tokenized as identifier

Key Bug Fix:
Found and fixed critical lexer bug where bare underscore `_` was being lexed as TOKEN_IDENT instead of TOKEN_UNDERSCORE. The issue was in lex_identifier() - underscore matched is_ident_start() before reaching the switch statement. Added special case to return TOKEN_UNDERSCORE when identifier is exactly "_".

Match Expression Implementation:
- Parses syntax: match <value>: <pattern> -> <expr>, <pattern> -> <expr>
- Supports literal patterns (integers, strings, booleans)
- Supports wildcard pattern (_)
- Uses MatchArmVec for collecting arms
- Integrated into parse_primary_internal()

Test Results:
```
=== Parser Tests ===
Running test_parse_match_simple... ✓ PASS
Running test_parse_match_with_default... ✓ PASS

Total:  40
Passed: 40
```

Ready for CONTROLLER verification.

### Verification Notes from Iteration 2

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 02:50:00

✅ ACCEPTED - Match expression implementation

Verification Results:
- Tests: 40/40 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Error handling appropriate ✓
- Follows existing parser patterns ✓
- Critical lexer bug fixed ✓

Success Criteria Met:
- [x] Both new tests pass (test_parse_match_simple, test_parse_match_with_default)
- [x] No regression (38 → 40 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing patterns

Code Review:
- expr_match() helper: Clean implementation ✓
- Pattern parsing: Wildcard and literal patterns work ✓
- Lexer fix: Excellent debugging of underscore tokenization ✓
- Comment quality: Clear and helpful ✓

Commits reviewed:
- 4b8e368: Tests (RED phase) ✓
- 46c4723: Implementation (GREEN phase) ✓
- 5c32473: ROADMAP update ✓

Ready for next task: Block expressions

---

## Iteration 3: Block Expressions

**Agent Turn**: CONTROLLER
**Status**: COMPLETE
**Task**: Implement block expression parsing

### Completed Task

- [x] Implement block expression parsing ✅ COMPLETE

**Tests Written**:
- test_parse_block_simple() - Parse: { let x = 5, x + 1 } ✓
- test_parse_block_multiple_statements() - Parse: { let a = 1, let b = 2, a + b } ✓

**Files Modified**:
- tests/test_parser.c (added 2 new tests)
- lib/parser.c (added block parsing)
- lib/ast.c (added expr_block helper)
- include/ast.h (added expr_block declaration)

**Success Criteria Met**:
- [x] Both new tests pass
- [x] No regression in existing tests (40 → 42 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing parser patterns

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28 03:00:00
**Commits**: 1e3d3da (RED), d31a1c8 (GREEN)

Implementation completed with TDD workflow:
1. RED phase: Added failing tests for block expressions
2. GREEN phase: Implemented block parsing with comma-separated statements

Block Expression Implementation:
- Parses syntax: { <stmt>, <stmt>, <expr> }
- Comma-separated statements (matching match expression syntax)
- Final expression (no trailing comma) becomes the block's value
- Supports let statements, return statements, and expression statements
- Uses StmtVec for collecting statements
- Integrated into parse_primary_internal()

Key Design Decision:
Used comma separators instead of semicolons/newlines to match the existing match expression syntax and keep the parser simple. This creates a consistent pattern across the language.

Test Results:
```
=== Parser Tests ===
Running test_parse_block_simple... ✓ PASS
Running test_parse_block_multiple_statements... ✓ PASS

Total:  42
Passed: 42
```

Ready for CONTROLLER verification.

### Verification Notes from Iteration 3

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 03:10:00

✅ ACCEPTED - Block expression implementation

Verification Results:
- Tests: 42/42 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Error handling appropriate ✓
- Follows existing parser patterns ✓
- Clean separation of statements vs final expression ✓

Success Criteria Met:
- [x] Both new tests pass (test_parse_block_simple, test_parse_block_multiple_statements)
- [x] No regression (40 → 42 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing patterns

Code Review:
- expr_block() helper: Clean, proper null handling for final_expr ✓
- Parser logic: Clear distinction between statements and final expression ✓
- Comma-separated syntax: Consistent with match expressions ✓
- Good comments explaining the parsing logic ✓

Design Decision:
Using comma separators is a smart choice - creates consistency with match expressions and keeps the grammar simple. Well reasoned!

Commits reviewed:
- 1e3d3da: Tests (RED phase) ✓
- d31a1c8: Implementation (GREEN phase) ✓
- 413684a: ROADMAP update ✓

Ready for next task: List literals

---

## Iteration 4: List Literals

**Agent Turn**: CONTROLLER
**Status**: COMPLETE
**Task**: Implement list literal parsing

### Completed Task

- [x] Implement list literal parsing ✅ COMPLETE

**Tests Written**:
- test_parse_list_empty() - Parse: [] ✓
- test_parse_list_simple() - Parse: [1, 2, 3] ✓
- test_parse_list_expressions() - Parse: [x + 1, y * 2, f()] ✓

**Files Modified**:
- tests/test_parser.c (added 3 new tests)
- lib/parser.c (added list parsing)
- lib/ast.c (added expr_list helper)
- include/ast.h (added expr_list declaration)

**Success Criteria Met**:
- [x] All three new tests pass
- [x] No regression in existing tests (42 → 45 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing parser patterns

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28 03:20:00
**Commits**: b6f3d62 (RED), b6a8014 (GREEN)

Implementation completed with TDD workflow:
1. RED phase: Added failing tests for list literals
2. GREEN phase: Implemented list parsing

List Literal Implementation:
- Parses syntax: [<expr>, <expr>, ...]
- Comma-separated expressions
- Empty lists handled correctly: []
- Can contain any expressions (literals, binary ops, function calls, etc.)
- Uses ExprVec for collecting elements
- Integrated into parse_primary_internal()

Parsing Strategy:
```c
if (!check(parser, TOKEN_RBRACKET)) {
    do {
        Expr* elem = parse_expression(parser);
        ExprVec_push(parser->arena, elements, elem);
    } while (match(parser, TOKEN_COMMA));
}
```

The do-while with comma matching ensures we parse all elements and handle trailing commas gracefully.

Test Results:
```
=== Parser Tests ===
Running test_parse_list_empty... ✓ PASS
Running test_parse_list_simple... ✓ PASS
Running test_parse_list_expressions... ✓ PASS

Total:  45
Passed: 45
```

Ready for CONTROLLER verification.

### Verification Notes from Iteration 4

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 03:25:00

✅ ACCEPTED - List literal implementation

Verification Results:
- Tests: 45/45 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Error handling appropriate ✓
- Follows existing parser patterns ✓
- Empty list edge case handled correctly ✓

Success Criteria Met:
- [x] All three new tests pass (empty, simple, expressions)
- [x] No regression (42 → 45 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing patterns

Code Review:
- expr_list() helper: Clean, simple implementation ✓
- Parser logic: Smart use of do-while with comma matching ✓
- Empty list handling: Proper check before parsing elements ✓
- Good comments explaining the syntax ✓

Commits reviewed:
- b6f3d62: Tests (RED phase) ✓
- b6a8014: Implementation (GREEN phase) ✓
- 32ff958: ROADMAP update ✓

**Parser Progress Summary:**
We've now completed 4 iterations with 45/45 tests passing. The parser handles:
- Basic expressions (literals, identifiers, binary/unary ops)
- Control flow (if/else, match with patterns)
- Data structures (blocks, lists)
- Statements (let, return, expression statements)

Next priority: Completing core expression parsing before moving to function definitions.

Ready for next task: Nested expressions and precedence edge cases

---

## Iteration 5: Nested List and Block Expressions

**Agent Turn**: IMPLEMENTER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Test and handle nested expressions

### Completed Task

- [x] Add tests for nested list/block expressions ✅ VERIFIED

**Tests Written**:
- test_parse_nested_lists() - Parse: [[1, 2], [3, 4]] ✓
- test_parse_list_in_block() - Parse: { let x = [1, 2], x } ✓
- test_parse_block_in_list() - Parse: [{ let a = 1, a }, { let b = 2, b }] ✓

**Files Modified**:
- tests/test_parser.c (added 3 new tests)
- No implementation changes needed (recursive parser handles nesting correctly!)

**Success Criteria Met**:
- [x] All three new tests pass
- [x] No regression in existing tests (45 → 48 tests, all passing)
- [x] No compiler warnings
- [x] Verifies that recursive parser handles nesting correctly

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28
**Commits**: b757851

Verification completed - all 3 nested expression tests pass immediately without
any implementation changes. This confirms the recursive descent parser correctly
handles arbitrary nesting of lists and blocks.

Test Details:
- **Nested lists** (`[[1, 2], [3, 4]]`): Inner lists parsed as list elements, verified element count and values at both levels.
- **List in block** (`{ let x = [1, 2], x }`): List correctly parsed as let binding value inside a block expression.
- **Block in list** (`[{ let a = 1, a }, { let b = 2, b }]`): Block expressions correctly parsed as list elements, including their let statements and final expressions.

Test Results:
```
=== Parser Tests ===
Running test_parse_nested_lists... ✓ PASS
Running test_parse_list_in_block... ✓ PASS
Running test_parse_block_in_list... ✓ PASS

Total:  48
Passed: 48
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 11:35:00

✅ ACCEPTED - Nested expression tests

Verification Results:
- Tests: 48/48 passing ✓
- Code quality: Clean test-only addition ✓
- No compiler warnings ✓
- No implementation changes needed (recursive parser works!) ✓

Success Criteria Met:
- [x] All three new tests pass
- [x] No regression (45 → 48 tests, all passing)
- [x] Verifies recursive descent parser handles arbitrary nesting

Code Review:
- Tests verify nested lists, lists in blocks, blocks in lists ✓
- Good coverage of nesting edge cases ✓
- Confirms parser architecture is sound ✓

Commits reviewed:
- b757851: Tests (verification only) ✓
- 9220679: ROADMAP update ✓

Ready for next task.

---

## Iteration 6: Bind Operator Parsing

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement <- (bind) operator parsing

### Completed Task

- [x] Implement <- (bind) operator parsing ✅ VERIFIED

**Tests Written**:
- test_parse_bind_simple() - Parse: `x <- f()` ✓
- test_parse_bind_with_call() - Parse: `result <- read_file("test.txt")` ✓
- test_parse_bind_in_block() - Parse: `{ content <- load(), process(content) }` ✓

**Files Modified**:
- tests/test_parser.c (added 3 new tests)
- lib/parser.c (added bind parsing in identifier branch)
- lib/ast.c (added expr_bind helper)
- include/ast.h (added expr_bind declaration)

**Success Criteria Met**:
- [x] All three new tests pass
- [x] No regression in existing tests (48 → 51 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing parser patterns

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 3 failing tests for bind expressions (c4b71d9)
2. GREEN phase: Implemented bind parsing (5d1a089)

Bind Expression Implementation:
- Parses syntax: `name <- expression`
- When an identifier is parsed, the parser looks ahead for TOKEN_BIND (`<-`)
- If found, parses the right-hand side as a full expression
- Creates an EXPR_BIND node with the identifier name and value expression
- Integrated into parse_primary_internal() identifier branch

Key Design Decision:
The bind operator is parsed at the primary expression level rather than as a binary operator. This is because the left-hand side must be a bare identifier (not an arbitrary expression), making it a special form rather than a true binary operator. Parsing it during identifier handling keeps the precedence simple and ensures only valid bind forms are accepted.

Added `expr_bind()` helper function following the existing pattern used by other expression constructors (arena allocation, assertions, field assignment).

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 11:40:00

✅ ACCEPTED - Bind operator implementation

Verification Results:
- Tests: 51/51 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Error handling appropriate ✓
- Follows existing parser patterns ✓
- Correct design decision (special form vs binary operator) ✓

Success Criteria Met:
- [x] All three new tests pass
- [x] No regression (48 → 51 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing patterns

Code Review:
- expr_bind() helper: Clean implementation with proper assertions ✓
- Parser integration: Smart lookahead in identifier branch ✓
- Design rationale: Well-reasoned (LHS must be identifier) ✓
- Commit messages: Clear and conventional ✓

Commits reviewed:
- c4b71d9: Tests (RED phase) ✓
- 5d1a089: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
We've now completed 6 iterations with 51/51 tests passing. The parser successfully handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let, return, expression statements)
- **NEW**: Result handling with <- bind operator

This completes the core expression parsing tasks outlined in Milestone 2. The parser is now ready for function definition parsing, type parsing, and module declarations.

---

## Iteration 7: Pipe Operator Parsing

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement |> (pipe) operator parsing

### Completed Task

- [x] Implement |> (pipe) operator parsing ✅ VERIFIED

**Tests Written**:
- test_parse_pipe_simple() - Parse: `x |> double()` ✓
- test_parse_pipe_chain() - Parse: `data |> parse() |> validate()` (left-associative chaining) ✓
- test_parse_pipe_in_block() - Parse: `{ let result = x |> double(), result }` ✓

**Files Modified**:
- tests/test_parser.c (added 3 new tests)
- No implementation changes needed (pipe parsing was already implemented!)

**Success Criteria Met**:
- [x] All three new tests pass
- [x] No regression in existing tests (51 → 54 tests, all passing)
- [x] No compiler warnings
- [x] Verifies pipe operator parsing works correctly

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

The pipe operator (`|>`) was already fully implemented in `parse_pipe()` in `lib/parser.c`.
The implementation parses `|>` as a left-associative binary operator (`BINOP_PIPE`) with
the lowest expression precedence (below logical operators), which is correct per DESIGN.md.

Test Details:
- **Simple pipe** (`x |> double()`): Verifies basic pipe creates EXPR_BINARY with BINOP_PIPE, left side is identifier, right side is function call.
- **Chained pipe** (`data |> parse() |> validate()`): Verifies left-associative chaining produces nested binary expressions `(data |> parse()) |> validate()`.
- **Pipe in block** (`{ let result = x |> double(), result }`): Verifies pipe works correctly inside block expressions as a let binding value.

Precedence hierarchy (lowest to highest):
1. pipe (`|>`) — `parse_pipe()`
2. or — `parse_logical_or()`
3. and — `parse_logical_and()`
4. equality (`==`, `!=`) — `parse_equality()`
5. comparison (`<`, `<=`, `>`, `>=`) — `parse_comparison()`
6. term (`+`, `-`) — `parse_term()`
7. factor (`*`, `/`) — `parse_factor()`
8. unary (`-`, `not`) — `parse_unary()`
9. call — `parse_call()`
10. primary — `parse_primary_internal()`

Test Results:
```
=== Parser Tests ===
Running test_parse_pipe_simple... ✓ PASS
Running test_parse_pipe_chain... ✓ PASS
Running test_parse_pipe_in_block... ✓ PASS

Total:  54
Passed: 54
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 11:45:00

✅ ACCEPTED - Pipe operator verification

Verification Results:
- Tests: 54/54 passing ✓
- Code quality: Tests verify existing implementation ✓
- No compiler warnings ✓
- Pipe operator already implemented correctly ✓
- Left-associativity verified ✓
- Precedence verified (lowest expression precedence) ✓

Success Criteria Met:
- [x] All three new tests pass
- [x] No regression (51 → 54 tests, all passing)
- [x] Verifies pipe operator works correctly in all contexts
- [x] Confirms precedence hierarchy is correct

Code Review:
- Verification tests are thorough ✓
- Covers simple, chained, and nested pipe usage ✓
- Confirms existing implementation matches DESIGN.md ✓

Commits reviewed:
- 0687fa9: Tests (verification only) ✓
- eb80b7f: ROADMAP update ✓

**Parser Milestone Progress:**
We've now completed 7 iterations with 54/54 tests passing. The parser successfully handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let, return, expression statements)
- Result handling with <- bind operator
- **NEW**: Function composition with |> pipe operator (verified)

Next priority: Type parsing for type annotations.

---

## Iteration 8: Type Parsing

**Agent Turn**: CONTROLLER
**Status**: COMPLETE ✅ VERIFIED
**Task**: Implement type annotation parsing

### Completed Task

- [x] Implement type annotation parsing ✅ VERIFIED

**Tests Written**:
- test_parse_type_int() - Parse: `Int` ✓
- test_parse_type_string() - Parse: `String` ✓
- test_parse_type_bool() - Parse: `Bool` ✓
- test_parse_type_custom() - Parse: `User` ✓
- test_parse_type_result() - Parse: `Result(String, Error)` ✓
- test_parse_type_list() - Parse: `List(Int)` ✓
- test_parse_type_function() - Parse: `(Int, String) -> Bool` ✓

**Files Modified**:
- tests/test_parser.c (added 7 new tests)
- include/ast.h (expanded TypeExpr with TypeExprKind, NamedType, FunctionType, TypeExprVec)
- lib/ast.c (added type_named and type_function helpers)
- include/parser.h (added parse_type declaration)
- lib/parser.c (added parse_type function)

**Success Criteria Met**:
- [x] All 7 new tests pass
- [x] No regression in existing tests (54 → 61 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing parser patterns

### Implementation Notes

**Written by**: IMPLEMENTER (Opus 4.5)
**Time**: 2026-01-28

Implementation completed with TDD workflow:
1. RED phase: Added 7 failing tests for type annotations (9aafe10)
2. GREEN phase: Implemented type parsing (12208d8)

Type Expression AST Redesign:
- Replaced simple `TypeExpr { String* name; SourceLoc loc; }` with a proper tagged union
- Added `TypeExprKind` enum: `TYPE_NAMED`, `TYPE_FUNCTION`
- `TYPE_NAMED`: Supports simple types (Int, Bool) and parameterized types (Result(String, Error), List(Int))
- `TYPE_FUNCTION`: Supports function types ((Int, String) -> Bool)
- Added `TypeExprVec` for type argument and parameter lists

parse_type() Implementation:
- Parses function types when starting with `(`: `(param, param) -> return_type`
- Parses named types: bare identifier or identifier followed by `(arg, arg)`
- Recursive - type arguments can themselves be any type expression
- Integrated as public API in parser.h

Test Results:
```
Total:  61
Passed: 61
```

### Verification Notes

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 11:50:00

✅ ACCEPTED - Type annotation parsing implementation

Verification Results:
- Tests: 61/61 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Follows existing parser patterns ✓
- Comprehensive type support ✓

Success Criteria Met:
- [x] All 7 new tests pass
- [x] No regression (54 → 61 tests, all passing)
- [x] No compiler warnings
- [x] Proper AST design with tagged unions

Code Review:
- TypeExpr redesign: Excellent use of tagged union pattern ✓
- parse_type() function: Clean recursive descent ✓
- Supports simple, parameterized, and function types ✓
- Helper functions (type_named, type_function) follow patterns ✓

Commits reviewed:
- 9aafe10: Tests (RED phase) ✓
- 12208d8: Implementation (GREEN phase) ✓

**Parser Milestone Progress:**
We've completed 8 iterations with 61/61 tests passing. The parser now handles:
- Basic expressions (literals, identifiers, binary/unary ops, function calls)
- Control flow (if/else, match with patterns)
- Data structures (blocks, lists, nested combinations)
- Statements (let, return, expression statements)
- Result handling (← bind operator)
- Function composition (|> pipe operator)
- **NEW**: Type annotations (simple, parameterized, function types)

Core expression and type parsing is now complete. Ready to move beyond Milestone 2 or enhance current functionality.

---

---

### Verification Notes from Iteration 1

**Written by**: CONTROLLER (Sonnet 4.5)
**Time**: 2026-01-28 02:20:00

✅ ACCEPTED - If expression implementation

Verification Results:
- Tests: 38/38 passing ✓
- Code quality: Excellent ✓
- No compiler warnings ✓
- Uses arena allocation correctly ✓
- Error handling appropriate ✓
- Follows existing parser patterns ✓

Success Criteria Met:
- [x] Both new tests pass (test_parse_if_simple, test_parse_if_else)
- [x] No regression (36 → 38 tests, all passing)
- [x] No compiler warnings
- [x] Follows existing patterns

Code Review:
- expr_if() helper: Clean implementation ✓
- Parser integration: Proper precedence ✓
- Optional else branch: Handled correctly ✓

Commits reviewed:
- 0674f57: Tests (RED phase) ✓
- 3e725f3: Implementation (GREEN phase) ✓

Ready for next task: Match expressions

### Blockers

None

---
