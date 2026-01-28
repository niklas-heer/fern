/* String Tests */

#include "test.h"
#include "arena.h"
#include "fern_string.h"

void test_string_new(void) {
    Arena* arena = arena_create(4096);
    
    String* s = string_new(arena, "Hello");
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ(string_cstr(s), "Hello");
    ASSERT_EQ(string_len(s), 5);
    
    arena_destroy(arena);
}

void test_string_empty(void) {
    Arena* arena = arena_create(4096);
    
    String* s = string_empty(arena);
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ(string_cstr(s), "");
    ASSERT_EQ(string_len(s), 0);
    ASSERT_TRUE(string_is_empty(s));
    
    arena_destroy(arena);
}

void test_string_concat(void) {
    Arena* arena = arena_create(4096);
    
    String* s1 = string_new(arena, "Hello");
    String* s2 = string_new(arena, " World");
    String* s3 = string_concat(arena, s1, s2);
    
    ASSERT_NOT_NULL(s3);
    ASSERT_STR_EQ(string_cstr(s3), "Hello World");
    ASSERT_EQ(string_len(s3), 11);
    
    arena_destroy(arena);
}

void test_string_equal(void) {
    Arena* arena = arena_create(4096);
    
    String* s1 = string_new(arena, "test");
    String* s2 = string_new(arena, "test");
    String* s3 = string_new(arena, "other");
    
    ASSERT_TRUE(string_equal(s1, s2));
    ASSERT_FALSE(string_equal(s1, s3));
    
    arena_destroy(arena);
}

void test_string_compare(void) {
    Arena* arena = arena_create(4096);
    
    String* s1 = string_new(arena, "apple");
    String* s2 = string_new(arena, "banana");
    String* s3 = string_new(arena, "apple");
    
    ASSERT_TRUE(string_compare(s1, s2) < 0);  // apple < banana
    ASSERT_TRUE(string_compare(s2, s1) > 0);  // banana > apple
    ASSERT_EQ(string_compare(s1, s3), 0);     // apple == apple
    
    arena_destroy(arena);
}

void test_string_format(void) {
    Arena* arena = arena_create(4096);
    
    String* s = string_format(arena, "Number: %d, String: %s", 42, "test");
    ASSERT_NOT_NULL(s);
    ASSERT_STR_EQ(string_cstr(s), "Number: 42, String: test");
    
    arena_destroy(arena);
}

void run_string_tests(void) {
    printf("\n=== String Tests ===\n");
    TEST_RUN(test_string_new);
    TEST_RUN(test_string_empty);
    TEST_RUN(test_string_concat);
    TEST_RUN(test_string_equal);
    TEST_RUN(test_string_compare);
    TEST_RUN(test_string_format);
}
