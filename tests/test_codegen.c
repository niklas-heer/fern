/* Code Generator Tests */

#include "test.h"
#include "codegen.h"
#include "parser.h"
#include "arena.h"
#include "fern_string.h"
#include <string.h>

/* Helper to generate QBE from source code */
static const char* generate_qbe(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    StmtVec* stmts = parse_stmts(parser);
    if (!stmts || parser->had_error) return NULL;
    
    Codegen* cg = codegen_new(arena);
    codegen_program(cg, stmts);
    return string_cstr(codegen_output(cg));
}

/* Helper to generate QBE for a single expression */
static const char* generate_expr_qbe(Arena* arena, const char* src) {
    Parser* parser = parser_new(arena, src);
    Expr* expr = parse_expr(parser);
    if (!expr || parser->had_error) return NULL;
    
    Codegen* cg = codegen_new(arena);
    codegen_expr(cg, expr);
    return string_cstr(codegen_output(cg));
}

static int count_substring(const char* haystack, const char* needle) {
    int count = 0;
    const char* p = haystack;
    size_t needle_len = strlen(needle);
    while (p && *p) {
        const char* match = strstr(p, needle);
        if (!match) break;
        count++;
        p = match + needle_len;
    }
    return count;
}

/* ========== Integer Literal Tests ========== */

void test_codegen_int_literal(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "42");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 42") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_negative_int(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "-5");
    
    ASSERT_NOT_NULL(qbe);
    /* -5 is parsed as unary negation of 5 */
    ASSERT_TRUE(strstr(qbe, "copy 5") != NULL);
    ASSERT_TRUE(strstr(qbe, "sub 0") != NULL);
    
    arena_destroy(arena);
}

/* ========== Binary Operation Tests ========== */

void test_codegen_add(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "1 + 2");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 2") != NULL);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_sub(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "10 - 3");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "sub") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_mul(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "4 * 5");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "mul") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_div(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "20 / 4");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "div") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_complex_expr(void) {
    Arena* arena = arena_create(4096);
    
    /* (1 + 2) * 3 */
    const char* qbe = generate_expr_qbe(arena, "(1 + 2) * 3");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    ASSERT_TRUE(strstr(qbe, "mul") != NULL);
    
    arena_destroy(arena);
}

/* ========== Comparison Tests ========== */

void test_codegen_eq(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "1 == 2");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "ceqw") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_lt(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "1 < 2");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "csltw") != NULL);
    
    arena_destroy(arena);
}

/* ========== Function Definition Tests ========== */

void test_codegen_fn_simple(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "fn answer() -> Int: 42");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "function") != NULL);
    ASSERT_TRUE(strstr(qbe, "$answer") != NULL);
    ASSERT_TRUE(strstr(qbe, "ret") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_fn_with_params(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "fn add(a: Int, b: Int) -> Int: a + b");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$add") != NULL);
    ASSERT_TRUE(strstr(qbe, "%a") != NULL);
    ASSERT_TRUE(strstr(qbe, "%b") != NULL);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_fn_call(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "add(1, 2)");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "call $add") != NULL);
    
    arena_destroy(arena);
}

/* ========== If Expression Tests ========== */

void test_codegen_if_expr(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "if true: 1 else: 0");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);  /* Conditional jump */
    ASSERT_TRUE(strstr(qbe, "@L") != NULL);   /* Labels */
    
    arena_destroy(arena);
}

/* ========== Let Statement Tests ========== */

void test_codegen_let(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "let x = 42");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "%x") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy") != NULL);
    
    arena_destroy(arena);
}

/* ========== String Tests ========== */

void test_codegen_string_literal(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "\"hello\"");
    
    ASSERT_NOT_NULL(qbe);
    /* String literals should create a data section */
    ASSERT_TRUE(strstr(qbe, "data") != NULL);
    ASSERT_TRUE(strstr(qbe, "hello") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_string_in_fn(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_qbe(arena, "fn greet() -> String: \"hello\"");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$greet") != NULL);
    ASSERT_TRUE(strstr(qbe, "data") != NULL);
    
    arena_destroy(arena);
}

/* ========== Boolean Tests ========== */

void test_codegen_bool_true(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "true");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_bool_false(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "false");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 0") != NULL);
    
    arena_destroy(arena);
}

