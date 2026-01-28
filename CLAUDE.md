# Fern Compiler - AI Development Guide

This guide contains critical information for AI-assisted development of the Fern compiler.

## Development Workflow

### CRITICAL: Test-Driven Development (TDD)

**ALWAYS write tests BEFORE implementation. This is non-negotiable.**

```
┌─────────────────────────────────────────────────────────┐
│ TDD Workflow (MUST FOLLOW)                              │
├─────────────────────────────────────────────────────────┤
│ 1. Read design.md - Understand the feature spec        │
│ 2. Write test FIRST - What should the code do?         │
│ 3. Run test - It should FAIL (red)                     │
│ 4. Implement - Make it pass                            │
│ 5. Run test - It should PASS (green)                   │
│ 6. Update ROADMAP.md - Mark task complete              │
│ 7. Commit - One feature at a time                      │
└─────────────────────────────────────────────────────────┘
```

**Example TDD Cycle:**

```c
// Step 1: Read design.md - "<- operator binds Result values"

// Step 2: Write test FIRST (tests/test_lexer.c)
void test_lex_bind_operator(void) {
    Arena* arena = arena_create(4096);
    Lexer* lex = lexer_new(arena, "content <- read_file(\"test\")");
    
    Token t1 = lexer_next(lex);
    ASSERT_STR_EQ(string_cstr(t1.text), "content");
    ASSERT_EQ(t1.type, TOKEN_IDENT);
    
    Token t2 = lexer_next(lex);
    ASSERT_EQ(t2.type, TOKEN_BIND);  // <-
    ASSERT_STR_EQ(string_cstr(t2.text), "<-");
    
    arena_destroy(arena);
}

// Step 3: Run test - FAILS (TOKEN_BIND doesn't exist yet)

// Step 4: Implement
typedef enum {
    TOKEN_IDENT,
    TOKEN_BIND,    // Add <- token
    // ...
} TokenType;

Token lexer_next(Lexer* lex) {
    // ... implementation to recognize <-
}

// Step 5: Run test - PASSES
// Step 6: Update ROADMAP.md milestone progress
// Step 7: Commit with message: "feat(lexer): add <- bind operator token"
```

### Required Reading Before Every Task

1. **design.md** - Source of truth for Fern language features
2. **ROADMAP.md** - Current milestone and task status
3. **DECISIONS.md** - Record of architectural decisions
4. **CLAUDE.md** (this file) - Safety guidelines and patterns

**Before writing ANY code:**
- [ ] Read relevant section of design.md
- [ ] Check ROADMAP.md for current milestone
- [ ] Write test based on design.md spec
- [ ] Verify test fails (red)
- [ ] Then implement

### When to Add a Decision

**ALWAYS document significant decisions in DECISIONS.md using `/decision`**

Add a decision when you:
- Choose between multiple valid approaches
- Make a design trade-off (performance vs simplicity, etc.)
- Deviate from common patterns or conventions
- Add/remove a major feature
- Change an existing design decision

**Examples of decisions to document:**
- ✅ "Should we use garbage collection or manual memory management?"
- ✅ "Should we support null values or use Option types?"
- ✅ "Should we use tabs or spaces for indentation?"
- ✅ "Should we add a REPL or focus on compiled execution?"
- ❌ "Should this variable be named 'count' or 'total'?" (too minor)
- ❌ "Should we add a comment here?" (too trivial)

**How to add a decision:**

```bash
# Use the /decision skill
/decision DECISIONS.md | Title | Brief description

# Example:
/decision DECISIONS.md | Using tabs for indentation | We decided to enforce tabs instead of spaces
```

The skill will guide you through:
1. Status (Adopted, Accepted, Deprecated, Supersedes)
2. Decision statement ("I will...")
3. Context (why? what alternatives?)
4. Consequences (what follows from this?)

**During implementation, if you face a choice:**
1. Pause implementation
2. Document the decision with `/decision`
3. Continue implementation following the decision

### Updating ROADMAP.md

**After completing each task:**

```markdown
<!-- Before -->
- [ ] Implement <- operator lexing

<!-- After -->
- [x] Implement <- operator lexing (test_lex_bind_operator passes)
```

