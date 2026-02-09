/* Release Automation Integration Tests */

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

void test_release_automation_files_exist(void) {
    ASSERT_EQ(access(".github/workflows/release-please.yml", F_OK), 0);
    ASSERT_EQ(access(".github/release-please-config.json", F_OK), 0);
    ASSERT_EQ(access(".github/.release-please-manifest.json", F_OK), 0);
}

void test_release_automation_workflow_uses_release_please(void) {
    char* workflow = read_file_text(".github/workflows/release-please.yml");
    ASSERT_NOT_NULL(workflow);
    ASSERT_TRUE(strstr(workflow, "googleapis/release-please-action") != NULL);
    ASSERT_TRUE(strstr(workflow, "branches: [main]") != NULL);
    ASSERT_TRUE(strstr(workflow, "pull-requests: write") != NULL);
    ASSERT_TRUE(strstr(workflow, "contents: write") != NULL);
    free(workflow);
}

void test_release_automation_config_tracks_version_header(void) {
    char* config = read_file_text(".github/release-please-config.json");
    ASSERT_NOT_NULL(config);
    ASSERT_TRUE(strstr(config, "\"release-type\": \"simple\"") != NULL);
    ASSERT_TRUE(strstr(config, "\"initial-version\": \"0.1.0\"") != NULL);
    ASSERT_TRUE(strstr(config, "\"bump-minor-pre-major\": true") != NULL);
    ASSERT_TRUE(strstr(config, "\"include-v-in-tag\": true") != NULL);
    ASSERT_TRUE(strstr(config, "\"include/version.h\"") != NULL);
    free(config);

    char* version_header = read_file_text("include/version.h");
    ASSERT_NOT_NULL(version_header);
    ASSERT_TRUE(strstr(version_header, "x-release-please-version") != NULL);
    free(version_header);
}

void test_ci_workflow_runs_docs_consistency_check(void) {
    char* workflow = read_file_text(".github/workflows/ci.yml");
    ASSERT_NOT_NULL(workflow);
    ASSERT_TRUE(strstr(workflow, "name: Check docs consistency") != NULL);
    ASSERT_TRUE(strstr(workflow, "run: just docs-consistency") != NULL);
    free(workflow);
}

void run_release_automation_tests(void) {
    printf("\n=== Release Automation Tests ===\n");
    TEST_RUN(test_release_automation_files_exist);
    TEST_RUN(test_release_automation_workflow_uses_release_please);
    TEST_RUN(test_release_automation_config_tracks_version_header);
    TEST_RUN(test_ci_workflow_runs_docs_consistency_check);
}
