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

- [ ] **Write parser tests first**
  ```c
  void test_parse_function() {
      char* source = "fn add(a: Int, b: Int) -> Int:\n    a + b";
      Expr* ast = parse(source);
      assert_expr_type(ast, EXPR_FUNCTION);
      assert_string_eq(ast->fn.name, "add");
      assert_int_eq(ast->fn.params.len, 2);
      // ...
  }
  ```

- [ ] Define AST types (Datatype99)
  ```c
  datatype(Expr,
      (ExprInt, int64_t value),
      (ExprString, sds value),
      (ExprIdent, sds name),
      (ExprBinOp, Expr* left, BinOp op, Expr* right),
      (ExprCall, Expr* func, List(Expr*) args),
      (ExprPipe, Expr* value, Expr* func),
      (ExprMatch, Expr* value, List(MatchArm) arms),
      (ExprBind, sds name, Expr* value),  // <-
      (ExprWith, List(Binding) bindings, Expr* do_block, List(MatchArm) else_block),
      (ExprDefer, Expr* expr),
      (ExprFunction, sds name, List(Param) params, Type* return_type, Expr* body)
  );
  
  datatype(Type,
      (TypeIdent, sds name),
      (TypeGeneric, sds name, List(Type*) params),
      (TypeArrow, List(Type*) params, Type* return_type),
      (TypeResult, Type* ok, Type* err)
  );
  ```

- [ ] Implement parser.c
  - [ ] Expression parsing (precedence climbing)
  - [ ] Pattern parsing
  - [ ] Type parsing
  - [ ] Function definitions (including clauses)
  - [ ] Module declarations
  - [ ] Error recovery

- [ ] Parser utilities
  - [ ] Pretty-print AST
  - [ ] AST validation
  - [ ] Error messages with context

**Success Criteria:**
- All parser tests pass
- `fern parse file.fn` prints AST
- Error messages show parse issues
- Handles all syntax from DESIGN.md

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