**After completing each milestone:**

```markdown
## Milestone 1: Lexer ✓ COMPLETE

**Status**: ✓ All tests passing (45/45)
**Completed**: 2024-01-28
```

### Pre-Commit Checklist

Before every commit, verify:

- [ ] All tests pass: `make test`
- [ ] No compiler warnings: `make clean && make`
- [ ] ROADMAP.md is updated with progress
- [ ] Code follows design.md specification
- [ ] Test was written BEFORE implementation
- [ ] Commit message follows conventional commits
- [ ] Significant decisions are documented in DECISIONS.md

### Commit Message Format

```
<type>(<scope>): <short summary>

<detailed description>

Tests:
- test_name_1 passes
- test_name_2 passes

Refs: design.md section X.Y
```

**Types:**
- `feat`: New feature implementation
- `fix`: Bug fix
- `test`: Adding tests (without implementation)
- `refactor`: Code restructuring
- `docs`: Documentation only
- `chore`: Build system, tooling

**Examples:**

```
feat(lexer): add <- bind operator token

Implement lexing for the <- operator used in Result binding.
This operator comes before the function call to make error
handling visible at the call site.

Tests:
- test_lex_bind_operator passes

Refs: design.md section 3.2 (Error Handling)
```

```
test(parser): add tests for with expression

Add comprehensive tests for with expression parsing before
implementing the feature. All tests currently fail as expected.

Tests:
- test_parse_with_expression (FAILING - expected)
- test_parse_with_else_clause (FAILING - expected)

Refs: design.md section 3.2.2 (with expression)
```

### When Tests Fail

**DO NOT:**
- Modify the test to make it pass
- Skip the failing test
- Comment out assertions
- Commit failing tests

**DO:**
- Fix the implementation to match the spec in design.md
- If design.md is unclear, ask for clarification
- Add more tests to isolate the issue
- Use debugging tools (gdb, AddressSanitizer)

### Directory Structure Reminder

```
fern/
├── design.md           # ← SPECIFICATION (read this FIRST)
├── ROADMAP.md          # ← TASKS (update after each task)
├── DECISIONS.md        # ← DECISIONS (document choices with /decision)
├── CLAUDE.md           # ← THIS FILE (safety guidelines)
├── BUILD.md            # ← Build instructions
├── src/                # Implementation code
├── tests/              # Tests (write FIRST)
├── lib/                # Safety libraries
├── include/            # Header files
└── scripts/            # Development scripts
```

## Language: C with Safety Libraries

The Fern compiler is written in **C11** with modern safety libraries. This provides:
- Excellent AI code generation (C is well-represented in training data)
- Fast compilation and iteration
- Direct control over memory and codegen
- Native performance

## Critical Safety Rules

### Memory Management: Arena Allocation Only

**ALWAYS use arena allocation. NEVER call free() on individual allocations.**

```c
#include "arena.h"

// ✅ CORRECT: Arena allocation
typedef struct {
    Arena ast_arena;
    Arena string_arena;
    Arena type_arena;
} CompilerContext;

Expr* new_expr(CompilerContext *ctx) {
    // Allocate from arena - no individual free needed
    Expr *e = arena_alloc(&ctx->ast_arena, sizeof(Expr));
    return e;
}

void compile(const char *filename) {
    CompilerContext ctx = {0};
    
    // ... use ctx ...
    
    // Free all at once when done
    arena_free(&ctx.ast_arena);
    arena_free(&ctx.string_arena);
    arena_free(&ctx.type_arena);
}

// ❌ WRONG: Individual malloc/free
Expr* bad_example() {
    Expr *e = malloc(sizeof(Expr));  // DON'T DO THIS
    // ...
    free(e);  // DANGEROUS - easy to forget or double-free
    return e;
}
```

**Why:** Arena allocation eliminates entire classes of bugs:
- No use-after-free
- No double-free
- No memory leaks
- Faster allocation (bump pointer)

### Tagged Unions: Use Datatype99

**ALWAYS use Datatype99 for variant types. NEVER manual tagged unions.**