/* ========== Float Tests ========== */

void test_codegen_float_literal(void) {
    Arena* arena = arena_create(4096);
    
    const char* qbe = generate_expr_qbe(arena, "3.14");
    
    ASSERT_NOT_NULL(qbe);
    /* Float literals use 'd' (double) type in QBE */
    ASSERT_TRUE(strstr(qbe, "3.14") != NULL);
    
    arena_destroy(arena);
}

/* ========== Match Expression Tests ========== */

void test_codegen_match_int(void) {
    Arena* arena = arena_create(4096);
    
    /* match x: 1 -> 10, 2 -> 20, _ -> 0 */
    const char* qbe = generate_qbe(arena, 
        "fn test(x: Int) -> Int: match x: 1 -> 10, 2 -> 20, _ -> 0");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$test") != NULL);
    /* Should have comparison and jumps */
    ASSERT_TRUE(strstr(qbe, "ceqw") != NULL);  /* compare equal */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);   /* conditional jump */
    
    arena_destroy(arena);
}

void test_codegen_match_wildcard(void) {
    Arena* arena = arena_create(4096);
    
    /* Simple match with wildcard */
    const char* qbe = generate_qbe(arena,
        "fn always_zero(x: Int) -> Int: match x: _ -> 0");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "copy 0") != NULL);
    
    arena_destroy(arena);
}

/* ========== Tuple Tests ========== */

void test_codegen_tuple(void) {
    Arena* arena = arena_create(4096);
    
    /* Simple tuple */
    const char* qbe = generate_expr_qbe(arena, "(1, 2, 3)");
    
    ASSERT_NOT_NULL(qbe);
    /* Tuple elements should be generated */
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 2") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 3") != NULL);
    
    arena_destroy(arena);
}

/* ========== List Tests ========== */

void test_codegen_list(void) {
    Arena* arena = arena_create(4096);
    
    /* Simple list */
    const char* qbe = generate_expr_qbe(arena, "[1, 2, 3]");
    
    ASSERT_NOT_NULL(qbe);
    /* List elements should be generated */
    ASSERT_TRUE(strstr(qbe, "copy 1") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 2") != NULL);
    ASSERT_TRUE(strstr(qbe, "copy 3") != NULL);
    
    arena_destroy(arena);
}

/* ========== Lambda Tests ========== */

void test_codegen_lambda(void) {
    Arena* arena = arena_create(4096);
    
    /* Lambda that adds 1 to its argument */
    const char* qbe = generate_expr_qbe(arena, "(x) -> x + 1");
    
    ASSERT_NOT_NULL(qbe);
    /* Lambda should generate a function */
    ASSERT_TRUE(strstr(qbe, "function") != NULL);
    ASSERT_TRUE(strstr(qbe, "add") != NULL);
    
    arena_destroy(arena);
}

/* ========== Try Expression (?) Tests ========== */

void test_codegen_try_operator(void) {
    Arena* arena = arena_create(4096);
    
    /* fn get_value() -> Result(Int, String): Ok(42)? */
    const char* qbe = generate_qbe(arena, 
        "fn get_value() -> Result(Int, String): Ok(42)?");
    
    ASSERT_NOT_NULL(qbe);
    /* ? operator should generate:
     * 1. Call to check if Result is Ok
     * 2. Conditional branch on error
     * 3. Return early if Err
     * 4. Unwrap value if Ok
     */
    ASSERT_TRUE(strstr(qbe, "call") != NULL);  /* Runtime call */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);   /* Conditional branch */
    
    arena_destroy(arena);
}

void test_codegen_try_in_chain(void) {
    Arena* arena = arena_create(8192);  /* Larger arena for complex code */
    
    /* Multiple ? in sequence - each needs its own check */
    /* Simpler version: just test that ? generates expected pattern */
    const char* qbe = generate_qbe(arena,
        "fn chain(x: Int) -> Result(Int, String): Ok(x)?");
    
    ASSERT_NOT_NULL(qbe);
    /* Should have conditional branch for ? operator */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);
    ASSERT_TRUE(strstr(qbe, "fern_result") != NULL);
    
    arena_destroy(arena);
}

