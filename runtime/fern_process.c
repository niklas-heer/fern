/** Bounded literal process capture. Decision85 defines heap ABI, ownership and cleanup limits. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "fern_runtime.h"
#include "fern_gc.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <libproc.h>
#endif
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;
#define EXEC_ARG_COUNT 4096
#define EXEC_ARG_BYTES (1024u * 1024u)
#define EXEC_OUTPUT_BYTES (16u * 1024u * 1024u)
#define EXEC_CHUNK 4096u
_Static_assert(sizeof(FernExecResult) == 24, "native execution tuple must occupy three full words");
_Static_assert(offsetof(FernExecResult, exit_code) == 0, "exit status offset");
_Static_assert(offsetof(FernExecResult, stdout_str) == 8, "stdout pointer offset");
_Static_assert(offsetof(FernExecResult, stderr_str) == 16, "stderr pointer offset");

typedef struct {
    int read_fd;
    int write_fd;
    char* text;
    size_t length;
    size_t capacity;
} ExecStream;
typedef struct {
    ExecStream stream[2];
    int input_fd;
    int error;
    pid_t child;
    bool retained;
    bool finished;
    int exit_code;
    int64_t deadline;
    size_t limit;
} ExecState;

/** Retain the first failure. @param state Owned operation. @param code Nonzero stable error. */
static void exec_error(ExecState* state, int code) {
    assert(state != NULL);
    assert(code >= 1 && code <= 7);
    if (state->error == 0) state->error = code;
}

/** Read monotonic milliseconds without integer overflow. @return Time or -1 on clock failure. */
static int64_t exec_now(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    if (now.tv_sec < 0 || (uint64_t)now.tv_sec > (uint64_t)INT64_MAX / 1000) return -1;
    assert(now.tv_nsec >= 0);
    assert(now.tv_nsec < 1000000000);
    int64_t seconds = (int64_t)now.tv_sec * 1000;
    int64_t partial = now.tv_nsec / 1000000;
    return seconds > INT64_MAX - partial ? -1 : seconds + partial;
}

/** Check the deadline after bounded work. @param state Owned operation. @return Remaining milliseconds or zero. */
static int64_t exec_remaining(ExecState* state) {
    assert(state != NULL);
    assert(state->deadline >= 0);
    int64_t now = exec_now();
    if (now < 0) { exec_error(state, FERN_EXEC_IO); return 0; }
    if (now >= state->deadline) { exec_error(state, FERN_EXEC_TIMEOUT); return 0; }
    return state->deadline - now;
}

/** Validate complete scalar text without reading beyond len. @param text Captured bytes. @param len Byte length. @return True for NUL-free UTF8. */
static bool exec_utf8(const char* text, size_t len) {
    assert(text != NULL);
    assert(len <= EXEC_OUTPUT_BYTES || len <= EXEC_ARG_BYTES);
    for (size_t i = 0; i < len;) {
        unsigned char a = (unsigned char)text[i++];
        if (a == 0) return false;
        if (a < 0x80) continue;
        unsigned extra = a >= 0xc2 && a <= 0xdf ? 1 : a >= 0xe0 && a <= 0xef ? 2 : a >= 0xf0 && a <= 0xf4 ? 3 : 0;
        if (extra == 0 || extra > len - i) return false;
        unsigned char b = (unsigned char)text[i];
        if ((a == 0xe0 && b < 0xa0) || (a == 0xed && b >= 0xa0) ||
            (a == 0xf0 && b < 0x90) || (a == 0xf4 && b >= 0x90)) return false;
        for (unsigned j = 0; j < extra; j++) {
            unsigned char byte = (unsigned char)text[i++];
            if (byte < 0x80 || byte > 0xbf) return false;
        }
    }
    return true;
}

/** Validate header/count before entries and terminator-inclusive bytes before argv allocation. @param args Native list. @return Validity. */
static bool exec_arguments(const FernStringList* args) {
    if (args == NULL || args->data == NULL || args->len < 1 || args->len > EXEC_ARG_COUNT || args->cap < args->len) return false;
    size_t remaining = EXEC_ARG_BYTES;
    for (int64_t i = 0; i < args->len; i++) {
        const char* arg = args->data[i];
        if (arg == NULL || remaining == 0) return false;
        size_t length = strnlen(arg, remaining);
        if (length == remaining || (i == 0 && length == 0) || !exec_utf8(arg, length)) return false;
        remaining -= length + 1;
    }
    assert(args->len <= EXEC_ARG_COUNT);
    assert(remaining <= EXEC_ARG_BYTES);
    return true;
}