```c
#include <datatype99.h>

// ✅ CORRECT: Datatype99 tagged unions
datatype(
    Expr,
    (IntLit, int64_t),
    (StringLit, sds),
    (BinOp, struct Expr*, TokenType, struct Expr*),
    (Ident, sds)
);

// Pattern matching with exhaustive checking
int64_t eval(Expr *expr) {
    match(*expr) {
        of(IntLit, n) return *n;
        of(BinOp, lhs, op, rhs) {
            // Compiler checks this is exhaustive
            return eval_binop(*lhs, *op, *rhs);
        }
        of(Ident, name) return lookup(*name);
        of(StringLit, s) return 0;  // Must handle ALL cases
    }
    // Compiler warns if we miss a case!
}

// ❌ WRONG: Manual tagged unions
typedef enum { EXPR_INT, EXPR_BINOP } ExprKind;
typedef struct {
    ExprKind kind;  // DON'T DO THIS
    union {
        int64_t int_val;
        struct { void *lhs, *rhs; } binop;
    } data;
} BadExpr;
```

**Why:** Datatype99 provides:
- Exhaustive matching (compiler warns on missing cases)
- Type safety (can't access wrong variant)
- Clearer code (AI generates it correctly)

### Strings: Use SDS (Simple Dynamic Strings)

**ALWAYS use SDS for strings. NEVER use char* with manual length tracking.**

```c
#include "sds.h"

// ✅ CORRECT: SDS strings
sds name = sdsnew("hello");
name = sdscat(name, " world");
printf("Length: %zu\n", sdslen(name));  // O(1)
printf("String: %s\n", name);
sdsfree(name);

// ✅ CORRECT: SDS in structs
typedef struct {
    sds filename;
    sds source;
    int line;
} SourceLoc;

SourceLoc make_loc(const char *file, const char *src) {
    return (SourceLoc){
        .filename = sdsnew(file),
        .source = sdsnew(src),
        .line = 1
    };
}

// ❌ WRONG: Manual char* management
char *bad = malloc(100);  // DON'T DO THIS
strcpy(bad, "hello");     // Unsafe
strcat(bad, " world");    // Can overflow
```

**Why:** SDS provides:
- Binary safety (handles null bytes)
- O(1) length (stored in header)
- Automatic resizing
- Still compatible with C string APIs

### Data Structures: Use stb_ds.h

**ALWAYS use stb_ds for dynamic arrays and hash maps.**

```c
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// ✅ CORRECT: Dynamic arrays
Expr **exprs = NULL;  // Always initialize to NULL
arrput(exprs, expr1);
arrput(exprs, expr2);
printf("Length: %d\n", arrlen(exprs));

for (int i = 0; i < arrlen(exprs); i++) {
    process(exprs[i]);
}

arrfree(exprs);

// ✅ CORRECT: Hash maps (string -> value)
typedef struct {
    sds key;
    Type *value;
} SymbolEntry;

SymbolEntry *symbols = NULL;  // Initialize to NULL
shput(symbols, "x", type_int);
shput(symbols, "y", type_string);

Type *t = shget(symbols, "x");
if (t) {
    // Found
}

shfree(symbols);

// ❌ WRONG: Manual array management
Expr **bad = malloc(10 * sizeof(Expr*));  // DON'T
// ... resize manually, error-prone
```

**Why:** stb_ds provides:
- Type-safe macros
- Automatic resizing
- Hash maps with string keys (perfect for symbol tables)
- Simple API (AI generates correctly)

### Error Handling: Use Result Types

**ALWAYS return Result types for operations that can fail.**

```c
// ✅ CORRECT: Result types with Datatype99
datatype(
    ParseResult,
    (OkExpr, Expr*),
    (ParseErr, sds)
);

ParseResult parse_expr(Parser *p) {
    if (peek(p).type != TOK_INT) {
        sds err = sdscatprintf(sdsempty(),
            "%s:%d: Expected integer",
            p->filename, p->line);
        return ParseErr(err);
    }
    
    Expr *e = /* ... */;
    return OkExpr(e);
}

// Usage - MUST handle both cases
ParseResult result = parse_expr(parser);
match(result) {
    of(OkExpr, expr) {
        codegen(*expr);
    }
    of(ParseErr, msg) {
        fprintf(stderr, "%s\n", *msg);
        exit(1);
    }
}

// ❌ WRONG: Return NULL on error
Expr* bad_parse(Parser *p) {
    if (error) return NULL;  // Lost error information!
    return expr;
}
```

**Why:** Result types:
- Force error handling (exhaustive matching)
- Preserve error messages
- Type-safe (can't ignore errors)
- Clear in function signatures

## Compilation Flags

### Debug Build (Development)

```bash
gcc -std=c11 -g -O0 \
    -fsanitize=address,undefined \
    -fstack-protector-strong \
    -Wall -Wextra -Wpedantic \
    -fanalyzer \
    -ftrack-macro-expansion=0 \
    -Ideps -Isrc \
    src/*.c deps/sds.c \
    -o fern
```

**Required flags:**
- `-std=c11` - Use C11 standard
- `-ftrack-macro-expansion=0` - Required for Datatype99 (GCC)
- `-fmacro-backtrace-limit=1` - Required for Datatype99 (Clang)

**Safety flags (debug only):**
- `-fsanitize=address` - Detect buffer overflows, use-after-free
- `-fsanitize=undefined` - Detect undefined behavior
- `-fstack-protector-strong` - Stack overflow protection
- `-fanalyzer` - Static analysis (GCC 10+)

### Release Build (Production)

```bash
gcc -std=c11 -O2 -DNDEBUG \
    -ftrack-macro-expansion=0 \
    -Ideps -Isrc \
    src/*.c deps/sds.c \
    -o fern
```

## Common Patterns

### String Interning

```c
typedef struct {
    sds key;
    int id;
} InternEntry;

InternEntry *intern_table = NULL;  // stb_ds hash map
int next_id = 0;

int intern_string(const char *str) {
    int idx = shgeti(intern_table, str);
    if (idx >= 0) {
        return intern_table[idx].id;
    }
    
    InternEntry entry = {
        .key = sdsnew(str),
        .id = next_id++
    };
    shput(intern_table, entry.key, entry.id);
    return entry.id;
}
```

### Symbol Table

```c
typedef struct {
    sds name;
    Type *type;
    int scope_depth;
} Symbol;

Symbol *symbols = NULL;  // Dynamic array

void add_symbol(const char *name, Type *type, int depth) {
    Symbol sym = {
        .name = sdsnew(name),
        .type = type,
        .scope_depth = depth
    };
    arrput(symbols, sym);
}

Symbol* lookup_symbol(const char *name) {
    // Search backwards (innermost scope first)
    for (int i = arrlen(symbols) - 1; i >= 0; i--) {
        if (strcmp(symbols[i].name, name) == 0) {
            return &symbols[i];
        }
    }
    return NULL;
}
```

### Error Messages

```c
datatype(
    ErrorType,
    (SyntaxError, sds),
    (TypeError, sds),
    (NameError, sds)
);

typedef struct {
    ErrorType type;
    sds filename;
    int line;
    int column;
} CompileError;

CompileError make_error(ErrorType type, const char *file, int line, int col) {
    return (CompileError){
        .type = type,
        .filename = sdsnew(file),
        .line = line,
        .column = col
    };
}

void print_error(CompileError *err) {
    fprintf(stderr, "%s:%d:%d: ", err->filename, err->line, err->column);
    
    match(err->type) {
        of(SyntaxError, msg) {
            fprintf(stderr, "Syntax error: %s\n", *msg);
        }
        of(TypeError, msg) {
            fprintf(stderr, "Type error: %s\n", *msg);
        }
        of(NameError, msg) {
            fprintf(stderr, "Name error: %s\n", *msg);
        }
    }
}
```

## QBE Code Generation

### Basic Template

```c
void codegen_function(FILE *out, Function *fn) {
    // Function signature
    fprintf(out, "export function w $%s(", fn->name);
    
    for (int i = 0; i < arrlen(fn->params); i++) {
        if (i > 0) fprintf(out, ", ");
        fprintf(out, "w %%arg%d", i);
    }
    
    fprintf(out, ") {\n@start\n");
    
    // Function body
    for (int i = 0; i < arrlen(fn->body); i++) {
        codegen_stmt(out, fn->body[i]);
    }
    
    fprintf(out, "}\n\n");
}

sds codegen_expr(FILE *out, Expr *expr, int *tmp_counter) {
    match(*expr) {
        of(IntLit, n) {
            sds tmp = sdscatprintf(sdsempty(), "%%t%d", (*tmp_counter)++);
            fprintf(out, "    %s =w copy %ld\n", tmp, *n);
            return tmp;
        }
        
        of(BinOp, lhs, op, rhs) {
            sds left = codegen_expr(out, *lhs, tmp_counter);
            sds right = codegen_expr(out, *rhs, tmp_counter);
            sds result = sdscatprintf(sdsempty(), "%%t%d", (*tmp_counter)++);
            
            const char *qbe_op = NULL;
            switch (*op) {
                case TOK_PLUS:  qbe_op = "add"; break;
                case TOK_MINUS: qbe_op = "sub"; break;
                case TOK_STAR:  qbe_op = "mul"; break;
                case TOK_SLASH: qbe_op = "div"; break;
            }
            
            fprintf(out, "    %s =w %s %s, %s\n", result, qbe_op, left, right);
            
            sdsfree(left);
            sdsfree(right);
            return result;
        }
    }
    
    return NULL;
}
```

## Testing Pattern

```c
// test_lexer.c
#include <assert.h>
#include "lexer.h"

void test_lex_integer() {
    CompilerContext ctx = {0};
    Lexer *lex = lexer_new(&ctx, "42");
    Token tok = next_token(lex);
    
    assert(tok.type == TOK_INT);
    assert(strcmp(tok.text, "42") == 0);
    
    ctx_free(&ctx);
}

int main() {
    test_lex_integer();
    // ... more tests
    
    printf("All tests passed!\n");
    return 0;
}
```

## DO NOT Do These Things

### ❌ Manual Memory Management
```c
// WRONG
char *s = malloc(100);
strcpy(s, "hello");
// ... forget to free, or double free
```

### ❌ Manual Tagged Unions
```c
// WRONG
typedef struct {
    enum { INT, STRING } kind;
    union { int i; char *s; } data;
} BadVariant;
```

### ❌ Null-Terminated String Handling
```c
// WRONG
char buf[100];
strcpy(buf, input);  // Buffer overflow
strcat(buf, more);   // More overflow risk
```

### ❌ Returning NULL for Errors
```c
// WRONG
Expr* parse() {
    if (error) return NULL;  // Lost context
    return expr;
}
```

### ❌ Manual Array Management
```c
// WRONG
int size = 10;
Expr **arr = malloc(size * sizeof(Expr*));
// Manual resize, easy to get wrong
```

## Summary Checklist

Before committing code, verify:

- [ ] All allocations use arena (no malloc/free)
- [ ] All variants use Datatype99 (no manual unions)
- [ ] All strings use SDS (no char*)
- [ ] All arrays/maps use stb_ds (no manual management)
- [ ] All fallible functions return Result types
- [ ] All match() statements are exhaustive
- [ ] Compiles with `-Wall -Wextra -Wpedantic` (no warnings)
- [ ] Passes AddressSanitizer (no memory errors)
- [ ] Passes UndefinedBehaviorSanitizer (no UB)

## Resources

- **Arena**: https://github.com/tsoding/arena
- **Datatype99**: https://github.com/Hirrolot/datatype99
- **SDS**: https://github.com/antirez/sds
- **stb_ds.h**: https://github.com/nothings/stb
- **QBE**: https://c9x.me/compile/

## Key Insight for AI

The entire safety strategy is:
1. **Memory**: Never free individually, use arenas
2. **Variants**: Never manual tags, use Datatype99
3. **Strings**: Never char*, use SDS
4. **Collections**: Never manual arrays, use stb_ds
5. **Errors**: Never NULL, use Result types

These five rules eliminate 90% of C's danger while keeping AI productivity high.