/* ========== String Runtime Integration Tests ========== */

void test_codegen_string_concat(void) {
    Arena* arena = arena_create(8192);
    
    /* String concatenation - for now just verify the function compiles */
    /* Full string concat requires runtime integration */
    const char* qbe = generate_qbe(arena,
        "fn greet(name: String) -> String: name");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$greet") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_string_new(void) {
    Arena* arena = arena_create(4096);
    
    /* String literal should create a FernString via runtime */
    const char* qbe = generate_qbe(arena,
        "fn get_str() -> String: \"hello\"");
    
    ASSERT_NOT_NULL(qbe);
    /* Should have string data and potentially runtime call */
    ASSERT_TRUE(strstr(qbe, "data") != NULL);
    ASSERT_TRUE(strstr(qbe, "hello") != NULL);
    
    arena_destroy(arena);
}

/* ========== List Runtime Integration Tests ========== */

void test_codegen_list_new(void) {
    Arena* arena = arena_create(8192);
    
    /* List creation should call fern_list_new and fern_list_push */
    const char* qbe = generate_qbe(arena,
        "fn make_list() -> List(Int): [1, 2, 3]");
    
    ASSERT_NOT_NULL(qbe);
    /* Should call runtime list functions */
    ASSERT_TRUE(strstr(qbe, "$fern_list_new") != NULL);
    ASSERT_TRUE(strstr(qbe, "$fern_list_push") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_list_index(void) {
    Arena* arena = arena_create(8192);
    
    /* List indexing should call fern_list_get */
    const char* qbe = generate_qbe(arena,
        "fn first(items: List(Int)) -> Int: items[0]");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$first") != NULL);
    ASSERT_TRUE(strstr(qbe, "$fern_list_get") != NULL);
    
    arena_destroy(arena);
}

/* ========== Result Type Tests ========== */

void test_codegen_ok_constructor(void) {
    Arena* arena = arena_create(8192);
    
    /* Ok() constructor should create a Result with tag=0 */
    const char* qbe = generate_qbe(arena,
        "fn success() -> Result(Int, String): Ok(42)");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$success") != NULL);
    /* Should call fern_result_ok */
    ASSERT_TRUE(strstr(qbe, "$fern_result_ok") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_err_constructor(void) {
    Arena* arena = arena_create(8192);
    
    /* Err() constructor should create a Result with tag=1 */
    const char* qbe = generate_qbe(arena,
        "fn failure() -> Result(Int, String): Err(\"error\")");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$failure") != NULL);
    /* Should call fern_result_err */
    ASSERT_TRUE(strstr(qbe, "$fern_result_err") != NULL);
    
    arena_destroy(arena);
}

/* ========== For Loop Tests ========== */

void test_codegen_for_loop(void) {
    Arena* arena = arena_create(8192);
    
    /* For loop iterating over a list */
    const char* qbe = generate_expr_qbe(arena, "for x in [1, 2, 3]: x");
    
    ASSERT_NOT_NULL(qbe);
    /* Should have loop structure: labels for loop start, body, end */
    ASSERT_TRUE(strstr(qbe, "@L") != NULL);  /* Should have labels */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL); /* Conditional jump for loop condition */
    ASSERT_TRUE(strstr(qbe, "jmp") != NULL); /* Unconditional jump back to loop start */
    /* Should call fern_list_len to get length */
    ASSERT_TRUE(strstr(qbe, "$fern_list_len") != NULL);
    /* Should call fern_list_get to get elements */
    ASSERT_TRUE(strstr(qbe, "$fern_list_get") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_for_in_function(void) {
    Arena* arena = arena_create(8192);
    
    /* For loop inside a function */
    const char* qbe = generate_qbe(arena,
        "fn sum_list(items: List(Int)) -> Int: for x in items: x");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$sum_list") != NULL);
    /* Should have loop labels */
    ASSERT_TRUE(strstr(qbe, "@L") != NULL);
    
    arena_destroy(arena);
}

/* ========== Defer Statement Tests ========== */

void test_codegen_defer_simple(void) {
    Arena* arena = arena_create(8192);
    
    /* Simple defer statement - cleanup should be called before return */
    const char* qbe = generate_qbe(arena,
        "fn process() -> Int: { defer cleanup(), 42 }");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$process") != NULL);
    /* Should call cleanup before returning */
    ASSERT_TRUE(strstr(qbe, "$cleanup") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_defer_multiple(void) {
    Arena* arena = arena_create(8192);
    
    /* Multiple defers - should run in reverse order (LIFO) */
    const char* qbe = generate_qbe(arena,
        "fn process() -> Int: { defer cleanup1(), defer cleanup2(), 42 }");
    
    ASSERT_NOT_NULL(qbe);
    /* Both cleanups should be called */
    ASSERT_TRUE(strstr(qbe, "$cleanup1") != NULL);
    ASSERT_TRUE(strstr(qbe, "$cleanup2") != NULL);
    /* cleanup2 should appear before cleanup1 in the output (LIFO) */
    const char* pos1 = strstr(qbe, "$cleanup1");
    const char* pos2 = strstr(qbe, "$cleanup2");
    ASSERT_TRUE(pos2 < pos1); /* cleanup2 runs first, so appears first */
    
    arena_destroy(arena);
}

/* ========== With Expression Tests ========== */

void test_codegen_with_simple(void) {
    Arena* arena = arena_create(8192);
    
    /* Simple with expression - binds result and continues */
    const char* qbe = generate_qbe(arena,
        "fn process() -> Result(Int, String): with x <- get_value() do Ok(x)");
    
    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$process") != NULL);
    /* Should call get_value */
    ASSERT_TRUE(strstr(qbe, "$get_value") != NULL);
    /* Should have conditional jump for Ok/Err check */
    ASSERT_TRUE(strstr(qbe, "jnz") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_with_multiple_bindings(void) {
    Arena* arena = arena_create(8192);
    
    /* Multiple bindings - each checked for Ok/Err */
    const char* qbe = generate_qbe(arena,
        "fn process() -> Result(Int, String): with x <- get_a(), y <- get_b() do Ok(x)");
    
    ASSERT_NOT_NULL(qbe);
    /* Should call both get_a and get_b */
    ASSERT_TRUE(strstr(qbe, "$get_a") != NULL);
    ASSERT_TRUE(strstr(qbe, "$get_b") != NULL);
    /* Should check results */
    ASSERT_TRUE(strstr(qbe, "$fern_result_is_ok") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_with_no_else_propagates_error(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_qbe(arena,
        "fn process() -> Result(Int, String): with x <- get_value() do Ok(x)");

    ASSERT_NOT_NULL(qbe);
    /* One early return for Err path + one final function return */
    ASSERT_TRUE(count_substring(qbe, "ret ") >= 2);
    ASSERT_TRUE(strstr(qbe, "TODO: else arm matching") == NULL);

    arena_destroy(arena);
}

void test_codegen_with_else_arms(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_qbe(arena,
        "fn process() -> Int: with x <- get_value() do x else Err(e) -> 0");

    ASSERT_NOT_NULL(qbe);
    /* Else handling should unwrap the failed Result and test constructor arms */
    ASSERT_TRUE(strstr(qbe, "$fern_result_unwrap") != NULL);
    ASSERT_TRUE(strstr(qbe, "loadw") != NULL);
    ASSERT_TRUE(strstr(qbe, "ceqw") != NULL);
    /* Placeholder TODO emission should be gone */
    ASSERT_TRUE(strstr(qbe, "TODO: else arm matching") == NULL);

    arena_destroy(arena);
}

/* ========== Pointer Type Handling Tests ========== */

/* Test: Function returning tuple uses 'l' return type */
void test_codegen_fn_returns_tuple(void) {
    Arena* arena = arena_create(8192);
    
    const char* qbe = generate_qbe(arena,
        "fn get_pair() -> (Int, Int): (1, 2)\n"
        "fn main(): let p = get_pair() 0");
    
    ASSERT_NOT_NULL(qbe);
    /* Function should return 'l' (pointer) for tuple */
    ASSERT_TRUE(strstr(qbe, "function l $get_pair") != NULL);
    /* Call should use '=l' */
    ASSERT_TRUE(strstr(qbe, "=l call $get_pair") != NULL);
    
    arena_destroy(arena);
}

/* Test: Function returning String uses 'l' return type */
void test_codegen_fn_returns_string(void) {
    Arena* arena = arena_create(8192);
    
    const char* qbe = generate_qbe(arena,
        "fn greet() -> String: \"hello\"\n"
        "fn main(): let s = greet() 0");
    
    ASSERT_NOT_NULL(qbe);
    /* Function should return 'l' (pointer) for String */
    ASSERT_TRUE(strstr(qbe, "function l $greet") != NULL);
    /* Call should use '=l' */
    ASSERT_TRUE(strstr(qbe, "=l call $greet") != NULL);
    
    arena_destroy(arena);
}

/* Test: Function with String parameter uses 'l' param type */
void test_codegen_fn_string_param(void) {
    Arena* arena = arena_create(8192);
    
    const char* qbe = generate_qbe(arena,
        "fn process(s: String) -> Int: 0\n"
        "fn main(): process(\"test\") 0");
    
    ASSERT_NOT_NULL(qbe);
    /* Parameter should be 'l' (pointer) for String */
    ASSERT_TRUE(strstr(qbe, "function w $process(l %s)") != NULL);
    
    arena_destroy(arena);
}

/* Test: Function with List parameter uses 'l' param type */
void test_codegen_fn_list_param(void) {
    Arena* arena = arena_create(8192);
    
    const char* qbe = generate_qbe(arena,
        "fn process(items: List(Int)) -> Int: 0\n"
        "fn main(): process([1, 2, 3]) 0");
    
    ASSERT_NOT_NULL(qbe);
    /* Parameter should be 'l' (pointer) for List */
    ASSERT_TRUE(strstr(qbe, "function w $process(l %items)") != NULL);
    
    arena_destroy(arena);
}

/* Test: If expression returning String uses 'l' type */
void test_codegen_if_returns_string(void) {
    Arena* arena = arena_create(8192);
    
    const char* qbe = generate_qbe(arena,
        "fn choose(b: Bool) -> String: if b: \"yes\" else: \"no\"\n"
        "fn main(): let s = choose(true) 0");
    
    ASSERT_NOT_NULL(qbe);
    /* If branches should use '=l copy' for String results */
    ASSERT_TRUE(strstr(qbe, "=l copy") != NULL);
    
    arena_destroy(arena);
}

void test_codegen_dup_inserted_for_pointer_alias_binding(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_qbe(arena,
        "fn keep_alias(x: String) -> String: let y = x y");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "function l $keep_alias(l %x)") != NULL);
    ASSERT_TRUE(strstr(qbe, "%y =l call $fern_dup(l %x)") != NULL);
    ASSERT_TRUE(strstr(qbe, "call $fern_drop(l %x)") != NULL);
    ASSERT_TRUE(strstr(qbe, "call $fern_drop(l %y)") == NULL);

    arena_destroy(arena);
}

void test_codegen_drop_inserted_for_unreturned_pointer_bindings(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_qbe(arena,
        "fn use_alias(x: String) -> Int: let y = x 0");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "%y =l call $fern_dup(l %x)") != NULL);
    ASSERT_TRUE(strstr(qbe, "call $fern_drop(l %y)") != NULL);
    ASSERT_TRUE(strstr(qbe, "call $fern_drop(l %x)") != NULL);

    arena_destroy(arena);
}

/* ========== Lowercase Stdlib API Tests ========== */

void test_codegen_fs_read_calls_runtime(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "fs.read(\"notes.txt\")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_read_file") != NULL);

    arena_destroy(arena);
}

void test_codegen_json_parse_calls_runtime(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "json.parse(\"[]\")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_json_parse") != NULL);

    arena_destroy(arena);
}

void test_codegen_http_get_calls_runtime(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "http.get(\"https://example.com\")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_http_get") != NULL);

    arena_destroy(arena);
}

void test_codegen_sql_open_calls_runtime(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "sql.open(\"app.db\")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_sql_open") != NULL);

    arena_destroy(arena);
}

