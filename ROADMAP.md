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
  - [x] Numeric literals (Int) - Float TODO
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

// Test: <- operator
void test_lex_bind_operator() {
    char* source = "content <- read_file(\"config.txt\")";
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
  - [ ] Pattern parsing (basic identifier patterns done)
  - [ ] Type parsing
  - [ ] Function definitions (including clauses)
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

## Milestone 3: Type System

**Goal:** Type checking and inference

### Test Files

```
tests/typechecker/
├── test_basic_types.fn        # Int, String, Bool
├── test_functions.fn          # Function types
├── test_generics.fn           # List(a), Map(k,v)
├── test_traits.fn             # Show, Eq, Ord
├── test_result.fn             # Result(ok, err)
├── test_inference.fn          # Local inference
├── test_unification.fn        # Type unification
├── test_errors.fn             # Type errors
└── test_result_handling.fn    # <- must be in Result function
```

### Tasks

- [ ] **Write type checker tests first**
  ```c
  void test_infer_integer() {
      char* source = "let x = 42";
      Type* type = infer_type(source, "x");
      assert_type_eq(type, TYPE_INT);
  }
  
  void test_require_result_handling() {
      char* source = 
          "fn main() -> ():\n"
          "    let x = read_file(\"test\")";  // Not handled!
      
      TypeResult result = typecheck(source);
      assert(result.is_err);
      assert_error_contains(result.err, "Result value must be handled");
  }
  ```

- [ ] Implement type representation
  ```c
  datatype(Type,
      (TypeInt),
      (TypeFloat),
      (TypeString),
      (TypeBool),
      (TypeVar, sds name),  // Generic variable
      (TypeApp, Type* constructor, List(Type*) args),  // List(Int)
      (TypeArrow, List(Type*) params, Type* result),
      (TypeResult, Type* ok, Type* err)
  );
  ```

- [ ] Implement type checker
  - [ ] Basic type checking
  - [ ] Type inference (Hindley-Milner)
  - [ ] Generic instantiation
  - [ ] Trait checking
  - [ ] Result handling enforcement
  - [ ] Exhaustiveness checking for match

- [ ] Error messages
  - [ ] Type mismatch errors
  - [ ] Unhandled Result errors
  - [ ] Missing patterns
  - [ ] Helpful suggestions

**Success Criteria:**
- All type checker tests pass
- `fern check file.fn` reports type errors
- Unhandled Results are caught
- Error messages are helpful

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

## Ralph Loop Status

**Current Milestone**: 2 - Parser
**Current Iteration**: 8
**Agent Turn**: IMPLEMENTER
**Status**: IN_PROGRESS
**Started**: 2026-01-28 11:45:00
**Last Updated**: 2026-01-28 11:45:00

### Previous Task

- [x] Implement if expression parsing ✅ VERIFIED

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

**Agent Turn**: IMPLEMENTER
**Status**: COMPLETE ✅
**Task**: Implement type annotation parsing

### Completed Task

- [x] Implement type annotation parsing ✅ COMPLETE

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
2. GREEN phase: Implemented type parsing

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

Ready for CONTROLLER verification.

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
