/** Standalone native test supervision, adapted from the audited Decision 93
 * retained-child core. No GC, cache, shell or source-language ABI is linked. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#endif
extern char **environ;
#define LOG_LIMIT (256u * 1024u)
#define PUBLICATION_MS 1000
#include <sys/stat.h>
#define ARG_BYTES (1024u * 1024u)
#define CHUNK 4096u

typedef enum {
    SUP_OK,
    SUP_INVALID,
    SUP_SPAWN,
    SUP_TIME,
    SUP_CAP,
    SUP_IO,
    SUP_SIGNAL,
    SUP_REAPER,
    SUP_PARENT
} SupError;
typedef enum { READ_IDLE, READ_PROGRESS, READ_RETRY } ReadStatus;
typedef struct {
    int reader, writer, file;
    size_t length;
} SupStream;
typedef struct {
    SupStream streams[2];
    int input, status;
    pid_t child;
    bool retained, finished;
    SupError error;
    int signal;
    int64_t deadline;
    size_t limit;
    unsigned writes;
    int parent, directory;
    bool created;
    struct stat identity;
} Supervisor;
static volatile sig_atomic_t interrupted;

/** Record only the first supervisor failure. @param state Owned operation.
 * @param error Failure. */
static void failure(Supervisor *state, SupError error) {
    assert(state != NULL);
    assert(error > SUP_OK && error <= SUP_PARENT);
    if (state->error == SUP_OK) {
        state->error = error;
    }
}

/** Async-signal-safe flag publication; standalone process owns this handler.
 * @param signal First interruption to preserve; no allocation, IO or assertion
 * in signal context.
 */
static void interrupt_handler(int signal) {
    // FERN_STYLE: allow(assertion-density) asynchronous external signal ABI uses
    // sig_atomic_t only
    if (interrupted == 0) {
        interrupted = signal;
    }
}

/** Read monotonic milliseconds safely. @return Milliseconds, or-1 on
 * unsupported clock range/failure. */
static int64_t milliseconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 || now.tv_sec < 0 ||
        (uint64_t)now.tv_sec > (uint64_t)INT64_MAX / 1000 - 1) {
        return -1;
    }
    assert(now.tv_nsec >= 0);
    assert(now.tv_nsec < 1000000000);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/** Check interruption/deadline after bounded work. @param state Operation.
 * @return Time remaining or0. */
static int64_t remaining(Supervisor *state) {
    assert(state != NULL);
    assert(state->deadline >= 0);
    if (interrupted) {
        state->signal = interrupted;
        failure(state, SUP_SIGNAL);
    }
    int64_t now = milliseconds();
    if (now < 0) {
        failure(state, SUP_IO);
        return 0;
    }
    if (now >= state->deadline) {
        failure(state, SUP_TIME);
        return 0;
    }
    return state->deadline - now;
}

/** Parse a bounded decimal setting without sign/overflow. @param text Argument.
 * @param maximum Limit. @param value Output. @return Validity. */
static bool number(const char *text, uint64_t maximum, uint64_t *value) {
    assert(text != NULL);
    assert(value != NULL);
    size_t length = strnlen(text, 20);
    if (length == 0 || length == 20) {
        return false;
    }
    uint64_t result = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        unsigned digit = (unsigned)(text[i] - '0');
        if (digit > maximum || result > (maximum - digit) / 10) {
            return false;
        }
        result = result * 10 + digit;
    }
    *value = result;
    return true;
}

/** Validate literal command bounds and timeout before any process is spawned.
 * @param argc OS count. @param argv OS strings. @param state Settings output.
 * @return Whether all bounds and absolute paths are valid. */
