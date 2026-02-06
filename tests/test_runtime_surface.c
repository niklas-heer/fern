/* Gate C Runtime Surface Integration Tests */

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    int exit_code;
    char* output;
} CmdResult;

typedef struct {
    CmdResult build;
    CmdResult run;
} BuildRunResult;

static char* dup_cstr(const char* text) {
    if (!text) return NULL;
    size_t len = strlen(text);
    char* copy = (char*)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static char* read_pipe_all(FILE* pipe) {
    if (!pipe) return NULL;

    size_t cap = 1024;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    int ch = 0;
    while ((ch = fgetc(pipe)) != EOF) {
        if (len + 1 >= cap) {
            size_t next_cap = cap * 2;
            char* next = (char*)realloc(buf, next_cap);
            if (!next) {
                free(buf);
                return NULL;
            }
            buf = next;
            cap = next_cap;
        }
        buf[len++] = (char)ch;
    }
    buf[len] = '\0';
    return buf;
}

static CmdResult run_cmd(const char* cmd) {
    CmdResult result;
    result.exit_code = -1;
    result.output = NULL;

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        return result;
    }

    result.output = read_pipe_all(pipe);
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }

    return result;
}

static char* write_tmp_source(const char* source) {
    char tmpl[] = "/tmp/fern_runtime_surface_XXXXXX.fn";
    int fd = mkstemps(tmpl, 3);
    if (fd < 0) return NULL;

    FILE* file = fdopen(fd, "w");
    if (!file) {
        close(fd);
        unlink(tmpl);
        return NULL;
    }

    fputs(source, file);
    fclose(file);

    return dup_cstr(tmpl);
}

static char* write_tmp_c_source(const char* source) {
    char tmpl[] = "/tmp/fern_runtime_surface_XXXXXX.c";
    int fd = mkstemps(tmpl, 2);
    if (fd < 0) return NULL;

    FILE* file = fdopen(fd, "w");
    if (!file) {
        close(fd);
        unlink(tmpl);
        return NULL;
    }

    fputs(source, file);
    fclose(file);

    return dup_cstr(tmpl);
}

static char* make_tmp_output_path(void) {
    char tmpl[] = "/tmp/fern_runtime_surface_out_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd < 0) return NULL;
    close(fd);
    unlink(tmpl);
    return dup_cstr(tmpl);
}

static void free_cmd_result(CmdResult* result) {
    if (!result) return;
    free(result->output);
    result->output = NULL;
}

static void free_build_run_result(BuildRunResult* result) {
    if (!result) return;
    free_cmd_result(&result->build);
    free_cmd_result(&result->run);
}

static BuildRunResult build_and_run_source(const char* source) {
    BuildRunResult result;
    result.build.exit_code = -1;
    result.build.output = NULL;
    result.run.exit_code = -1;
    result.run.output = NULL;

    char* source_path = write_tmp_source(source);
    if (!source_path) {
        result.build.output = dup_cstr("failed to create temporary source");
        return result;
    }

    char* output_path = make_tmp_output_path();
    if (!output_path) {
        result.build.output = dup_cstr("failed to create temporary output path");
        unlink(source_path);
        free(source_path);
        return result;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "./bin/fern build -o %s %s 2>&1", output_path, source_path);
    result.build = run_cmd(cmd);
    if (result.build.exit_code == 0) {
        snprintf(cmd, sizeof(cmd), "%s 2>&1", output_path);
        result.run = run_cmd(cmd);
    }

    unlink(source_path);
    unlink(output_path);
    free(source_path);
    free(output_path);
    return result;
}

static BuildRunResult build_and_run_c_source(const char* source) {
    BuildRunResult result;
    result.build.exit_code = -1;
    result.build.output = NULL;
    result.run.exit_code = -1;
    result.run.output = NULL;

    char* source_path = write_tmp_c_source(source);
    if (!source_path) {
        result.build.output = dup_cstr("failed to create temporary C source");
        return result;
    }

    char* output_path = make_tmp_output_path();
    if (!output_path) {
        result.build.output = dup_cstr("failed to create temporary C output path");
        unlink(source_path);
        free(source_path);
        return result;
    }

    char cmd[1536];
    snprintf(
        cmd,
        sizeof(cmd),
        "make bin/libfern_runtime.a >/dev/null 2>&1 && "
        "cc -std=c11 -Wall -Wextra -Werror -Iruntime "
        "%s bin/libfern_runtime.a "
        "$(pkg-config --libs bdw-gc 2>/dev/null || echo -lgc) "
        "-o %s 2>&1",
        source_path,
        output_path
    );
    result.build = run_cmd(cmd);
    if (result.build.exit_code == 0) {
        snprintf(cmd, sizeof(cmd), "%s 2>&1", output_path);
        result.run = run_cmd(cmd);
    }

    unlink(source_path);
    unlink(output_path);
    free(source_path);
    free(output_path);
    return result;
}

