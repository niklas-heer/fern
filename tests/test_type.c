/* Type System Tests */

#include "test.h"
#include "type.h"
#include "type_env.h"
#include "arena.h"
#include "fern_string.h"

/* ========== Type Creation Tests ========== */

void test_type_primitives(void) {
    Arena* arena = arena_create(4096);
    
    Type* t_int = type_int(arena);
    Type* t_float = type_float(arena);
    Type* t_string = type_string(arena);
    Type* t_bool = type_bool(arena);
    Type* t_unit = type_unit(arena);
    
    ASSERT_EQ(t_int->kind, TYPE_INT);
    ASSERT_EQ(t_float->kind, TYPE_FLOAT);
    ASSERT_EQ(t_string->kind, TYPE_STRING);
    ASSERT_EQ(t_bool->kind, TYPE_BOOL);
    ASSERT_EQ(t_unit->kind, TYPE_UNIT);
    
    arena_destroy(arena);
}

void test_type_equals_primitives(void) {
    Arena* arena = arena_create(4096);
    
    ASSERT_TRUE(type_equals(type_int(arena), type_int(arena)));
    ASSERT_TRUE(type_equals(type_string(arena), type_string(arena)));
    ASSERT_FALSE(type_equals(type_int(arena), type_string(arena)));
    ASSERT_FALSE(type_equals(type_int(arena), type_float(arena)));
    
    arena_destroy(arena);
}