static bool arguments(int argc, char **argv, Supervisor *state) {
    assert(argv != NULL);
    assert(state != NULL);
    if (argc < 5 || argc > 4100 || strcmp(argv[3], "--") != 0 || argv[4][0] != '/' ||
        argv[2][0] != '/' || strnlen(argv[2], 4096) == 4096) {
        return false;
    }
    uint64_t timeout = 0;
    if (!number(argv[1], 60000, &timeout) || timeout == 0) {
        return false;
    }
    size_t room = ARG_BYTES;
    for (int i = 4; i < argc; i++) {
        size_t n = strnlen(argv[i], room);
        if (n == room) {
            return false;
        }
        room -= n + 1;
    }
    int64_t now = milliseconds();
    if (now < 0 || now > INT64_MAX - (int64_t)timeout) {
        return false;
    }
    state->deadline = now + (int64_t)timeout;
    state->limit = LOG_LIMIT;
    return true;
}

/** Install process-owned interruption handlers and reject auto-reaping. @return
 * Success. */
static bool signals(void) {
    struct sigaction current;
    if (sigaction(SIGCHLD, NULL, &current) != 0) {
        return false;
    }
    if (current.sa_handler == SIG_IGN || (current.sa_flags & SA_NOCLDWAIT)) {
        return false;
    }
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_handler = interrupt_handler;
    int caught[] = {SIGINT, SIGTERM, SIGHUP};
    assert(sizeof(caught) / sizeof(*caught) == 3);
    assert(interrupted == 0);
    for (unsigned i = 0; i < 3; i++) {
        if (sigaction(caught[i], &action, NULL) != 0) {
            return false;
        }
    }
    sigset_t unblock;
    if (sigemptyset(&unblock) != 0) {
        return false;
    }
    for (unsigned i = 0; i < 3; i++) {
        if (sigaddset(&unblock, caught[i]) != 0) {
            return false;
        }
    }
    return sigprocmask(SIG_UNBLOCK, &unblock, NULL) == 0;
}

/** Close an owned descriptor exactly once. @param state Operation. @param slot
 * Owned fd slot. */
static void close_owned(Supervisor *state, int *slot) {
    assert(state != NULL);
    assert(slot != NULL);
    int fd = *slot;
    *slot = -1;
    if (fd >= 0 && close(fd) != 0) {
        failure(state, SUP_IO);
    }
}

/** Move an owned fd above stdio with close-on-exec. @param fd Owned fd. @return
 * New fd or-1. */
static int move_fd(int fd) {
    assert(fd >= 0);
    int result = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    int error = errno;
    if (close(fd) != 0) {
        if (result >= 0) {
            close(result);
        }
        return -1;
    }
    errno = error;
    assert(result < 0 || result >= 3);
    return result;
}

/** Create owned pipes with nonblocking readers only. @param stream Empty slots.
 * @return Success. */
static bool capture_pipe(SupStream *stream) {
    assert(stream != NULL);
    assert(stream->reader == -1 && stream->writer == -1);
    int pair[2];
    if (pipe(pair) != 0) {
        return false;
    }
    stream->reader = move_fd(pair[0]);
    stream->writer = move_fd(pair[1]);
    if (stream->reader < 0 || stream->writer < 0) {
        return false;
    }
    int flags = fcntl(stream->reader, F_GETFL);
    return flags >= 0 && fcntl(stream->reader, F_SETFL, flags | O_NONBLOCK) == 0;
}

/** Prepare independent EOF input and capture pipes. @param state Empty
 * operation. @return Success.
 */
static bool descriptors(Supervisor *state) {
    assert(state != NULL);
    assert(state->input == -1);
    int input = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (input < 0) {
        return false;
    }
    state->input = move_fd(input);
    return state->input >= 0 && capture_pipe(&state->streams[0]) &&
           capture_pipe(&state->streams[1]);
}

/** Define child stdio exclusively from owned descriptors. @param actions
 * Initialized spawn actions.
 * @param state Prepared descriptors. @return POSIX error. */
