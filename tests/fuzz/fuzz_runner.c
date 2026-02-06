#include "fuzz_generator.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    uint64_t seed;
    uint32_t iterations;
    const char* fern_bin;
} RunnerConfig;

typedef struct {
    bool launched;
    bool signaled;
    int signal_no;
    int exit_code;
} CommandStatus;

typedef struct {
    bool ok;
    const char* stage;
    bool crashed;
    int signal_no;
    int exit_code;
    char failure_path[512];
} CaseResult;

static void print_usage(const char* argv0) {
    fprintf(stderr,
        "Usage: %s [--iterations N] [--seed N] [--fern-bin PATH]\n",
        argv0 ? argv0 : "fuzz_runner");
}

static bool parse_u64(const char* text, uint64_t* out) {
    char* end = NULL;
    unsigned long long value = 0;

    if (!text || !out) {
        return false;
    }

    errno = 0;
    value = strtoull(text, &end, 0);
    if (errno != 0 || !end || *end != '\0') {
        return false;
    }

    *out = (uint64_t)value;
    return true;
}

static bool parse_u32(const char* text, uint32_t* out) {
    uint64_t value = 0;

    if (!parse_u64(text, &value)) {
        return false;
    }

    if (value > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static bool parse_args(int argc, char** argv, RunnerConfig* cfg) {
    int i = 0;
    uint64_t seed_value = 0;

    if (!cfg) {
        return false;
    }

    cfg->iterations = 256;
    cfg->fern_bin = "./bin/fern";

    seed_value = (uint64_t)time(NULL);
    cfg->seed = seed_value == 0 ? UINT64_C(1) : seed_value;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--iterations") == 0) {
            if (i + 1 >= argc || !parse_u32(argv[i + 1], &cfg->iterations)) {
                return false;
            }
            i++;
            continue;
        }

        if (strcmp(argv[i], "--seed") == 0) {
            if (i + 1 >= argc || !parse_u64(argv[i + 1], &cfg->seed)) {
                return false;
            }
            i++;
            continue;
        }

        if (strcmp(argv[i], "--fern-bin") == 0) {
            if (i + 1 >= argc) {
                return false;
            }
            cfg->fern_bin = argv[i + 1];
            i++;
            continue;
        }

        return false;
    }

    return true;
}

static CommandStatus run_fern_subcommand(const char* fern_bin, const char* command, const char* path) {
    CommandStatus status = {0};
    pid_t pid = 0;
    int wait_status = 0;

    if (!fern_bin || !command || !path) {
        return status;
    }

    pid = fork();
    if (pid < 0) {
        return status;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            (void)dup2(devnull, STDIN_FILENO);
            (void)dup2(devnull, STDOUT_FILENO);
            (void)dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        execl(fern_bin, fern_bin, command, path, (char*)NULL);
        _exit(127);
    }

    status.launched = true;
    if (waitpid(pid, &wait_status, 0) < 0) {
        status.launched = false;
        return status;
    }

    if (WIFSIGNALED(wait_status)) {
        status.signaled = true;
        status.signal_no = WTERMSIG(wait_status);
        return status;
    }

    if (WIFEXITED(wait_status)) {
        status.exit_code = WEXITSTATUS(wait_status);
    }

    return status;
}

static char* write_temp_source(const char* source) {
    char pattern[] = "/tmp/fern_fuzz_XXXXXX.fn";
    int fd = -1;
    FILE* file = NULL;
    char* path = NULL;

    if (!source) {
        return NULL;
    }

    fd = mkstemps(pattern, 3);
    if (fd < 0) {
        return NULL;
    }

    file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        unlink(pattern);
        return NULL;
    }

    if (fputs(source, file) == EOF) {
        fclose(file);
        unlink(pattern);
        return NULL;
    }

    if (fclose(file) != 0) {
        unlink(pattern);
        return NULL;
    }

    path = (char*)malloc(strlen(pattern) + 1);
    if (!path) {
        unlink(pattern);
        return NULL;
    }

    strcpy(path, pattern);
    return path;
}

static char* read_file_all(const char* path) {
    FILE* file = NULL;
    long size = 0;
    size_t read_size = 0;
    char* data = NULL;

    if (!path) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    data = (char*)malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }

    read_size = fread(data, 1, (size_t)size, file);
    fclose(file);

    if (read_size != (size_t)size) {
        free(data);
        return NULL;
    }

    data[size] = '\0';
    return data;
}

static bool ensure_failure_dir(void) {
    if (mkdir("tests/fuzz/failures", 0777) == 0) {
        return true;
    }

    return errno == EEXIST;
}

static bool persist_failure_source(
    uint64_t seed,
    uint32_t iteration,
    const char* stage,
    const char* source,
    char* out_path,
    size_t out_path_len
) {
    FILE* file = NULL;
    int written = 0;

    if (!stage || !source || !out_path || out_path_len == 0) {
        return false;
    }

    if (!ensure_failure_dir()) {
        return false;
    }

    written = snprintf(
        out_path,
        out_path_len,
        "tests/fuzz/failures/fail_%" PRIu32 "_%016" PRIx64 "_%s.fn",
        iteration,
        seed,
        stage
    );
    if (written < 0 || (size_t)written >= out_path_len) {
        return false;
    }

    file = fopen(out_path, "wb");
    if (!file) {
        return false;
    }

    if (fputs(source, file) == EOF) {
        fclose(file);
        return false;
    }

    if (fclose(file) != 0) {
        return false;
    }

    return true;
}

