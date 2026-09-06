/** Deterministic syscall seam tests; no fake PID is ever sent to the operating system. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#endif
static ssize_t fake_read(int, void *, size_t);
static int fake_kill(pid_t, int);
static pid_t fake_waitpid(pid_t, int *, int);
static int fake_waitid(idtype_t, id_t, siginfo_t *, int);
#ifdef __APPLE__
static int fake_proc_listpids(uint32_t, uint32_t, void *, int);
#define proc_listpids fake_proc_listpids
#endif
#define read fake_read
#define kill fake_kill
#define waitpid fake_waitpid
#define waitid fake_waitid
#define main supervisor_main
#include "../../scripts/bootstrap/style_supervisor.c"
#undef main
#undef read
#undef assert
/* Keep fixture checks active when the production translation unit uses NDEBUG. */
#define assert(x)                                                                                  \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "state check failed at %d: %s\n", __LINE__, #x);                       \
            exit(90);                                                                              \
        }                                                                                          \
    } while (0)
static int killed, reaped, kill_error, observe_error, membership, interrupted_wait;

static int retry_reads;

/** Inject read interruptions before allowing actual buffered pipe reads.
 * @param fd Input fd. @param buffer Bytes. @param size Capacity. @return Actual read or EINTR.
 */
static ssize_t fake_read(int fd, void *buffer, size_t size) {
    assert(fd >= 0);
    assert(buffer != NULL);
    if (retry_reads > 0) {
        retry_reads--;
        errno = EINTR;
        return -1;
    }
    return read(fd, buffer, size);
}

/** Interrupted post-cleanup reads must not masquerade as EOF and lose successful bytes. */
static void drain_interruptions(void) {
    Supervisor state = {.streams = {{-1, -1, 0}, {-1, -1, 0}}, .input = -1, .limit = 4};
    state.deadline = milliseconds() + 1000;
    assert(capture_pipe(&state.streams[0]));
    assert(write(state.streams[0].writer, "done", 4) == 4);
    close_owned(&state, &state.streams[0].writer);
    retry_reads = 2;
    finish_streams(&state);
    assert(state.error == SUP_OK);
    assert(state.streams[0].length == 4);
    assert(retry_reads == 0);
}

/** A repeated interrupted read cannot spin indefinitely during post-cleanup drain. */
static void drain_retry_bound(void) {
    Supervisor state = {.streams = {{-1, -1, 0}, {-1, -1, 0}}, .input = -1, .limit = 4};
    state.deadline = milliseconds() + 1000;
    assert(capture_pipe(&state.streams[0]));
    retry_reads = 70000;
    finish_streams(&state);
    assert(state.error == SUP_IO);
    assert(retry_reads > 0);
    assert(state.streams[0].reader == -1);
    retry_reads = 0;
}

/** Count group signaling without touching any actual process.
 * @param pid Expected negative PGID. @param signal Expected kill signal. @return Simulated status.
 */
static int fake_kill(pid_t pid, int signal) {
    assert(pid == -12345);
    assert(signal == SIGKILL);
    assert(reaped == 0);
    killed++;
    errno = kill_error;
    return kill_error ? -1 : 0;
}

/** Verify signal-before-reap ordering, including interrupted kernel waits.
 * @param pid Expected child. @param status Output. @param flags Expected flags. @return Child or
 * -1.
 */
static pid_t fake_waitpid(pid_t pid, int *status, int flags) {
    assert(pid == 12345);
    assert(flags == 0);
    assert(killed == 1);
    if (interrupted_wait) {
        interrupted_wait = 0;
        errno = EINTR;
        return -1;
    }
    reaped++;
    *status = 0;
    return pid;
}

/** Emulate exact-child WNOWAIT observation or lost ownership.
 * @param type PID selector. @param id Child identity. @param info Output. @param flags Wait flags.
 * @return Simulated status.
 */
static int fake_waitid(idtype_t type, id_t id, siginfo_t *info, int flags) {
    assert(type == P_PID);
    assert(id == 12345);
    assert(flags & WNOWAIT);
    if (observe_error) {
        errno = observe_error;
        return -1;
    }
    info->si_pid = 12345;
    info->si_code = CLD_EXITED;
    info->si_status = 127;
    return 0;
}
#ifdef __APPLE__
/** Emulate complete singleton, incomplete or unverified group membership.
 * @param type Group selector. @param id Group identity. @param buffer Output. @param size Capacity.
 * @return Copied bytes.
 */