void test_type_list(void) {
    Arena* arena = arena_create(4096);
    
    Type* list_int = type_list(arena, type_int(arena));
    
    ASSERT_EQ(list_int->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(list_int->data.con.name), "List");
    ASSERT_EQ(list_int->data.con.args->len, 1);
    ASSERT_EQ(list_int->data.con.args->data[0]->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_type_result(void) {
    Arena* arena = arena_create(4096);
    
    Type* result_type = type_result(arena, type_int(arena), type_string(arena));
    
    ASSERT_EQ(result_type->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(result_type->data.con.name), "Result");
    ASSERT_EQ(result_type->data.con.args->len, 2);
    ASSERT_EQ(result_type->data.con.args->data[0]->kind, TYPE_INT);
    ASSERT_EQ(result_type->data.con.args->data[1]->kind, TYPE_STRING);
    
    arena_destroy(arena);
}

void test_type_fn(void) {
    Arena* arena = arena_create(4096);
    
    // (Int, String) -> Bool
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_int(arena));
    TypeVec_push(arena, params, type_string(arena));
    Type* fn = type_fn(arena, params, type_bool(arena));
    
    ASSERT_EQ(fn->kind, TYPE_FN);
    ASSERT_EQ(fn->data.fn.params->len, 2);
    ASSERT_EQ(fn->data.fn.params->data[0]->kind, TYPE_INT);
    ASSERT_EQ(fn->data.fn.params->data[1]->kind, TYPE_STRING);
    ASSERT_EQ(fn->data.fn.result->kind, TYPE_BOOL);
    
    arena_destroy(arena);
}

void test_type_to_string(void) {
    Arena* arena = arena_create(4096);
    
    ASSERT_STR_EQ(string_cstr(type_to_string(arena, type_int(arena))), "Int");
    ASSERT_STR_EQ(string_cstr(type_to_string(arena, type_bool(arena))), "Bool");
    ASSERT_STR_EQ(string_cstr(type_to_string(arena, type_unit(arena))), "()");
    
    Type* list_int = type_list(arena, type_int(arena));
    ASSERT_STR_EQ(string_cstr(type_to_string(arena, list_int)), "List(Int)");
    
    arena_destroy(arena);
}

void test_type_is_result(void) {
    Arena* arena = arena_create(4096);
    
    Type* result_type = type_result(arena, type_int(arena), type_string(arena));
    Type* option_type = type_option(arena, type_int(arena));
    
    ASSERT_TRUE(type_is_result(result_type));
    ASSERT_FALSE(type_is_result(option_type));
    ASSERT_FALSE(type_is_result(type_int(arena)));
    
    arena_destroy(arena);
}

/* ========== Type Environment Tests ========== */

void test_type_env_create(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    
    ASSERT_NOT_NULL(env);
    ASSERT_EQ(type_env_depth(env), 0);
    
    arena_destroy(arena);
}

void test_type_env_define_lookup(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* name = string_new(arena, "x");
    Type* t = type_int(arena);
    
    type_env_define(env, name, t);
    Type* found = type_env_lookup(env, name);
    
    ASSERT_NOT_NULL(found);
    ASSERT_TRUE(type_equals(found, t));
    
    arena_destroy(arena);
}

void test_type_env_lookup_not_found(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* name = string_new(arena, "undefined");
    
    Type* found = type_env_lookup(env, name);
    
    ASSERT_NULL(found);
    
    arena_destroy(arena);
}

void test_type_env_scope_push_pop(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* x = string_new(arena, "x");
    
    // Define x in outer scope
    type_env_define(env, x, type_int(arena));
    ASSERT_EQ(type_env_depth(env), 0);
    
    // Push new scope
    type_env_push_scope(env);
    ASSERT_EQ(type_env_depth(env), 1);
    
    // x should still be visible
    ASSERT_NOT_NULL(type_env_lookup(env, x));
    ASSERT_EQ(type_env_lookup(env, x)->kind, TYPE_INT);
    
    // Define x in inner scope (shadows outer)
    type_env_define(env, x, type_string(arena));
    ASSERT_EQ(type_env_lookup(env, x)->kind, TYPE_STRING);
    
    // Pop scope - x should be Int again
    type_env_pop_scope(env);
    ASSERT_EQ(type_env_depth(env), 0);
    ASSERT_EQ(type_env_lookup(env, x)->kind, TYPE_INT);
    
    arena_destroy(arena);
}

void test_type_env_nested_scopes(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* a = string_new(arena, "a");
    String* b = string_new(arena, "b");
    String* c = string_new(arena, "c");
    
    // Global scope: define a
    type_env_define(env, a, type_int(arena));
    
    // Scope 1: define b
    type_env_push_scope(env);
    type_env_define(env, b, type_string(arena));
    
    // Scope 2: define c
    type_env_push_scope(env);
    type_env_define(env, c, type_bool(arena));
    
    // All visible in innermost scope
    ASSERT_NOT_NULL(type_env_lookup(env, a));
    ASSERT_NOT_NULL(type_env_lookup(env, b));
    ASSERT_NOT_NULL(type_env_lookup(env, c));
    
    // Pop to scope 1
    type_env_pop_scope(env);
    ASSERT_NOT_NULL(type_env_lookup(env, a));
    ASSERT_NOT_NULL(type_env_lookup(env, b));
    ASSERT_NULL(type_env_lookup(env, c));  // c no longer visible
    
    // Pop to global
    type_env_pop_scope(env);
    ASSERT_NOT_NULL(type_env_lookup(env, a));
    ASSERT_NULL(type_env_lookup(env, b));  // b no longer visible
    ASSERT_NULL(type_env_lookup(env, c));
    
    arena_destroy(arena);
}

void test_type_env_define_function(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* fn_name = string_new(arena, "add");
    
    // (Int, Int) -> Int
    TypeVec* params = TypeVec_new(arena);
    TypeVec_push(arena, params, type_int(arena));
    TypeVec_push(arena, params, type_int(arena));
    Type* fn_type = type_fn(arena, params, type_int(arena));
    
    type_env_define(env, fn_name, fn_type);
    
    Type* found = type_env_lookup(env, fn_name);
    ASSERT_NOT_NULL(found);
    ASSERT_EQ(found->kind, TYPE_FN);
    ASSERT_EQ(found->data.fn.params->len, 2);
    
    arena_destroy(arena);
}

void test_type_env_define_type(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* type_name = string_new(arena, "UserId");
    
    // UserId is a newtype wrapper around Int
    Type* user_id_type = type_con(arena, type_name, NULL);
    
    type_env_define_type(env, type_name, user_id_type);
    
    Type* found = type_env_lookup_type(env, type_name);
    ASSERT_NOT_NULL(found);
    ASSERT_EQ(found->kind, TYPE_CON);
    ASSERT_STR_EQ(string_cstr(found->data.con.name), "UserId");
    
    arena_destroy(arena);
}

void test_type_env_is_defined(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* x = string_new(arena, "x");
    String* y = string_new(arena, "y");
    
    type_env_define(env, x, type_int(arena));
    
    ASSERT_TRUE(type_env_is_defined(env, x));
    ASSERT_FALSE(type_env_is_defined(env, y));
    
    arena_destroy(arena);
}

void test_type_env_is_defined_in_current_scope(void) {
    Arena* arena = arena_create(4096);
    
    TypeEnv* env = type_env_new(arena);
    String* x = string_new(arena, "x");
    
    // Define in global scope
    type_env_define(env, x, type_int(arena));
    ASSERT_TRUE(type_env_is_defined_in_current_scope(env, x));
    
    // Push new scope
    type_env_push_scope(env);
    
    // x is defined globally but NOT in current scope
    ASSERT_TRUE(type_env_is_defined(env, x));
    ASSERT_FALSE(type_env_is_defined_in_current_scope(env, x));
    
    // Define x in current scope
    type_env_define(env, x, type_string(arena));
    ASSERT_TRUE(type_env_is_defined_in_current_scope(env, x));
    
    arena_destroy(arena);
}

/* ========== Test Runner ========== */

void run_type_tests(void) {
    printf("\n--- Type Tests ---\n");
    
    // Type creation tests
    TEST_RUN(test_type_primitives);
    TEST_RUN(test_type_equals_primitives);
    TEST_RUN(test_type_list);
    TEST_RUN(test_type_result);
    TEST_RUN(test_type_fn);
    TEST_RUN(test_type_to_string);
    TEST_RUN(test_type_is_result);
    
    // Type environment tests
    TEST_RUN(test_type_env_create);
    TEST_RUN(test_type_env_define_lookup);
    TEST_RUN(test_type_env_lookup_not_found);
    TEST_RUN(test_type_env_scope_push_pop);
    TEST_RUN(test_type_env_nested_scopes);
    TEST_RUN(test_type_env_define_function);
    TEST_RUN(test_type_env_define_type);
    TEST_RUN(test_type_env_is_defined);
    TEST_RUN(test_type_env_is_defined_in_current_scope);
}
