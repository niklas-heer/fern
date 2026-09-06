/* Mise Task Integration Tests */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char* read_file_text(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) != 0) return NULL;

    FILE* file = fopen(path, "rb");
    if (!file) return NULL;

    char* text = (char*)malloc((size_t)st.st_size + 1);
    if (!text) {
        fclose(file);
        return NULL;
    }

    size_t read_n = fread(text, 1, (size_t)st.st_size, file);
    fclose(file);
    text[read_n] = '\0';
    return text;
}

void test_mise_exists(void) {
    ASSERT_EQ(access("mise.toml", F_OK), 0);
}

void test_mise_exposes_core_recipes(void) {
    char* config = read_file_text("mise.toml");
    ASSERT_NOT_NULL(config);

    ASSERT_TRUE(strstr(config, "[tasks.debug]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.release]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.test]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.check]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.docs]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.docs-check]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.docs-consistency]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.release-package]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.benchmark-report]") != NULL);

    ASSERT_TRUE(strstr(config, "[tasks._build-fern]") != NULL);
    ASSERT_TRUE(strstr(config, "[tasks.runtime-lib]") != NULL);

    free(config);
}

void test_release_package_check_uses_dist_staging_layout(void) {
    char* config = read_file_text("mise.toml");
    ASSERT_NOT_NULL(config);

    ASSERT_TRUE(strstr(config, "[tasks.release-package-check]") != NULL);
    ASSERT_TRUE(strstr(config, "verify-layout --staging dist/staging") != NULL);

    free(config);
}

void test_docs_check_runs_docs_consistency_gate(void) {
    char* config = read_file_text("mise.toml");
    ASSERT_NOT_NULL(config);

    ASSERT_TRUE(strstr(config, "[tasks.docs-check]") != NULL);
    ASSERT_TRUE(strstr(config, "mise run docs-consistency") != NULL);

    free(config);
}

void run_mise_tests(void) {
    printf("\n=== mise.toml Tests ===\n");
    TEST_RUN(test_mise_exists);
    TEST_RUN(test_mise_exposes_core_recipes);
    TEST_RUN(test_release_package_check_uses_dist_staging_layout);
    TEST_RUN(test_docs_check_runs_docs_consistency_gate);
}