void test_runtime_json_parse_empty_returns_err_code(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match json.parse(\"\"):\n"
        "        Ok(_) -> 10\n"
        "        Err(code) -> code\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 3);

    free_build_run_result(&result);
}

void test_runtime_json_stringify_empty_returns_ok_empty(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match json.stringify(\"\"):\n"
        "        Ok(text) -> if String.eq(text, \"\"): 0 else: 11\n"
        "        Err(_) -> 12\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_http_get_empty_returns_io_error(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match http.get(\"\"):\n"
        "        Ok(_) -> 10\n"
        "        Err(code) -> if code == 3: 0 else: 11\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_http_post_placeholder_returns_io_error(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match http.post(\"https://example.com\", \"payload\"):\n"
        "        Ok(_) -> 10\n"
        "        Err(code) -> if code == 3: 0 else: 11\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_sql_open_empty_returns_io_error(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match sql.open(\"\"):\n"
        "        Ok(_) -> 10\n"
        "        Err(code) -> if code == 3: 0 else: 11\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_sql_execute_placeholder_returns_io_error(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match sql.execute(1, \"select 1\"):\n"
        "        Ok(_) -> 10\n"
        "        Err(code) -> if code == 3: 0 else: 11\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_actors_start_returns_monotonic_ids(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    let first = actors.start(\"worker\")\n"
        "    let second = actors.start(\"worker\")\n"
        "    if second > first: 0 else: 1\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_actors_post_and_next_placeholder_contract(void) {
    BuildRunResult result = build_and_run_source(
        "fn post_status(pid: Int) -> Int:\n"
        "    match actors.post(pid, \"ping\"):\n"
        "        Ok(sent) -> sent\n"
        "        Err(_) -> -1\n"
        "\n"
        "fn next_status(pid: Int) -> Int:\n"
        "    match actors.next(pid):\n"
        "        Ok(_) -> -1\n"
        "        Err(code) -> code\n"
        "\n"
        "fn main() -> Int:\n"
        "    let pid = actors.start(\"worker\")\n"
        "    let sent = post_status(pid)\n"
        "    let next_code = next_status(pid)\n"
        "    if sent == 0 and next_code == 3: 0 else: 1\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_memory_alloc_dup_drop_contract(void) {
    BuildRunResult result = build_and_run_c_source(
        "#include <stdint.h>\n"
        "#include \"fern_runtime.h\"\n"
        "\n"
        "int fern_main(void) {\n"
        "    int64_t* values = (int64_t*)fern_alloc(sizeof(int64_t) * 2);\n"
        "    if (values == NULL) return 10;\n"
        "\n"
        "    values[0] = 41;\n"
        "    values[1] = 0;\n"
        "\n"
        "    if (fern_dup(NULL) != NULL) return 11;\n"
        "    fern_drop(NULL);\n"
        "\n"
        "    int64_t* alias = (int64_t*)fern_dup(values);\n"
        "    if (alias != values) return 12;\n"
        "    alias[1] = 1;\n"
        "    if (values[0] + values[1] != 42) return 13;\n"
        "\n"
        "    fern_drop(alias);\n"
        "    fern_drop(values);\n"
        "    return 0;\n"
        "}\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void run_runtime_surface_tests(void) {
    printf("\n=== Runtime Surface Tests ===\n");
    TEST_RUN(test_runtime_json_parse_empty_returns_err_code);
    TEST_RUN(test_runtime_json_stringify_empty_returns_ok_empty);
    TEST_RUN(test_runtime_http_get_empty_returns_io_error);
    TEST_RUN(test_runtime_http_post_placeholder_returns_io_error);
    TEST_RUN(test_runtime_sql_open_empty_returns_io_error);
    TEST_RUN(test_runtime_sql_execute_placeholder_returns_io_error);
    TEST_RUN(test_runtime_actors_start_returns_monotonic_ids);
    TEST_RUN(test_runtime_actors_post_and_next_placeholder_contract);
    TEST_RUN(test_runtime_memory_alloc_dup_drop_contract);
}