void test_codegen_actors_start_calls_runtime(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "actors.start(\"worker\")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_actor_start") != NULL);

    arena_destroy(arena);
}

/* ========== Tui Prompt Tests ========== */

void test_codegen_tui_prompt_input(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "Tui.Prompt.input(\"Name: \")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_prompt_input") != NULL);
    ASSERT_TRUE(strstr(qbe, "=l call") != NULL);

    arena_destroy(arena);
}

void test_codegen_tui_prompt_confirm(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "Tui.Prompt.confirm(\"Continue?\")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_prompt_confirm") != NULL);
    ASSERT_TRUE(strstr(qbe, "=w call") != NULL);

    arena_destroy(arena);
}

void test_codegen_tui_prompt_select(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena,
        "Tui.Prompt.select(\"Pick one\", [\"a\", \"b\"])");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_prompt_select") != NULL);
    ASSERT_TRUE(strstr(qbe, "=w call") != NULL);

    arena_destroy(arena);
}

void test_codegen_tui_prompt_password(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "Tui.Prompt.password(\"Password: \")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_prompt_password") != NULL);
    ASSERT_TRUE(strstr(qbe, "=l call") != NULL);

    arena_destroy(arena);
}

void test_codegen_tui_prompt_int(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "Tui.Prompt.int(\"Age\", 0, 120)");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_prompt_int") != NULL);
    ASSERT_TRUE(strstr(qbe, "=w call") != NULL);

    arena_destroy(arena);
}