/** Move one owned descriptor above stdio with close-on-exec. @param fd Owned descriptor. @return New fd or -1. */
static int exec_move_fd(int fd) {
    assert(fd >= 0);
    int moved = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    int saved = errno;
    if (close(fd) != 0) { if (moved >= 0) close(moved); return -1; }
    errno = saved;
    assert(moved < 0 || moved >= 3);
    return moved;
}

/** Close once, never retry an ambiguous EINTR descriptor. @param state Operation. @param descriptor Owned slot. */
static void exec_close(ExecState* state, int* descriptor) {
    assert(state != NULL);
    assert(descriptor != NULL);
    int fd = *descriptor;
    *descriptor = -1;
    if (fd >= 0 && close(fd) != 0) exec_error(state, FERN_EXEC_IO);
}

/** Create independent pipes; only parent readers become nonblocking. @param stream Selected capture. @return Success. */
static bool exec_pipe(ExecStream* stream) {
    assert(stream != NULL);
    assert(stream->read_fd == -1 && stream->write_fd == -1);
    int raw[2];
    if (pipe(raw) != 0) return false;
    stream->read_fd = exec_move_fd(raw[0]);
    stream->write_fd = exec_move_fd(raw[1]);
    if (stream->read_fd < 0 || stream->write_fd < 0) return false;
    int flags = fcntl(stream->read_fd, F_GETFL);
    if (flags < 0 || fcntl(stream->read_fd, F_SETFL, flags | O_NONBLOCK) < 0) return false;
    stream->text = FERN_ALLOC(1);
    if (stream->text == NULL) return false;
    stream->text[0] = 0;
    stream->capacity = 1;
    return true;
}

/** Establish owned stdio sources without changing caller descriptors. @param state Operation. @return Success. */
static bool exec_descriptors(ExecState* state) {
    assert(state != NULL);
    assert(state->input_fd == -1);
    int initial = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (initial < 0) return false;
    state->input_fd = exec_move_fd(initial);
    if (state->input_fd < 0) return false;
    return exec_pipe(&state->stream[0]) && exec_pipe(&state->stream[1]);
}

/** Build checked dup/close actions from exclusively owned fds. @param actions Initialized actions. @param state Ready capture. @return POSIX code. */
static int exec_actions(posix_spawn_file_actions_t* actions, const ExecState* state) {
    assert(actions != NULL);
    assert(state->input_fd >= 3);
    int source[] = {state->input_fd, state->stream[0].write_fd, state->stream[1].write_fd};
    int status = 0;
    for (int i = 0; i < 3 && status == 0; i++) status = posix_spawn_file_actions_adddup2(actions, source[i], i);
    int owned[] = {state->input_fd, state->stream[0].read_fd, state->stream[0].write_fd, state->stream[1].read_fd, state->stream[1].write_fd};
    for (unsigned i = 0; i < 5 && status == 0; i++) status = posix_spawn_file_actions_addclose(actions, owned[i]);
    return status;
}

/** Give the child a private group and predictable signals. @param attributes Initialized spawn settings. @return POSIX code. */
static int exec_attributes(posix_spawnattr_t* attributes) {
    assert(attributes != NULL);
    sigset_t mask, defaults;
    if (sigemptyset(&mask) != 0 || sigfillset(&defaults) != 0 || sigdelset(&defaults, SIGKILL) != 0 || sigdelset(&defaults, SIGSTOP) != 0) return EINVAL;
    int status = posix_spawnattr_setpgroup(attributes, 0);
    if (status == 0) status = posix_spawnattr_setsigmask(attributes, &mask);
    if (status == 0) status = posix_spawnattr_setsigdefault(attributes, &defaults);
    short flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
    assert((flags & POSIX_SPAWN_SETPGROUP) != 0);
    if (status == 0) status = posix_spawnattr_setflags(attributes, flags);
    return status;
}

/** Validate PATH once and reserve one reusable candidate without repeated argv copying.
 * @param state Operation. @param name Executable basename. @param path Prepared search. @param maximum Longest component.
 * @return Managed candidate buffer, or NULL after recording an error.
 */