static int spawn_actions(posix_spawn_file_actions_t *actions, const Supervisor *state) {
    assert(actions != NULL);
    assert(state->input >= 3);
    int source[] = {state->input, state->streams[0].writer, state->streams[1].writer};
    int error = 0;
    for (int i = 0; i < 3 && error == 0; i++) {
        error = posix_spawn_file_actions_adddup2(actions, source[i], i);
    }
    int owned[] = {state->input, state->streams[0].reader, state->streams[0].writer,
                   state->streams[1].reader, state->streams[1].writer};
    for (unsigned i = 0; i < 5 && error == 0; i++) {
        error = posix_spawn_file_actions_addclose(actions, owned[i]);
    }
    return error;
}

/** Give the child its own group and default signals. @param attrs Initialized
 * attributes. @return POSIX error. */
static int spawn_attributes(posix_spawnattr_t *attrs) {
    assert(attrs != NULL);
    sigset_t mask, defaults;
    sigemptyset(&mask);
    sigfillset(&defaults);
    sigdelset(&defaults, SIGKILL);
    sigdelset(&defaults, SIGSTOP);
    int error = posix_spawnattr_setpgroup(attrs, 0);
    if (!error) {
        error = posix_spawnattr_setsigmask(attrs, &mask);
    }
    if (!error) {
        error = posix_spawnattr_setsigdefault(attrs, &defaults);
    }
    short flags = POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF;
    assert(flags & POSIX_SPAWN_SETPGROUP);
    if (!error) {
        error = posix_spawnattr_setflags(attrs, flags);
    }
    return error;
}

/** Spawn a literal absolute command, never a shell/PATH fallback. @param state
 * Operation.
 * @param argv Validated OS arguments. */
static void spawn_child(Supervisor *state, char **argv) {
    assert(state != NULL);
    assert(argv != NULL && argv[0][0] == '/');
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attrs;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        failure(state, SUP_IO);
        return;
    }
    if (posix_spawnattr_init(&attrs) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        failure(state, SUP_IO);
        return;
    }
    int error = spawn_actions(&actions, state);
    if (!error) {
        error = spawn_attributes(&attrs);
    }
    if (error) {
        failure(state, SUP_IO);
    } else if (remaining(state) > 0 && state->error == SUP_OK) {
        error = posix_spawn(&state->child, argv[0], &actions, &attrs, argv, environ);
        if (error) {
            failure(state, SUP_SPAWN);
        } else {
            state->retained = true;
        }
    }
    if (posix_spawnattr_destroy(&attrs) != 0) {
        failure(state, SUP_IO);
    }
    if (posix_spawn_file_actions_destroy(&actions) != 0) {
        failure(state, SUP_IO);
    }
}

/** Forward bounded bytes without changing inherited descriptor flags.
 * @param state Operation.
 * @param fd Log destination. @param text Bytes. @param length Count. */
static void forward(Supervisor *state, int fd, const char *text, size_t length) {
    assert(state != NULL && text != NULL);
    assert(length <= CHUNK);
    for (size_t offset = 0; offset < length;) {
        if (++state->writes > 65536) {
            failure(state, SUP_IO);
            return;
        }
        ssize_t n = write(fd, text + offset, length - offset);
        if (n > 0) {
            offset += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            remaining(state);
            if (state->error != SUP_OK) {
                return;
            }
        } else {
            failure(state, SUP_IO);
            return;
        }
    }
}

/** Read at most one chunk, reserving the stream cap before forwarding.
 * @param state Operation.
 * @param index Stream0/1. @return Progress, retry on interruption, or idle on
 * EOF/EAGAIN. */
