/** Native literal-argv process regression fixture. */
#include "fern_runtime.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/** Call literal argv with a stack-owned list; argv and each argument are valid strings. */
static FernExecResult* execute(char** argv, int64_t count) {
    assert(argv != NULL);
    assert(count >= 0);
    FernStringList list = {.data = argv, .len = count, .cap = count};
    return fern_exec_args(&list);
}

/** Run one requested process contract and report failures through the executable exit code. */
int fern_main(void) {
    assert(fern_args_count() >= 2);
    const char* mode = fern_arg(1);
    assert(mode != NULL);
    if (strcmp(mode, "child") == 0) {
        fputs("child output", stdout);
        fputs("child error", stderr);
        return 7;
    }
    if (strcmp(mode, "child-bulk") == 0) {
        char bytes[4096];
        memset(bytes, 'x', sizeof(bytes));
        for (int i = 0; i < 128; i++) {
            fwrite(bytes, 1, sizeof(bytes), stdout);
            fwrite(bytes, 1, sizeof(bytes), stderr);
        }
        return 0;
    }
    if (strcmp(mode, "signal") == 0) {
        raise(SIGTERM);
        return 1;
    }
    if (strcmp(mode, "missing") == 0) {
        char* args[] = {"/definitely-missing-fern-command"};
        FernExecResult* result = execute(args, 1);
        printf("%lld\n", (long long)result->exit_code);
        return 0;
    }
    if (strcmp(mode, "empty") == 0) {
        char* args[] = {NULL};
        FernExecResult* result = execute(args, 0);
        printf("%lld\n", (long long)result->exit_code);
        return 0;
    }
    if (strcmp(mode, "literal") == 0) {
        char* args[] = {"/usr/bin/printf", "%s", fern_arg(2)};
        FernExecResult* result = execute(args, 3);
        assert(result->exit_code == 0);
        assert(strcmp(result->stderr_str, "") == 0);
        fputs(result->stdout_str, stdout);
        return 0;
    }
    if (strcmp(mode, "closed") == 0) { close(STDIN_FILENO); close(STDERR_FILENO); }
    char* child_mode = strcmp(mode, "signaled") == 0 ? "signal" : "child";
    if (strcmp(mode, "bulk") == 0) child_mode = "child-bulk";
    char* args[] = {fern_arg(0), child_mode};
    FernExecResult* result = execute(args, 2);
    if (strcmp(mode, "bulk") == 0) {
        printf("%lld\n%zu\n%zu\n", (long long)result->exit_code, strlen(result->stdout_str), strlen(result->stderr_str));
    } else {
        printf("%lld\n%s\n%s\n", (long long)result->exit_code, result->stdout_str, result->stderr_str);
    }
    return 0;
}