static char* exec_search_buffer(ExecState* state, const char* name, const char** path, size_t* maximum) {
    assert(state != NULL);
    assert(name != NULL && path != NULL && maximum != NULL);
    *path = getenv("PATH");
    if (*path == NULL) *path = "/usr/bin:/bin";
    size_t length = strnlen(*path, EXEC_ARG_BYTES + 1);
    if (length > EXEC_ARG_BYTES) { exec_error(state, FERN_EXEC_INVALID); return NULL; }
    size_t components = 1, current = 0;
    *maximum = 0;
    for (size_t i = 0; i <= length; i++) {
        if (i == length || (*path)[i] == ':') {
            if (current > *maximum) *maximum = current;
            current = 0;
            if (i < length) components++;
        } else current++;
    }
    if (components > EXEC_ARG_COUNT) { exec_error(state, FERN_EXEC_INVALID); return NULL; }
    size_t name_length = strlen(name);
    char* buffer = FERN_ALLOC(*maximum + name_length + 2);
    if (buffer == NULL) { exec_error(state, FERN_EXEC_IO); return NULL; }
    buffer[*maximum] = '/';
    memcpy(buffer + *maximum + 1, name, name_length + 1);
    return buffer;
}

/** Search literal PATH candidates without libc's ENOEXEC shell fallback.
 * @param state Operation. @param argv Original vector. @param actions Initialized file actions. @param attributes Initialized settings.
 * @return A synchronous spawn status; state preserves validation/deadline failures separately.
 */
static int exec_literal_spawn(ExecState* state, char** argv, posix_spawn_file_actions_t* actions,
                              posix_spawnattr_t* attributes) {
    if (strchr(argv[0], '/') != NULL) {
        if (exec_remaining(state) == 0) return ECANCELED;
        return posix_spawn(&state->child, argv[0], actions, attributes, argv, environ);
    }
    const char* path; size_t maximum;
    char* buffer = exec_search_buffer(state, argv[0], &path, &maximum);
    if (buffer == NULL) return ECANCELED;
    int status = ENOENT, denied = 0;
    for (const char* part = path;;) {
        if (exec_remaining(state) == 0) return ECANCELED;
        size_t length = strcspn(part, ":");
        char* candidate = buffer + maximum - length;
        if (length != 0) memcpy(candidate, part, length);
        status = posix_spawn(&state->child, length == 0 ? argv[0] : candidate, actions, attributes, argv, environ);
        if (status == 0 || (status != ENOENT && status != ENOTDIR && status != EACCES)) return status;
        if (status == EACCES) denied = status;
        part += length;
        if (*part == 0) return denied != 0 ? denied : status;
        part++;
    }
}

/** Spawn once; destroy only successfully initialized POSIX objects. @param state Operation. @param argv Validated vector. */
static void exec_spawn(ExecState* state, char** argv) {
    assert(state != NULL);
    assert(argv != NULL);
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    int status = posix_spawn_file_actions_init(&actions);
    if (status != 0) { exec_error(state, FERN_EXEC_IO); return; }
    status = posix_spawnattr_init(&attributes);
    if (status != 0) { posix_spawn_file_actions_destroy(&actions); exec_error(state, FERN_EXEC_IO); return; }
    status = exec_actions(&actions, state);
    if (status == 0) status = exec_attributes(&attributes);
    if (status != 0) exec_error(state, FERN_EXEC_IO);
    else {
        status = exec_literal_spawn(state, argv, &actions, &attributes);
        if (status != 0) exec_error(state, FERN_EXEC_SPAWN);
        else state->retained = true;
    }
    if (posix_spawnattr_destroy(&attributes) != 0) exec_error(state, FERN_EXEC_IO);
    if (posix_spawn_file_actions_destroy(&actions) != 0) exec_error(state, FERN_EXEC_IO);
}

/** Append after enforcing the independent cap; no capacity exceeds cap+1. @param state Operation. @param stream Capture. @param bytes New bytes. @param count Byte length. */
static void exec_append(ExecState* state, ExecStream* stream, const char* bytes, size_t count) {
    assert(stream->length <= state->limit);
    assert(count <= EXEC_CHUNK);
    if (count > state->limit - stream->length) { exec_error(state, FERN_EXEC_OUTPUT_LIMIT); return; }
    size_t required = stream->length + count + 1;
    if (required > stream->capacity) {
        size_t capacity = stream->capacity * 2;
        if (capacity < required) capacity = required;
        if (capacity > state->limit + 1) capacity = state->limit + 1;
        char* next = FERN_REALLOC(stream->text, capacity);
        if (next == NULL) { exec_error(state, FERN_EXEC_IO); return; }
        stream->text = next; stream->capacity = capacity;
    }
    memcpy(stream->text + stream->length, bytes, count);
    stream->length += count;
    stream->text[stream->length] = 0;
}