/* ========== Record/Actor and Fallback Path Tests ========== */

void test_codegen_record_update_has_concrete_path(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "%{ user | age: 31 }");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "TODO: codegen for expr type") == NULL);

    arena_destroy(arena);
}

void test_codegen_spawn_calls_runtime(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "spawn(worker_loop)");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_actor_spawn") != NULL);

    arena_destroy(arena);
}

void test_codegen_send_calls_runtime(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "send(1, \"msg\")");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "$fern_actor_send") != NULL);

    arena_destroy(arena);
}

void test_codegen_receive_has_concrete_path(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "receive: Ping -> 1, Shutdown -> 2");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "TODO: codegen for expr type") == NULL);

    arena_destroy(arena);
}

void test_codegen_pipe_generic_call_supported(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "1 |> add(2)");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "TODO: generic pipe operator not yet supported") == NULL);
    ASSERT_TRUE(strstr(qbe, "call $add") != NULL);

    arena_destroy(arena);
}

void test_codegen_indirect_call_supported(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "((x) -> x + 1)(2)");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "TODO: indirect call") == NULL);
    ASSERT_TRUE(strstr(qbe, "call %") != NULL);

    arena_destroy(arena);
}

void test_codegen_named_field_access_no_todo(void) {
    Arena* arena = arena_create(8192);

    const char* qbe = generate_expr_qbe(arena, "point.x");

    ASSERT_NOT_NULL(qbe);
    ASSERT_TRUE(strstr(qbe, "TODO: named field access") == NULL);

    arena_destroy(arena);
}