static bool validate_program(
    const RunnerConfig* cfg,
    uint64_t seed,
    uint32_t iteration,
    const char* source,
    CaseResult* result
) {
    char* temp_path = NULL;
    char* formatted_once = NULL;
    char* formatted_twice = NULL;
    CommandStatus parse_status = {0};
    CommandStatus fmt_status = {0};

    if (!cfg || !source || !result) {
        return false;
    }

    result->ok = false;
    result->stage = "setup";
    result->crashed = false;
    result->signal_no = 0;
    result->exit_code = 0;
    result->failure_path[0] = '\0';

    temp_path = write_temp_source(source);
    if (!temp_path) {
        result->stage = "tempfile";
        return false;
    }

    parse_status = run_fern_subcommand(cfg->fern_bin, "parse", temp_path);
    if (!parse_status.launched) {
        result->stage = "parse-launch";
        goto fail;
    }
    if (parse_status.signaled) {
        result->stage = "parse-crash";
        result->crashed = true;
        result->signal_no = parse_status.signal_no;
        goto fail;
    }
    if (parse_status.exit_code != 0) {
        result->stage = "parse-exit";
        result->exit_code = parse_status.exit_code;
        goto fail;
    }

    fmt_status = run_fern_subcommand(cfg->fern_bin, "fmt", temp_path);
    if (!fmt_status.launched) {
        result->stage = "fmt-launch";
        goto fail;
    }
    if (fmt_status.signaled) {
        result->stage = "fmt-crash";
        result->crashed = true;
        result->signal_no = fmt_status.signal_no;
        goto fail;
    }
    if (fmt_status.exit_code != 0) {
        result->stage = "fmt-exit";
        result->exit_code = fmt_status.exit_code;
        goto fail;
    }

    formatted_once = read_file_all(temp_path);
    if (!formatted_once) {
        result->stage = "fmt-read-1";
        goto fail;
    }

    fmt_status = run_fern_subcommand(cfg->fern_bin, "fmt", temp_path);
    if (!fmt_status.launched) {
        result->stage = "fmt2-launch";
        goto fail;
    }
    if (fmt_status.signaled) {
        result->stage = "fmt2-crash";
        result->crashed = true;
        result->signal_no = fmt_status.signal_no;
        goto fail;
    }
    if (fmt_status.exit_code != 0) {
        result->stage = "fmt2-exit";
        result->exit_code = fmt_status.exit_code;
        goto fail;
    }

    formatted_twice = read_file_all(temp_path);
    if (!formatted_twice) {
        result->stage = "fmt-read-2";
        goto fail;
    }

    if (strcmp(formatted_once, formatted_twice) != 0) {
        result->stage = "fmt-idempotence";
        goto fail;
    }

    parse_status = run_fern_subcommand(cfg->fern_bin, "parse", temp_path);
    if (!parse_status.launched) {
        result->stage = "parse2-launch";
        goto fail;
    }
    if (parse_status.signaled) {
        result->stage = "parse2-crash";
        result->crashed = true;
        result->signal_no = parse_status.signal_no;
        goto fail;
    }
    if (parse_status.exit_code != 0) {
        result->stage = "parse2-exit";
        result->exit_code = parse_status.exit_code;
        goto fail;
    }

    result->ok = true;
    unlink(temp_path);
    free(temp_path);
    free(formatted_once);
    free(formatted_twice);
    return true;

fail:
    (void)persist_failure_source(seed, iteration, result->stage, source, result->failure_path, sizeof(result->failure_path));

    if (temp_path) {
        unlink(temp_path);
    }
    free(temp_path);
    free(formatted_once);
    free(formatted_twice);
    return false;
}

static char* pick_program_source(uint64_t seed, uint32_t iteration) {
    size_t seed_count = fuzz_seed_program_count();

    if (seed_count > 0 && iteration < seed_count) {
        return fuzz_load_seed_program(iteration);
    }

    return fuzz_generate_program(seed, iteration);
}

int main(int argc, char** argv) {
    RunnerConfig cfg = {0};
    uint32_t iteration = 0;
    uint32_t passed = 0;

    if (!parse_args(argc, argv, &cfg)) {
        print_usage(argv[0]);
        return 2;
    }

    printf("FernFuzz: iterations=%" PRIu32 " seed=0x%016" PRIx64 " fern=%s\n",
        cfg.iterations, cfg.seed, cfg.fern_bin);
    fflush(stdout);

    for (iteration = 0; iteration < cfg.iterations; iteration++) {
        CaseResult result = {0};
        char* source = pick_program_source(cfg.seed, iteration);

        if (!source) {
            fprintf(stderr,
                "FernFuzz failed: could not build source for iteration=%" PRIu32 "\n",
                iteration);
            return 1;
        }

        if (!validate_program(&cfg, cfg.seed, iteration, source, &result)) {
            fprintf(stderr,
                "FernFuzz FAIL: iteration=%" PRIu32 " stage=%s seed=0x%016" PRIx64 "\n",
                iteration, result.stage, cfg.seed);
            if (result.crashed) {
                fprintf(stderr, "  crash signal: %d\n", result.signal_no);
            }
            if (result.exit_code != 0) {
                fprintf(stderr, "  exit code: %d\n", result.exit_code);
            }
            if (result.failure_path[0] != '\0') {
                fprintf(stderr, "  saved input: %s\n", result.failure_path);
            }
            free(source);
            return 1;
        }

        free(source);
        passed++;

        if ((iteration + 1U) % 64U == 0U || (iteration + 1U) == cfg.iterations) {
            printf("  progress: %" PRIu32 "/%" PRIu32 "\n", iteration + 1U, cfg.iterations);
            fflush(stdout);
        }
    }

    printf("FernFuzz PASS: %" PRIu32 " cases\n", passed);
    return 0;
}