/** Read one bounded chunk so neither stream nor signals starve the clock. @param state Operation. @param stream Capture. @return True on progress. */
static bool exec_read(ExecState* state, ExecStream* stream) {
    assert(state != NULL);
    assert(stream != NULL);
    if (stream->read_fd < 0) return false;
    char bytes[EXEC_CHUNK];
    size_t room = state->limit - stream->length;
    size_t request = room < EXEC_CHUNK ? room + 1 : EXEC_CHUNK;
    ssize_t count;
    do {
        count = read(stream->read_fd, bytes, request);
    } while (count < 0 && errno == EINTR && exec_remaining(state) > 0);
    if (count > 0) exec_append(state, stream, bytes, (size_t)count);
    else if (count == 0) exec_close(state, &stream->read_fd);
    else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) exec_error(state, FERN_EXEC_IO);
    exec_remaining(state);
    return count > 0;
}

/** Observe only the owned child without reaping its identity. @param state Spawned operation. */
static void exec_observe(ExecState* state) {
    assert(state->retained);
    assert(state->child > 0);
    siginfo_t info; memset(&info, 0, sizeof(info));
    if (waitid(P_PID, (id_t)state->child, &info, WEXITED | WNOHANG | WNOWAIT) != 0) {
        if (errno == EINTR) { exec_remaining(state); return; }
        if (errno == ECHILD) state->retained = false;
        exec_error(state, FERN_EXEC_IO); return;
    }
    if (info.si_pid == 0) return;
    state->finished = true;
    if (info.si_code == CLD_EXITED) state->exit_code = info.si_status;
    else if (info.si_code == CLD_KILLED || info.si_code == CLD_DUMPED) exec_error(state, FERN_EXEC_SIGNAL);
    else exec_error(state, FERN_EXEC_IO);
}

/** Wait briefly for either stream or another child observation, bounded by remaining time. @param state Operation. */
static void exec_poll(ExecState* state) {
    assert(state != NULL);
    assert(state->retained);
    int64_t remaining = exec_remaining(state);
    if (state->error != 0) return;
    struct pollfd fds[2] = {{state->stream[0].read_fd, POLLIN, 0}, {state->stream[1].read_fd, POLLIN, 0}};
    int status = poll(fds, 2, remaining > 10 ? 10 : (int)remaining);
    if (status < 0 && errno != EINTR) exec_error(state, FERN_EXEC_IO);
    for (unsigned i = 0; i < 2; i++) if (fds[i].revents & POLLNVAL) exec_error(state, FERN_EXEC_IO);
    exec_remaining(state);
}

/** Interleave status, reads and deadlines until completion or the first error. @param state Spawned operation. */
static void exec_capture(ExecState* state) {
    assert(state != NULL);
    assert(state->retained);
    while (!state->finished && state->error == 0) {
        if (exec_remaining(state) == 0) break;
        exec_observe(state);
        if (state->finished || state->error != 0) break;
        exec_read(state, &state->stream[0]);
        if (state->error == 0) exec_read(state, &state->stream[1]);
        if (state->error == 0) exec_poll(state);
    }
}

/** Verify Darwin's zombie-only EPERM using a complete fixed-size group snapshot.
 * @param state Retained child, already observed exited. @return True only for that one zombie.
 */
static bool exec_zombie_group(const ExecState* state) {
    assert(state != NULL);
    assert(state->child > 0);
    if (!state->retained || !state->finished) return false;
#ifdef __APPLE__
    pid_t members[2] = {0, 0};
    errno = 0;
    int bytes = proc_listpids(PROC_PGRP_ONLY, (uint32_t)state->child, members, sizeof(members));
    return errno == 0 && bytes == sizeof(pid_t) && members[0] == state->child;
#else
    return false;
#endif
}

