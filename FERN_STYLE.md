# Fern Style Guide

*Inspired by [TigerBeetle's TIGER_STYLE](https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md)*

This document defines Fern's engineering philosophy and coding standards. These rules are designed to:
1. Maximize code safety and correctness
2. Make AI-assisted development more effective
3. Catch bugs early through assertions and fuzzing

**Priority order:** Safety > Performance > Developer Experience

---

## Assertions

### Assertion Density

**Minimum 2 assertions per function.** Functions must not operate blindly on unchecked data.

```c
// ✅ Good: Multiple assertions
Expr* parse_binary_op(Parser* p, Expr* left, TokenType op) {
    assert(p != NULL);
    assert(left != NULL);
    assert(is_binary_op(op));

    Expr* right = parse_expr(p);
    assert(right != NULL);  // Or handle as error

    Expr* result = expr_binop(p->arena, left, op, right);
    assert(result != NULL);
    assert(result->type == EXPR_BINOP);

    return result;
}

// ❌ Bad: No assertions
Expr* parse_binary_op(Parser* p, Expr* left, TokenType op) {
    Expr* right = parse_expr(p);
    return expr_binop(p->arena, left, op, right);
}
```

### Pair Assertions

**Assert the same property in two different code paths.** Bugs hide at boundaries.

```c
// Assert before writing
void ast_serialize(FILE* out, Expr* expr) {
    assert(expr != NULL);
    assert(expr_is_valid(expr));  // Validate before write
    // ... write to file
}

// Assert after reading
Expr* ast_deserialize(Arena* arena, FILE* in) {
    Expr* expr = /* ... read from file */;
    assert(expr != NULL);
    assert(expr_is_valid(expr));  // Validate after read
    return expr;
}
```

### Assert Positive AND Negative Space

Assert what you expect AND what you don't expect. The boundary between valid and invalid is where bugs live.

```c
// ✅ Good: Both spaces
Token lexer_next(Lexer* lex) {
    assert(lex != NULL);
    assert(lex->pos <= lex->len);  // Positive: position is valid
    assert(lex->pos >= 0);          // Negative: not before start

    Token tok = /* ... */;

    assert(tok.type != TOKEN_INVALID || tok.type == TOKEN_ERROR);
    assert(tok.line > 0);           // Positive: valid line
    assert(tok.col >= 0);           // Positive: valid column

    return tok;
}
```

### Split Compound Assertions

Prefer `assert(a); assert(b);` over `assert(a && b);` — you'll know which failed.

```c
// ✅ Good: Split assertions
assert(left != NULL);
assert(right != NULL);
assert(left->type == right->type);

// ❌ Bad: Compound assertion
assert(left != NULL && right != NULL && left->type == right->type);
```

### Compile-Time Assertions

Validate constants and type sizes at compile time.

```c
// Validate type sizes
_Static_assert(sizeof(Token) <= 64, "Token too large for cache line");
_Static_assert(sizeof(Expr) <= 128, "Expr struct exceeds size budget");

// Validate enum ranges
_Static_assert(TOKEN_COUNT < 256, "Too many token types for u8");
_Static_assert(EXPR_COUNT < 256, "Too many expr types for u8");

// Validate relationships
_Static_assert(ARENA_BLOCK_SIZE >= sizeof(Expr), "Arena block too small");
```

---

## Function Design

### 70-Line Limit

**Hard limit: 70 lines per function.** If you can't see the whole function without scrolling, split it.

This isn't arbitrary — it's a physical constraint that forces good design:
- Easier for humans to review
- Easier for AI to generate correctly
- Forces decomposition into testable units

### Centralize Control Flow

Parent functions handle branching; leaf functions handle computation.

```c
// ✅ Good: Parent handles control flow
Stmt* parse_stmt(Parser* p) {
    switch (peek(p).type) {
        case TOKEN_LET:    return parse_let_stmt(p);
        case TOKEN_FN:     return parse_fn_stmt(p);
        case TOKEN_IF:     return parse_if_stmt(p);
        case TOKEN_MATCH:  return parse_match_stmt(p);
        default:           return parse_expr_stmt(p);
    }
}

// Leaf functions are focused
Stmt* parse_let_stmt(Parser* p) {
    assert(peek(p).type == TOKEN_LET);
    // ... focused parsing logic
}
```

### Keep Leaf Functions Pure

Leaf functions should not modify global state. Pass state explicitly.

```c
// ✅ Good: Explicit state
Expr* parse_expr(Parser* p, Arena* arena, int precedence);

// ❌ Bad: Hidden global state
Expr* parse_expr(int precedence);  // Uses global parser
```

---

## Naming

### Use snake_case

Functions, variables, files, and constants use `snake_case`.

```c
// ✅ Good
Token lexer_next_token(Lexer* lex);
int token_count;
arena_alloc();

// ❌ Bad
Token lexerNextToken(Lexer* lex);
int tokenCount;
arenaAlloc();
```

### Units in Names

Include units, sorted by descending significance: `timeout_ms_max` not `max_timeout_ms`.

```c
// ✅ Good: Units clear and sorted
int timeout_ms_max = 5000;
int buffer_size_bytes = 4096;
int retry_count_max = 3;

// ❌ Bad: Units unclear or unsorted
int max_timeout = 5000;      // ms? seconds?
int buf_sz = 4096;           // bytes? elements?
int max_retries = 3;
```

### Prefix Helper Functions

Helper functions are prefixed with caller name.

```c
// ✅ Good: Clear relationship
void parse_match(Parser* p);
void parse_match_arm(Parser* p);
void parse_match_pattern(Parser* p);

// Callbacks end with _callback
void read_file_callback(void* ctx, int result);
```

---

## Memory Management

### Arena Allocation Only

**Never use malloc/free directly.** All allocations go through arenas.

```c
// ✅ Good: Arena allocation
Expr* expr = arena_alloc(arena, sizeof(Expr));

// ❌ Bad: Direct malloc
Expr* expr = malloc(sizeof(Expr));  // NEVER
free(expr);                          // NEVER
```

### Group Allocation and Deallocation

Visually group resource acquisition and release.

```c
// ✅ Good: Grouped with blank lines
Arena* ast_arena = arena_create(ARENA_SIZE);
Arena* string_arena = arena_create(ARENA_SIZE);

// ... use arenas ...

arena_destroy(string_arena);
arena_destroy(ast_arena);
```

### Static Allocation at Startup

Allocate everything at startup. No allocation during normal operation.

```c
// ✅ Good: Allocate once at startup
typedef struct {
    Arena ast_arena;
    Arena string_arena;
    Token tokens[MAX_TOKENS];
    char error_buffer[ERROR_BUFFER_SIZE];
} CompilerState;

CompilerState* compiler_init(void) {
    CompilerState* state = /* allocate once */;
    arena_init(&state->ast_arena, AST_ARENA_SIZE);
    arena_init(&state->string_arena, STRING_ARENA_SIZE);
    return state;
}
```

---

## Limits and Bounds

### Put Limits on Everything

Every loop, buffer, and queue has an explicit upper bound.

```c
// ✅ Good: Explicit limits
#define MAX_TOKENS 10000
#define MAX_NESTED_DEPTH 256
#define MAX_FUNCTION_PARAMS 255
#define MAX_ERROR_LENGTH 1024

for (int i = 0; i < arrlen(items) && i < MAX_ITEMS; i++) {
    // ...
}

// ❌ Bad: Unbounded
while (has_more_tokens()) {  // Could run forever
    // ...
}
```

### Document Limits

Limits are documented with rationale.

```c
// Max 255 function parameters (u8 index)
// Rationale: More than this is a code smell; u8 saves space in bytecode
#define MAX_FUNCTION_PARAMS 255

// Max 64KB source file
// Rationale: Larger files should be split; keeps memory predictable
#define MAX_SOURCE_SIZE (64 * 1024)
```

---

## Error Handling

### All Errors Must Be Handled

Use Result types. Never ignore errors.

```c
// ✅ Good: Result type
typedef struct {
    bool ok;
    union {
        Expr* expr;
        ParseError error;
    };
} ParseResult;

ParseResult parse_expr(Parser* p) {
    if (/* error */) {
        return (ParseResult){ .ok = false, .error = /* ... */ };
    }
    return (ParseResult){ .ok = true, .expr = expr };
}

// Caller MUST check
ParseResult result = parse_expr(p);
if (!result.ok) {
    handle_error(result.error);
    return;
}
use_expr(result.expr);
```

### Assertions vs Error Handling

- **Assertions**: Programmer errors (bugs). Should never happen. Crash is correct.
- **Error handling**: Operating errors (bad input). Expected. Handle gracefully.

```c
// Assertion: This should NEVER be null (programmer bug)
assert(parser != NULL);

// Error handling: User might give us bad input (expected)
if (source == NULL) {
    return (ParseResult){ .ok = false, .error = ERROR_NULL_SOURCE };
}
```

---

## Comments and Documentation

### Comments Are Sentences

Capital letter, full stop, space after `//`.

```c
// ✅ Good: Proper sentences
// Parse the left-hand side of a binary expression.
// This handles operator precedence via Pratt parsing.

// ❌ Bad: Fragments
// parse lhs
// pratt parsing stuff
```

### Motivate Decisions

Explain WHY, not just WHAT.

```c
// ✅ Good: Explains why
// Use a hash map instead of linear search because we expect
// 1000+ symbols in typical programs. O(1) vs O(n) matters here.
SymbolTable* symbols = hashmap_create();

// ❌ Bad: Just says what
// Create symbol table
SymbolTable* symbols = hashmap_create();
```

### End-of-Line Comments

Short phrases without punctuation are OK for end-of-line comments.

```c
int count = 0;       // reset for next iteration
Token tok = next();  // consume the opening paren
```

### Doc Comments (API Documentation)

Use `/** */` doc comments for **all functions** (not just public ones).
These are checked automatically by `make style`.

**Format:**
```c
/**
 * Short one-line description (or use @brief).
 *
 * Longer description with more details. Can span multiple lines.
 * Use markdown for formatting.
 *
 * @param name Description of parameter
 * @param other Another parameter
 * @return What the function returns
 *
 * @example
 *     Lexer* lex = lexer_new(arena, "let x = 42");
 *     Token tok = lexer_next(lex);
 *
 * @see related_function
 * @note Important notes or warnings
 */
```

**Required (checked by `make style`):**
- **Description** - First line of comment OR `@brief` tag
- `@param` - For each parameter (rule: `doc-params`)
- `@return` - For non-void functions (rule: `doc-return`)

**Optional tags:**
- `@example` - Code example (will be syntax highlighted)
- `@see` - Cross-reference to related functions
- `@note` - Important information
- `@warning` - Danger/caution notes

**Example:**
```c
/**
 * @brief Create a new lexer for tokenizing Fern source code.
 *
 * The lexer uses arena allocation for all internal data structures.
 * Call lexer_next() repeatedly to get tokens until TOKEN_EOF.
 *
 * @param arena Memory arena for allocations (must not be NULL)
 * @param source Source code string to tokenize (must not be NULL)
 * @return New Lexer instance, never NULL
 *
 * @example
 *     Arena* arena = arena_create(4096);
 *     Lexer* lex = lexer_new(arena, "fn main(): print(\"hello\")");
 *     while (!lexer_is_eof(lex)) {
 *         Token tok = lexer_next(lex);
 *         printf("%s\n", token_type_name(tok.type));
 *     }
 *
 * @see lexer_next, lexer_peek, lexer_is_eof
 */
Lexer* lexer_new(Arena* arena, const char* source);
```

**For structs:**
```c
/**
 * @brief Represents a source code location.
 *
 * Used for error messages and debugging. All AST nodes
 * carry a SourceLoc to enable precise error reporting.
 */
typedef struct {
    String* file;   /**< Source file path */
    int line;       /**< Line number (1-indexed) */
    int column;     /**< Column number (0-indexed) */
} SourceLoc;
```

**For enums:**
```c
/**
 * @brief Token types produced by the lexer.
 */
typedef enum {
    TOKEN_INT,      /**< Integer literal: 42, 0xFF */
    TOKEN_FLOAT,    /**< Float literal: 3.14, 1e-10 */
    TOKEN_STRING,   /**< String literal: "hello" */
    TOKEN_IDENT,    /**< Identifier: foo, myVar */
    TOKEN_EOF,      /**< End of file */
} TokenType;
```

---

## Testing

### Test Exhaustively

Test valid data, invalid data, and the boundary between them.

```c
void test_parse_integer() {
    // Valid inputs
    assert_parses("0");
    assert_parses("42");
    assert_parses("999999");

    // Invalid inputs
    assert_parse_error("");
    assert_parse_error("abc");
    assert_parse_error("12.34");  // Float, not int

    // Boundary cases
    assert_parses("0");           // Minimum
    assert_parses("2147483647");  // INT_MAX
    assert_parse_error("2147483648");  // INT_MAX + 1
}
```

### Fuzzing Requirements

Every parser function must be fuzz-tested:

1. **No crashes** on any input (valid or invalid)
2. **Round-trip** property: `parse(print(parse(x))) == parse(x)`
3. **Deterministic** from seed: Same seed = same generated program

```c
// Fuzz test with seed for reproducibility
void fuzz_parser(uint64_t seed) {
    FuzzState state = fuzz_init(seed);

    for (int i = 0; i < 10000; i++) {
        char* source = fuzz_generate_program(&state);

        // Must not crash
        ParseResult result = parse(source);

        // If valid, must round-trip
        if (result.ok) {
            char* printed = print_ast(result.expr);
            ParseResult reparsed = parse(printed);
            assert(reparsed.ok);
            assert(ast_equal(result.expr, reparsed.expr));
        }
    }
}
```

---

## Code Formatting

### Hard Limits

- **100 columns** maximum (fits two files side-by-side)
- **4-space indentation** (no tabs)
- **70 lines** per function maximum

### Braces

Always use braces for `if`, even single-line (prevents bugs when adding lines).

```c
// ✅ Good: Always braces
if (condition) {
    do_something();
}

// ❌ Bad: No braces
if (condition)
    do_something();
```

### Blank Lines

Use blank lines to group related statements. One blank line between functions.

```c
// Group: Setup
Arena* arena = arena_create(SIZE);
Parser* parser = parser_new(arena, source);

// Group: Main work
Expr* expr = parse_expr(parser);
Type* type = typecheck(expr);

// Group: Cleanup
arena_destroy(arena);
```

---

## Dependencies

### Minimal Dependencies

Fern uses only these external dependencies:

| Library | Purpose | Why |
|---------|---------|-----|
| arena.h | Memory allocation | Eliminates use-after-free |
| sds.h | String handling | Binary-safe, length-tracked |
| stb_ds.h | Data structures | Hash maps, dynamic arrays |
| datatype99.h | Tagged unions | Exhaustive matching |

**No other dependencies.** If you need something, implement it or justify the addition.

### Explicit Over Implicit

Don't rely on library defaults. Pass options explicitly.

```c
// ✅ Good: Explicit options
hashmap_create(INITIAL_SIZE, LOAD_FACTOR, hash_string, compare_string);

// ❌ Bad: Relying on defaults
hashmap_create();  // What size? What hash function?
```

---

## AI-Specific Guidelines

These rules make AI-assisted development more effective:

### 1. Small Functions

70-line limit means AI can see entire function in context window.

### 2. Explicit Types

Always use explicit types. AI generates better code when types are clear.

```c
// ✅ Good: Explicit types
int32_t count = 0;
Expr* expr = NULL;

// ❌ Bad: Implicit/auto types
auto count = 0;  // What type?
var expr = ...;  // What type?
```

### 3. Assertions Document Intent

Assertions tell AI what invariants must hold.

```c
// AI can see these invariants and maintain them
assert(index >= 0);
assert(index < arrlen(items));
assert(items[index] != NULL);
```

### 4. Consistent Patterns

Use the same pattern everywhere. AI learns from repetition.

```c
// Always check result the same way
ParseResult result = parse_something(p);
if (!result.ok) {
    return (ParseResult){ .ok = false, .error = result.error };
}
// Use result.value...
```

### 5. Test-Driven Development

Write tests first. AI generates better implementation when tests exist.

---

## Summary Checklist

Before committing code, verify:

**Run automated checks:**
```bash
make clean && make && make test && make style
```

**Code Quality:**
- [ ] Minimum 2 assertions per function
- [ ] Function under 70 lines
- [ ] All loops have explicit bounds
- [ ] All errors handled (no ignored Results)
- [ ] Arena allocation only (no malloc/free)
- [ ] No raw `char*` parameters (use `sds` or `const char*`)

**Documentation:**
- [ ] All functions have `/** */` doc comments
- [ ] `@param` for each parameter
- [ ] `@return` for non-void functions
- [ ] Comments explain WHY, not just WHAT

**Testing:**
- [ ] Tests cover valid, invalid, and boundary cases
- [ ] Compiles with `-Wall -Wextra -Werror`
- [ ] Passes AddressSanitizer
- [ ] Passes UndefinedBehaviorSanitizer

---

## References

- [TigerBeetle TIGER_STYLE](https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md)
- [TigerStyle.dev](https://tigerstyle.dev/)
- [TigerBeetle Safety Concepts](https://docs.tigerbeetle.com/concepts/safety/)