static ReadStatus read_stream(Supervisor *state, unsigned index) {
    assert(state != NULL);
    assert(index < 2);
    SupStream *stream = &state->streams[index];
    if (stream->reader < 0) {
        return READ_IDLE;
    }
    size_t room = state->limit - stream->length;
    char bytes[CHUNK];
    size_t request = room < CHUNK ? room + 1 : CHUNK;
    ssize_t n = read(stream->reader, bytes, request);
    if (n > 0) {
        size_t count = (size_t)n;
        if (count > room) {
            count = room;
            failure(state, SUP_CAP);
        }
        forward(state, stream->file, bytes, count);
        stream->length += count;
    } else if (n == 0) {
        close_owned(state, &stream->reader);
    } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
        failure(state, SUP_IO);
    }
    bool retry = n < 0 && errno == EINTR;
    remaining(state);
    return retry ? READ_RETRY : n > 0 ? READ_PROGRESS : READ_IDLE;
}

/** Observe only the retained child, without releasing its PID. @param state
 * Spawned operation. */
static void observe(Supervisor *state) {
    assert(state->retained);
    assert(state->child > 0);
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    if (waitid(P_PID, (id_t)state->child, &info, WEXITED | WNOHANG | WNOWAIT) != 0) {
        if (errno == EINTR) {
            return;
        }
        if (errno == ECHILD) {
            state->retained = false;
        }
        failure(state, SUP_REAPER);
        return;
    }
    if (info.si_pid == 0) {
        return;
    }
    state->finished = true;
    if (info.si_code != CLD_EXITED && info.si_code != CLD_KILLED && info.si_code != CLD_DUMPED) {
        failure(state, SUP_IO);
    }
}

/** Detect parent disconnect without changing its pipe flags or blocking.
 * @param state Current operation; any unexpected liveness bytes are invalid. */
static void parent_alive(Supervisor *state) {
    assert(state != NULL);
    assert(state->child >= 0);
    struct pollfd input = {0, POLLIN, 0};
    int ready = poll(&input, 1, 0);
    if (ready < 0 && errno != EINTR) {
        failure(state, SUP_IO);
    } else if (ready > 0) {
        if (input.revents & (POLLHUP | POLLIN)) {
            failure(state, SUP_PARENT);
        } else if (input.revents & (POLLERR | POLLNVAL)) {
            failure(state, SUP_IO);
        }
    }
}

/** Interleave streams and status with short deadline-aware polling.
 * @param state Retained operation. */
static void capture(Supervisor *state) {
    assert(state != NULL);
    assert(state->retained);
    for (unsigned turn = 0; turn < 65536 && state->error == SUP_OK && !state->finished; turn++) {
        parent_alive(state);
        int64_t left = remaining(state);
        if (state->error != SUP_OK) {
            break;
        }
        observe(state);
        if (state->error != SUP_OK || state->finished) {
            break;
        }
        read_stream(state, 0);
        if (state->error == SUP_OK) {
            read_stream(state, 1);
        }
        if (state->error != SUP_OK) {
            break;
        }
        struct pollfd fds[2] = {{state->streams[0].reader, POLLIN, 0},
                                {state->streams[1].reader, POLLIN, 0}};
        int result = poll(fds, 2, left > 10 ? 10 : (int)left);
        if (result < 0 && errno != EINTR) {
            failure(state, SUP_IO);
        }
        for (unsigned i = 0; i < 2; i++) {
            if (fds[i].revents & POLLNVAL) {
                failure(state, SUP_IO);
            }
        }
    }
    if (state->error == SUP_OK && !state->finished) {
        failure(state, SUP_IO);
    }
}

/** Verify only Darwin's confirmed retained-child-only zombie group exception.
 * @param state Owned exited child. @return Complete singleton membership. */
static bool zombie_group(const Supervisor *state) {
    assert(state != NULL);
    assert(state->child > 0);
    if (!state->retained || !state->finished) {
        return false;
    }
#ifdef __APPLE__
    pid_t members[2] = {0, 0};
    errno = 0;
    int count = proc_listpids(PROC_PGRP_ONLY, (uint32_t)state->child, members, sizeof(members));
    return errno == 0 && count == sizeof(pid_t) && members[0] == state->child;
#else
    return false;
#endif
}

/** Clean the private group before exact-child reap; never signal after ECHILD.
 * @param state Operation. */