/* ========== Test Runner ========== */

void run_codegen_tests(void) {
    printf("\n--- Code Generator Tests ---\n");
    
    /* Integer literals */
    TEST_RUN(test_codegen_int_literal);
    TEST_RUN(test_codegen_negative_int);
    
    /* Binary operations */
    TEST_RUN(test_codegen_add);
    TEST_RUN(test_codegen_sub);
    TEST_RUN(test_codegen_mul);
    TEST_RUN(test_codegen_div);
    TEST_RUN(test_codegen_complex_expr);
    
    /* Comparisons */
    TEST_RUN(test_codegen_eq);
    TEST_RUN(test_codegen_lt);
    
    /* Functions */
    TEST_RUN(test_codegen_fn_simple);
    TEST_RUN(test_codegen_fn_with_params);
    TEST_RUN(test_codegen_fn_call);
    
    /* Control flow */
    TEST_RUN(test_codegen_if_expr);
    
    /* Statements */
    TEST_RUN(test_codegen_let);
    
    /* Strings */
    TEST_RUN(test_codegen_string_literal);
    TEST_RUN(test_codegen_string_in_fn);
    
    /* Booleans */
    TEST_RUN(test_codegen_bool_true);
    TEST_RUN(test_codegen_bool_false);
    
    /* Float literals */
    TEST_RUN(test_codegen_float_literal);
    
    /* Match expressions */
    TEST_RUN(test_codegen_match_int);
    TEST_RUN(test_codegen_match_wildcard);
    
    /* Tuple expressions */
    TEST_RUN(test_codegen_tuple);
    
    /* List expressions */
    TEST_RUN(test_codegen_list);
    
    /* Lambda expressions */
    TEST_RUN(test_codegen_lambda);
    
    /* Try expression (? operator) */
    TEST_RUN(test_codegen_try_operator);
    TEST_RUN(test_codegen_try_in_chain);
    
    /* String runtime integration */
    TEST_RUN(test_codegen_string_concat);
    TEST_RUN(test_codegen_string_new);
    
    /* List runtime integration */
    TEST_RUN(test_codegen_list_new);
    TEST_RUN(test_codegen_list_index);
    
    /* Result type constructors */
    TEST_RUN(test_codegen_ok_constructor);
    TEST_RUN(test_codegen_err_constructor);
    
    /* For loops */
    TEST_RUN(test_codegen_for_loop);
    TEST_RUN(test_codegen_for_in_function);
    
    /* Defer statements */
    TEST_RUN(test_codegen_defer_simple);
    TEST_RUN(test_codegen_defer_multiple);
    
    /* With expressions */
    TEST_RUN(test_codegen_with_simple);
    TEST_RUN(test_codegen_with_multiple_bindings);
    TEST_RUN(test_codegen_with_no_else_propagates_error);
    TEST_RUN(test_codegen_with_else_arms);
    
    /* Pointer type handling */
    TEST_RUN(test_codegen_fn_returns_tuple);
    TEST_RUN(test_codegen_fn_returns_string);
    TEST_RUN(test_codegen_fn_string_param);
    TEST_RUN(test_codegen_fn_list_param);
    TEST_RUN(test_codegen_if_returns_string);
    TEST_RUN(test_codegen_dup_inserted_for_pointer_alias_binding);
    TEST_RUN(test_codegen_drop_inserted_for_unreturned_pointer_bindings);

    /* Lowercase stdlib API stabilization */
    TEST_RUN(test_codegen_fs_read_calls_runtime);
    TEST_RUN(test_codegen_json_parse_calls_runtime);
    TEST_RUN(test_codegen_http_get_calls_runtime);
    TEST_RUN(test_codegen_sql_open_calls_runtime);
    TEST_RUN(test_codegen_actors_start_calls_runtime);

    /* Tui.Prompt runtime calls */
    TEST_RUN(test_codegen_tui_prompt_input);
    TEST_RUN(test_codegen_tui_prompt_confirm);
    TEST_RUN(test_codegen_tui_prompt_select);
    TEST_RUN(test_codegen_tui_prompt_password);
    TEST_RUN(test_codegen_tui_prompt_int);

    /* Record/actor primitives and removed fallback TODO paths */
    TEST_RUN(test_codegen_record_update_has_concrete_path);
    TEST_RUN(test_codegen_spawn_calls_runtime);
    TEST_RUN(test_codegen_send_calls_runtime);
    TEST_RUN(test_codegen_receive_has_concrete_path);
    TEST_RUN(test_codegen_pipe_generic_call_supported);
    TEST_RUN(test_codegen_indirect_call_supported);
    TEST_RUN(test_codegen_named_field_access_no_todo);
}
