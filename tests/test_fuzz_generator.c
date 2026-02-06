/* FernFuzz generator coverage tests */

#include "test.h"
#include "arena.h"
#include "parser.h"
#include "fuzz/fuzz_generator.h"

#include <stdlib.h>
#include <string.h>

static void assert_parses(const char* source) {
    Arena* arena = arena_create(16384);
    Parser* parser = parser_new(arena, source);
    StmtVec* stmts = parse_stmts(parser);

    ASSERT_NOT_NULL(stmts);
    ASSERT_FALSE(parser_had_error(parser));

    arena_destroy(arena);
}

void test_fuzz_generator_is_deterministic_per_seed_and_case(void) {
    char* a = fuzz_generate_program(0xBEEF, 7);
    char* b = fuzz_generate_program(0xBEEF, 7);
    char* c = fuzz_generate_program(0xBEEF, 8);

    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NOT_NULL(c);

    ASSERT_STR_EQ(a, b);
    ASSERT_TRUE(strcmp(a, c) != 0);

    free(a);
    free(b);
    free(c);
}

void test_fuzz_generator_emits_if_construct(void) {
    char* source = fuzz_generate_program(0xCAFE, 1);

    ASSERT_NOT_NULL(source);
    ASSERT_TRUE(strstr(source, "if ") != NULL);
    ASSERT_TRUE(strstr(source, "else") != NULL);
    assert_parses(source);

    free(source);
}

void test_fuzz_generator_emits_match_construct(void) {
    char* source = fuzz_generate_program(0xCAFE, 2);

    ASSERT_NOT_NULL(source);
    ASSERT_TRUE(strstr(source, "match ") != NULL);
    ASSERT_TRUE(strstr(source, "->") != NULL);
    assert_parses(source);

    free(source);
}

void test_fuzz_generator_emits_with_construct(void) {
    char* source = fuzz_generate_program(0xCAFE, 3);

    ASSERT_NOT_NULL(source);
    ASSERT_TRUE(strstr(source, "with ") != NULL);
    ASSERT_TRUE(strstr(source, " do ") != NULL);
    assert_parses(source);

    free(source);
}

void test_fuzz_generator_emits_typed_signature(void) {
    char* source = fuzz_generate_program(0xCAFE, 4);

    ASSERT_NOT_NULL(source);
    ASSERT_TRUE(strstr(source, "fn ") != NULL);
    ASSERT_TRUE(strstr(source, "->") != NULL);
    ASSERT_TRUE(strstr(source, ": Int") != NULL);
    assert_parses(source);

    free(source);
}

void test_fuzz_seed_corpus_programs_parse(void) {
    size_t count = fuzz_seed_program_count();
    ASSERT_TRUE(count >= 4);

    for (size_t i = 0; i < count; i++) {
        char* source = fuzz_load_seed_program(i);
        ASSERT_NOT_NULL(source);
        assert_parses(source);
        free(source);
    }
}

void run_fuzz_generator_tests(void) {
    printf("\n=== FernFuzz Generator Tests ===\n");
    TEST_RUN(test_fuzz_generator_is_deterministic_per_seed_and_case);
    TEST_RUN(test_fuzz_generator_emits_if_construct);
    TEST_RUN(test_fuzz_generator_emits_match_construct);
    TEST_RUN(test_fuzz_generator_emits_with_construct);
    TEST_RUN(test_fuzz_generator_emits_typed_signature);
    TEST_RUN(test_fuzz_seed_corpus_programs_parse);
}