static void cleanup_child(Supervisor *state) {
    assert(state != NULL);
    assert(!state->retained || state->child > 0);
    if (!state->retained) {
        return;
    }
    if (kill(-state->child, SIGKILL) != 0) {
        int error = errno;
        if (error != ESRCH && !(error == EPERM && zombie_group(state))) {
            failure(state, SUP_IO);
        }
    }
    int status;
    pid_t reaped;
    do {
        reaped = waitpid(state->child, &status, 0);
    } while (reaped < 0 && errno == EINTR);
    if (reaped != state->child) {
        failure(state, SUP_IO);
    }
    if (reaped == state->child) {
        state->status = status;
    }
    state->retained = false;
}

/** Drain until EOF after cleanup; an escaped writer can consume only the
 * remaining deadline.
 * @param state Reaped operation; interruption retries count against an explicit
 * work budget. */
static void finish_streams(Supervisor *state) {
    assert(state != NULL);
    assert(!state->retained);
    for (unsigned round = 0; round < 65536 && state->error == SUP_OK; round++) {
        if (state->streams[0].reader < 0 && state->streams[1].reader < 0) {
            break;
        }
        parent_alive(state);
        ReadStatus first = read_stream(state, 0);
        ReadStatus second = state->error == SUP_OK ? read_stream(state, 1) : READ_IDLE;
        if (first != READ_IDLE || second != READ_IDLE || state->error != SUP_OK) {
            continue;
        }
        struct pollfd streams[2] = {{state->streams[0].reader, POLLIN, 0},
                                    {state->streams[1].reader, POLLIN, 0}};
        if (poll(streams, 2, 1) < 0 && errno != EINTR) {
            failure(state, SUP_IO);
        }
    }
    if (state->streams[0].reader >= 0 || state->streams[1].reader >= 0) {
        failure(state, SUP_IO);
    }
    close_owned(state, &state->input);
    for (unsigned i = 0; i < 2; i++) {
        close_owned(state, &state->streams[i].reader);
        close_owned(state, &state->streams[i].writer);
    }
}

#include "test_protocol.h"

/** Run validated capture with every partial setup path retaining exact
 * descriptor ownership.
 * @param state Initialized state. @param argv Validated CLI arguments. */
static void execute(Supervisor *state, char **argv) {
    assert(state != NULL);
    assert(argv != NULL);
    parent_alive(state);
    if (state->error == SUP_OK && !spool_files(state, argv[2])) {
        failure(state, SUP_IO);
    }
    if (state->error == SUP_OK && !descriptors(state)) {
        failure(state, SUP_IO);
    }
    if (state->error == SUP_OK) {
        spawn_child(state, argv + 4);
    }
    close_owned(state, &state->input);
    for (unsigned i = 0; i < 2; i++) {
        close_owned(state, &state->streams[i].writer);
    }
    if (state->retained && state->error == SUP_OK) {
        capture(state);
    }
    cleanup_child(state);
    finish_streams(state);
    for (unsigned i = 0; i < 2; i++) {
        if (state->streams[i].file >= 0) {
            load_spool(state, i);
        }
    }
    remove_spools(state);
}

/** Standalone OS-owned argv boundary; all working storage is fixed-size and
 * stack/static.
 * @param argc Argument count. @param argv Borrowed OS strings.
 * @return Zero for a completed discriminated record, 125 for failed transport.
 */
int main(int argc, char **argv) {
    Supervisor state = {
        .streams = {{-1, -1, -1, 0}, {-1, -1, -1, 0}}, .input = -1, .parent = -1, .directory = -1};
    assert(argc >= 0);
    assert(argv != NULL);
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || !protocol_channels()) {
        return 125;
    }
    if (!arguments(argc, argv, &state)) {
        failure(&state, SUP_INVALID);
    } else if (!signals()) {
        failure(&state, SUP_REAPER);
    } else {
        execute(&state, argv);
    }
    return publish(&state);
}
