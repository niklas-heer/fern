/* Gate C Runtime Surface Integration Tests */

#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

typedef struct {
    int exit_code;
    char* output;
} CmdResult;

typedef struct {
    CmdResult build;
    CmdResult run;
} BuildRunResult;

typedef struct {
    pid_t pid;
    int port;
} TestHttpServer;

static const char* TEST_HTTPS_SERVER_SCRIPT = "tests/fixtures/runtime_https_server.py";
static const char* TEST_HTTPS_SERVER_CERT = "tests/fixtures/runtime_https_cert.pem";
static const char* TEST_HTTPS_SERVER_KEY = "tests/fixtures/runtime_https_key.pem";

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

static int parse_content_length(const char* request) {
    if (!request) return -1;
    const char* marker = strstr(request, "Content-Length:");
    if (!marker) return -1;
    marker += strlen("Content-Length:");
    while (*marker == ' ') marker++;
    return atoi(marker);
}

static int write_http_response(int fd, int status, const char* body, size_t body_len) {
    const char* status_text = status == 200 ? "OK" : "Not Found";
    char header[256];
    int header_len = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n",
        status,
        status_text,
        body_len
    );
    if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
        return -1;
    }
    if (write(fd, header, (size_t)header_len) < 0) return -1;
    if (body_len > 0 && write(fd, body, body_len) < 0) return -1;
    return 0;
}

static int run_test_http_server(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return 1;

    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(listen_fd);
        return 2;
    }
    if (listen(listen_fd, 8) != 0) {
        close(listen_fd);
        return 3;
    }

    for (int i = 0; i < 6; i++) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            close(listen_fd);
            return 4;
        }

        char req[8192];
        ssize_t nread = read(client_fd, req, sizeof(req) - 1);
        if (nread <= 0) {
            close(client_fd);
            continue;
        }
        req[nread] = '\0';

        const char* body_start = strstr(req, "\r\n\r\n");
        if (body_start) body_start += 4;

        if (strncmp(req, "GET /health ", 12) == 0) {
            write_http_response(client_fd, 200, "ok", 2);
        } else if (strncmp(req, "POST /echo ", 11) == 0 && body_start) {
            int body_len = parse_content_length(req);
            if (body_len < 0) {
                body_len = (int)strlen(body_start);
            }
            if (body_len < 0) body_len = 0;
            write_http_response(client_fd, 200, body_start, (size_t)body_len);
        } else {
            write_http_response(client_fd, 404, "not found", 9);
        }

        close(client_fd);
    }

    close(listen_fd);
    return 0;
}

static bool wait_for_http_server_ready(int port) {
    for (int i = 0; i < 50; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return false;

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t)port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        int rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
        close(fd);
        if (rc == 0) {
            return true;
        }
        usleep(20 * 1000);
    }
    return false;
}

static TestHttpServer start_test_http_server(int port) {
    TestHttpServer server;
    server.pid = -1;
    server.port = port;

    pid_t pid = fork();
    if (pid < 0) return server;

    if (pid == 0) {
        int code = run_test_http_server(port);
        _exit(code);
    }

    if (!wait_for_http_server_ready(port)) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return server;
    }

    server.pid = pid;
    return server;
}

static TestHttpServer start_test_https_server(int port) {
    TestHttpServer server;
    server.pid = -1;
    server.port = port;

    if (access(TEST_HTTPS_SERVER_SCRIPT, X_OK) != 0) return server;
    if (access(TEST_HTTPS_SERVER_CERT, R_OK) != 0) return server;
    if (access(TEST_HTTPS_SERVER_KEY, R_OK) != 0) return server;

    pid_t pid = fork();
    if (pid < 0) return server;

    if (pid == 0) {
        char port_str[32];
        snprintf(port_str, sizeof(port_str), "%d", port);
        execlp("python3",
               "python3",
               TEST_HTTPS_SERVER_SCRIPT,
               port_str,
               TEST_HTTPS_SERVER_CERT,
               TEST_HTTPS_SERVER_KEY,
               (char*)NULL);
        _exit(127);
    }

    if (!wait_for_http_server_ready(port)) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return server;
    }

    server.pid = pid;
    return server;
}