static int fake_proc_listpids(uint32_t type, uint32_t id, void *buffer, int size) {
    assert(type == PROC_PGRP_ONLY);
    assert(id == 12345);
    assert(size == 2 * sizeof(pid_t));
    ((pid_t *)buffer)[0] = 12345;
    errno = 0;
    return membership * (int)sizeof(pid_t);
}
#endif

/** Exercise ownership loss and first-failure preservation independently of scheduler timing. */
static void ownership(void) {
    Supervisor state = {.child = 12345, .retained = true};
    observe_error = ECHILD;
    observe(&state);
    cleanup_child(&state);
    assert(!state.retained);
    assert(killed == 0 && reaped == 0);
    assert(state.error == SUP_IO);
    observe_error = 0;
    state = (Supervisor){.child = 12345, .retained = true};
    observe(&state);
    assert(state.finished && state.retained);
    assert(state.status == 127);
    interrupted_wait = 1;
    cleanup_child(&state);
    assert(killed == 1 && reaped == 1);
    assert(!state.retained);
    state.error = SUP_CAP;
    state.deadline = milliseconds() + 1000;
    interrupted = SIGTERM;
    remaining(&state);
    assert(state.error == SUP_CAP);
    assert(state.signal == SIGTERM);
    interrupted = 0;
}

/** A Darwin EPERM exception requires exactly one verified retained and exited child. */
static void permission(void) {
    for (membership = 0; membership <= 2; membership++) {
        killed = reaped = 0;
        kill_error = EPERM;
        Supervisor state = {.child = 12345, .retained = true, .finished = true};
        cleanup_child(&state);
#ifdef __APPLE__
        assert(state.error == (membership == 1 ? SUP_OK : SUP_IO));
#else
        assert(state.error == SUP_IO);
#endif
        assert(killed == 1 && reaped == 1);
    }
    killed = reaped = 0;
    membership = 1;
    Supervisor state = {.child = 12345, .retained = true, .error = SUP_TIME};
    cleanup_child(&state);
    assert(state.error == SUP_TIME);
    assert(killed == 1 && reaped == 1);
}

/** Verify automatic-reaping policy and numeric bounds without heap allocations.
 * @return Zero on success; assertions terminate with 90 on failure.
 */
/** Check argument byte bounds before OS ARG_MAX could obscure the supervisor contract. */
static void byte_bounds(void) {
    static char text[ARG_BYTES + 1];
    memset(text, 'x', sizeof(text));
    char *argv[] = {"supervisor", "1000", "0", "--", "/x", text, NULL};
    Supervisor state = {0};
    text[ARG_BYTES - 4] = 0;
    assert(arguments(6, argv, &state));
    text[ARG_BYTES - 4] = 'x';
    text[ARG_BYTES - 3] = 0;
    assert(!arguments(6, argv, &state));
}

/** Verify owned descriptor flags and repeated teardown without leaking any owned fd. */
static void file_descriptors(void) {
    for (unsigned attempt = 0; attempt < 128; attempt++) {
        Supervisor state = {.streams = {{-1, -1, 0}, {-1, -1, 0}}, .input = -1};
        state.deadline = milliseconds() + 1000;
        assert(descriptors(&state));
        int owned[] = {state.input, state.streams[0].reader, state.streams[0].writer,
                       state.streams[1].reader, state.streams[1].writer};
        for (unsigned i = 0; i < 5; i++) {
            assert(owned[i] >= 3);
            assert(fcntl(owned[i], F_GETFD) & FD_CLOEXEC);
        }
        assert(fcntl(state.streams[0].reader, F_GETFL) & O_NONBLOCK);
        assert(!(fcntl(state.streams[0].writer, F_GETFL) & O_NONBLOCK));
        finish_streams(&state);
        assert(state.error == SUP_OK);
        for (unsigned i = 0; i < 5; i++) {
            assert(fcntl(owned[i], F_GETFD) == -1 && errno == EBADF);
        }
    }
}

/** Verify private-state invariants without supervising real processes. @return Zero on success. */
int main(void) {
    drain_interruptions();
    drain_retry_bound();
    file_descriptors();
    byte_bounds();
    ownership();
    permission();
    struct sigaction old, action;
    memset(&action, 0, sizeof(action));
    action.sa_flags = SA_NOCLDWAIT;
    assert(sigemptyset(&action.sa_mask) == 0);
    assert(sigaction(SIGCHLD, &action, &old) == 0);
    assert(!signals());
    assert(sigaction(SIGCHLD, &old, NULL) == 0);
    uint64_t value = 0;
    assert(number("600000", 600000, &value) && value == 600000);
    assert(!number("600001", 600000, &value));
    assert(!number("18446744073709551615", 600000, &value));
    puts("private ownership and first-failure checks passed");
    return 0;
}