/** Kill remaining private-group members before exact-child reap; ECHILD forbids further group signaling. @param state Operation. */
static void exec_cleanup_child(ExecState* state) {
    assert(state != NULL);
    assert(!state->retained || state->child > 0);
    if (!state->retained) return;
    if (kill(-state->child, SIGKILL) != 0) {
        int error = errno;
        if (error != ESRCH && !(error == EPERM && exec_zombie_group(state))) exec_error(state, FERN_EXEC_IO);
    }
    int status;
    pid_t reaped;
    do { reaped = waitpid(state->child, &status, 0); } while (reaped < 0 && errno == EINTR);
    if (reaped != state->child) exec_error(state, FERN_EXEC_IO);
    state->retained = false;
}

/** Drain only immediately readable post-cleanup bytes; escaped writers cannot force an EOF wait. @param state Operation. */
static void exec_drain(ExecState* state) {
    assert(state != NULL);
    assert(!state->retained);
    bool active[2] = {true, true};
    for (size_t round = 0; round <= state->limit + 1 && (active[0] || active[1]) && state->error == 0; round++) {
        for (unsigned i = 0; i < 2 && state->error == 0; i++) {
            if (active[i]) active[i] = exec_read(state, &state->stream[i]);
        }
    }
}

/** Close every owned fd and validate only complete successful streams. @param state Finalized operation. @return Heap Result. */
static int64_t exec_result(ExecState* state) {
    assert(state != NULL);
    assert(!state->retained);
    exec_close(state, &state->input_fd);
    for (unsigned i = 0; i < 2; i++) {
        exec_close(state, &state->stream[i].read_fd);
        exec_close(state, &state->stream[i].write_fd);
        if (state->error == 0 && !exec_utf8(state->stream[i].text, state->stream[i].length)) exec_error(state, FERN_EXEC_TEXT);
    }
    if (state->error != 0) return fern_result_err(state->error);
    FernExecResult* result = FERN_ALLOC(sizeof(*result));
    if (result == NULL) return fern_result_err(FERN_EXEC_IO);
    result->exit_code = state->exit_code;
    result->stdout_str = state->stream[0].text;
    result->stderr_str = state->stream[1].text;
    return fern_result_ok((int64_t)(intptr_t)result);
}

/** Execute literal bounded argv with retained-child cleanup. @param args Native String list. @param timeout_ms Deadline budget. @param max_output_bytes Per-stream cap. @return Heap Result(native tuple, Int error). */
int64_t fern_exec_args_bounded(FernStringList* args, int64_t timeout_ms, int64_t max_output_bytes) {
    if (timeout_ms < 1 || timeout_ms > 600000 || max_output_bytes < 0 || max_output_bytes > EXEC_OUTPUT_BYTES || !exec_arguments(args)) return fern_result_err(FERN_EXEC_INVALID);
    struct sigaction policy;
    if (sigaction(SIGCHLD, NULL, &policy) != 0) return fern_result_err(FERN_EXEC_IO);
    if (policy.sa_handler == SIG_IGN || (policy.sa_flags & SA_NOCLDWAIT)) return fern_result_err(FERN_EXEC_INVALID);
    int64_t now = exec_now();
    if (now < 0 || now > INT64_MAX - timeout_ms) return fern_result_err(FERN_EXEC_IO);
    ExecState state = {.stream = {{.read_fd=-1,.write_fd=-1},{.read_fd=-1,.write_fd=-1}}, .input_fd=-1, .child=-1, .deadline=now+timeout_ms, .limit=(size_t)max_output_bytes};
    char** argv = FERN_ALLOC(((size_t)args->len + 1) * sizeof(*argv));
    if (argv == NULL) return fern_result_err(FERN_EXEC_IO);
    for (int64_t i = 0; i < args->len; i++) argv[i] = args->data[i];
    argv[args->len] = NULL;
    assert(args->len <= EXEC_ARG_COUNT);
    assert(state.limit <= EXEC_OUTPUT_BYTES);
    if (!exec_descriptors(&state)) exec_error(&state, FERN_EXEC_IO);
    if (state.error == 0) exec_spawn(&state, argv);
    exec_close(&state, &state.input_fd);
    exec_close(&state, &state.stream[0].write_fd);
    exec_close(&state, &state.stream[1].write_fd);
    if (state.retained && state.error == 0) exec_capture(&state);
    exec_cleanup_child(&state);
    if (state.error == 0) exec_drain(&state);
    return exec_result(&state);
}