static void stop_test_http_server(TestHttpServer server) {
    if (server.pid <= 0) return;
    kill(server.pid, SIGTERM);
    waitpid(server.pid, NULL, 0);
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

    char cmd[1536];
    snprintf(
        cmd,
        sizeof(cmd),
        "just runtime-lib >/dev/null 2>&1 && "
        "./bin/fern build -o %s %s 2>&1",
        output_path,
        source_path
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
        "just runtime-lib >/dev/null 2>&1 && "
        "cc -std=c11 -Wall -Wextra -Werror -Iruntime "
        "%s bin/libfern_runtime.a "
        "$(pkg-config --libs bdw-gc 2>/dev/null || echo -lgc) "
        "$(pkg-config --libs sqlite3 2>/dev/null || echo -lsqlite3) "
        "$(pkg-config --libs openssl 2>/dev/null || echo -lssl -lcrypto) "
        "-pthread "
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

void test_runtime_http_get_returns_ok_body(void) {
    TestHttpServer server = start_test_http_server(19081);
    ASSERT_TRUE(server.pid > 0);

    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match http.get(\"http://127.0.0.1:19081/health\"):\n"
        "        Ok(body) -> if String.eq(body, \"ok\"): 0 else: 10\n"
        "        Err(_) -> 11\n");

    stop_test_http_server(server);
    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_http_post_returns_ok_body(void) {
    TestHttpServer server = start_test_http_server(19082);
    ASSERT_TRUE(server.pid > 0);

    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match http.post(\"http://127.0.0.1:19082/echo\", \"ping\"):\n"
        "        Ok(body) -> if String.eq(body, \"ping\"): 0 else: 10\n"
        "        Err(_) -> 11\n");

    stop_test_http_server(server);
    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_http_get_https_returns_ok_body(void) {
    TestHttpServer server = start_test_https_server(19083);
    ASSERT_TRUE(server.pid > 0);

    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match http.get(\"https://127.0.0.1:19083/health\"):\n"
        "        Ok(body) -> if String.eq(body, \"ok\"): 0 else: 10\n"
        "        Err(_) -> 11\n");

    stop_test_http_server(server);
    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_http_post_https_returns_ok_body(void) {
    TestHttpServer server = start_test_https_server(19084);
    ASSERT_TRUE(server.pid > 0);

    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match http.post(\"https://127.0.0.1:19084/echo\", \"ping\"):\n"
        "        Ok(body) -> if String.eq(body, \"ping\"): 0 else: 10\n"
        "        Err(_) -> 11\n");

    stop_test_http_server(server);
    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_http_post_invalid_url_returns_io_error(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match http.post(\"not-a-url\", \"payload\"):\n"
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

void test_runtime_sql_open_and_execute_returns_ok(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match sql.open(\"/tmp/fern_runtime_sql_surface.db\"):\n"
        "        Ok(handle) ->\n"
        "            match sql.execute(handle, \"DROP TABLE IF EXISTS users\"):\n"
        "                Ok(_) ->\n"
        "                    match sql.execute(handle, \"CREATE TABLE users(id INTEGER PRIMARY KEY, name TEXT)\"):\n"
        "                        Ok(_) ->\n"
        "                            match sql.execute(handle, \"INSERT INTO users(name) VALUES ('fern')\"):\n"
        "                                Ok(changed) -> if changed == 1: 0 else: 12\n"
        "                                Err(_) -> 13\n"
        "                        Err(_) -> 14\n"
        "                Err(_) -> 15\n"
        "        Err(_) -> 16\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_sql_execute_invalid_handle_returns_io_error(void) {
    BuildRunResult result = build_and_run_source(
        "fn main() -> Int:\n"
        "    match sql.execute(424242, \"select 1\"):\n"
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

void test_runtime_actors_post_and_next_mailbox_contract(void) {
    BuildRunResult result = build_and_run_source(
        "fn post_status(pid: Int) -> Int:\n"
        "    match actors.post(pid, \"ping\"):\n"
        "        Ok(sent) -> sent\n"
        "        Err(_) -> -1\n"
        "\n"
        "fn post_status_2(pid: Int) -> Int:\n"
        "    match actors.post(pid, \"pong\"):\n"
        "        Ok(sent) -> sent\n"
        "        Err(_) -> -1\n"
        "\n"
        "fn first_msg_status(pid: Int) -> Int:\n"
        "    match actors.next(pid):\n"
        "        Ok(msg) -> if String.eq(msg, \"ping\"): 0 else: 41\n"
        "        Err(_) -> 42\n"
        "\n"
        "fn second_msg_status(pid: Int) -> Int:\n"
        "    match actors.next(pid):\n"
        "        Ok(msg) -> if String.eq(msg, \"pong\"): 0 else: 43\n"
        "        Err(_) -> 44\n"
        "\n"
        "fn third_msg_status(pid: Int) -> Int:\n"
        "    match actors.next(pid):\n"
        "        Ok(_) -> 45\n"
        "        Err(code) -> code\n"
        "\n"
        "fn main() -> Int:\n"
        "    let pid = actors.start(\"worker\")\n"
        "    let sent1 = post_status(pid)\n"
        "    let sent2 = post_status_2(pid)\n"
        "    let first = first_msg_status(pid)\n"
        "    let second = second_msg_status(pid)\n"
        "    let third = third_msg_status(pid)\n"
        "    if sent1 == 0 and sent2 == 0 and first == 0 and second == 0 and third == 3: 0 else: 1\n");

    ASSERT_EQ(result.build.exit_code, 0);
    ASSERT_EQ(result.run.exit_code, 0);

    free_build_run_result(&result);
}

void test_runtime_actor_scheduler_round_robin_contract(void) {
    BuildRunResult result = build_and_run_c_source(
        "#include <stdint.h>\n"
        "#include \"fern_runtime.h\"\n"
        "\n"
        "int fern_main(void) {\n"
        "    int64_t a = fern_actor_spawn(\"a\");\n"
        "    int64_t b = fern_actor_spawn(\"b\");\n"
        "    if (a <= 0 || b <= a) return 60;\n"
        "\n"
        "    int64_t r1 = fern_actor_send(a, \"a1\");\n"
        "    int64_t r2 = fern_actor_send(a, \"a2\");\n"
        "    int64_t r3 = fern_actor_send(b, \"b1\");\n"
        "    if (!fern_result_is_ok(r1) || !fern_result_is_ok(r2) || !fern_result_is_ok(r3)) return 61;\n"
        "\n"
        "    if (fern_actor_mailbox_len(a) != 2) return 62;\n"
        "    if (fern_actor_mailbox_len(b) != 1) return 63;\n"
        "\n"
        "    int64_t first = fern_actor_scheduler_next();\n"
        "    int64_t second = fern_actor_scheduler_next();\n"
        "    int64_t third = fern_actor_scheduler_next();\n"
        "    int64_t fourth = fern_actor_scheduler_next();\n"
        "    if (first != a) return 64;\n"
        "    if (second != b) return 65;\n"
        "    if (third != a) return 66;\n"
        "    if (fourth != 0) return 67;\n"
        "\n"
        "    int64_t m1 = fern_actor_receive(a);\n"
        "    int64_t m2 = fern_actor_receive(b);\n"
        "    int64_t m3 = fern_actor_receive(a);\n"
        "    int64_t m4 = fern_actor_receive(a);\n"
        "    if (!fern_result_is_ok(m1)) return 68;\n"
        "    if (!fern_result_is_ok(m2)) return 69;\n"
        "    if (!fern_result_is_ok(m3)) return 70;\n"
        "    if (fern_result_is_ok(m4)) return 71;\n"
        "\n"
        "    const char* s1 = (const char*)(intptr_t)fern_result_unwrap(m1);\n"
        "    const char* s2 = (const char*)(intptr_t)fern_result_unwrap(m2);\n"
        "    const char* s3 = (const char*)(intptr_t)fern_result_unwrap(m3);\n"
        "    if (!fern_str_eq(s1, \"a1\")) return 72;\n"
        "    if (!fern_str_eq(s2, \"b1\")) return 73;\n"
        "    if (!fern_str_eq(s3, \"a2\")) return 74;\n"
        "\n"
        "    return 0;\n"
        "}\n");

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

void test_runtime_rc_header_and_core_type_ops(void) {
    BuildRunResult result = build_and_run_c_source(
        "#include <stdint.h>\n"
        "#include \"fern_runtime.h\"\n"
        "\n"
        "int fern_main(void) {\n"
        "    int64_t* payload = (int64_t*)fern_rc_alloc(sizeof(int64_t), FERN_RC_TYPE_TUPLE);\n"
        "    if (payload == NULL) return 20;\n"
        "    if (fern_rc_refcount(payload) != 1) return 21;\n"
        "    if (fern_rc_type_tag(payload) != FERN_RC_TYPE_TUPLE) return 22;\n"
        "\n"
        "    fern_rc_set_flags(payload, FERN_RC_FLAG_UNIQUE);\n"
        "    if (fern_rc_flags(payload) != FERN_RC_FLAG_UNIQUE) return 23;\n"
        "\n"
        "    if (fern_rc_dup(NULL) != NULL) return 24;\n"
        "    fern_rc_drop(NULL);\n"
        "\n"
        "    int64_t* alias = (int64_t*)fern_rc_dup(payload);\n"
        "    if (alias != payload) return 25;\n"
        "    if (fern_rc_refcount(payload) != 2) return 26;\n"
        "\n"
        "    fern_rc_drop(alias);\n"
        "    if (fern_rc_refcount(payload) != 1) return 27;\n"
        "    fern_rc_drop(payload);\n"
        "    if (fern_rc_refcount(payload) != 0) return 28;\n"
        "\n"
        "    int64_t result_ptr = fern_result_ok(42);\n"
        "    if (fern_rc_type_tag((void*)(intptr_t)result_ptr) != FERN_RC_TYPE_RESULT) return 29;\n"
        "\n"
        "    FernList* list = fern_list_new();\n"
        "    if (list == NULL) return 30;\n"
        "    if (fern_rc_type_tag(list) != FERN_RC_TYPE_LIST) return 31;\n"
        "    if (fern_rc_refcount(list) != 1) return 32;\n"
        "\n"
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
    TEST_RUN(test_runtime_http_get_returns_ok_body);
    TEST_RUN(test_runtime_http_post_returns_ok_body);
    TEST_RUN(test_runtime_http_get_https_returns_ok_body);
    TEST_RUN(test_runtime_http_post_https_returns_ok_body);
    TEST_RUN(test_runtime_http_post_invalid_url_returns_io_error);
    TEST_RUN(test_runtime_sql_open_empty_returns_io_error);
    TEST_RUN(test_runtime_sql_open_and_execute_returns_ok);
    TEST_RUN(test_runtime_sql_execute_invalid_handle_returns_io_error);
    TEST_RUN(test_runtime_actors_start_returns_monotonic_ids);
    TEST_RUN(test_runtime_actors_post_and_next_mailbox_contract);
    TEST_RUN(test_runtime_actor_scheduler_round_robin_contract);
    TEST_RUN(test_runtime_memory_alloc_dup_drop_contract);
    TEST_RUN(test_runtime_rc_header_and_core_type_ops);
}
