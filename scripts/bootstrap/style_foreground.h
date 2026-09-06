/** Private final-checker mode: held directory identity, inherited stdio and no
 * build-output limits. */
#include <sys/stat.h>

typedef struct {
    int parent, directory;
    struct stat identity;
    char name[PATH_MAX];
} RunDirectory;

/** Require a private directory owned by this user. @param fd Directory fd.
 * @return Validity. */
static bool private_directory(int fd) {
    assert(fd >= 0);
    struct stat status;
    if (fstat(fd, &status) != 0) {
        return false;
    }
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0) {
        return false;
    }
    assert((flags & FD_CLOEXEC) != 0);
    return S_ISDIR(status.st_mode) && status.st_uid == geteuid() && (status.st_mode & 077) == 0;
}

/** Read the fixed ownership marker without following a link or accepting
 * trailing bytes.
 * @param directory Held directory. @return Exact owned marker validity.
 */
static bool run_marker(int directory) {
    assert(directory >= 0);
    const char expected[] = "FERN_STYLE_RUN_V1\n";
    int fd = openat(directory, "owner", O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }
    struct stat status;
    bool valid = fstat(fd, &status) == 0 && S_ISREG(status.st_mode) && status.st_uid == geteuid();
    char bytes[sizeof(expected)];
    ssize_t count = valid ? read(fd, bytes, sizeof(bytes)) : -1;
    assert(sizeof(bytes) > sizeof(expected) - 1);
    valid = valid && count == sizeof(expected) - 1 &&
            memcmp(bytes, expected, sizeof(expected) - 1) == 0;
    return close(fd) == 0 && valid;
}

/** Hold both directory identities before launching its exact program file.
 * @param run Empty descriptors. @param path Absolute private run path.
 * @param program Exact executable.
 * @return Ownership and shape validity; caller always closes held descriptors.
 */
static bool hold_run(RunDirectory *run, const char *path, const char *program) {
    assert(run != NULL && path != NULL);
    assert(program != NULL);
    size_t length = strnlen(path, PATH_MAX - 9);
    if (path[0] != '/' || length == PATH_MAX - 9) {
        return false;
    }
    char parent[PATH_MAX], expected[PATH_MAX];
    memcpy(parent, path, length + 1);
    char *last = strrchr(parent, '/');
    if (!last || strncmp(last + 1, "run.", 4) != 0 || strlen(last + 1) <= 4) {
        return false;
    }
    strcpy(run->name, last + 1);
    *last = 0;
    snprintf(expected, sizeof(expected), "%s/program", path);
    if (strcmp(expected, program) != 0) {
        return false;
    }
    run->parent = open(parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (run->parent < 0 || !private_directory(run->parent)) {
        return false;
    }
    run->directory =
        openat(run->parent, run->name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (run->directory < 0 || !private_directory(run->directory) || !run_marker(run->directory)) {
        return false;
    }
    struct stat executable;
    return fstat(run->directory, &run->identity) == 0 &&
           fstatat(run->directory, "program", &executable, AT_SYMLINK_NOFOLLOW) == 0 &&
           S_ISREG(executable.st_mode) && executable.st_uid == geteuid();
}

/** Close held directory descriptors exactly once, without altering the primary
 * result.
 * @param run Owned slots. @return Whether both closes succeeded.
 */
static bool close_run(RunDirectory *run) {
    assert(run != NULL);
    assert(run->directory >= -1 && run->parent >= -1);
    bool valid = true;
    if (run->directory >= 0 && close(run->directory) != 0) {
        valid = false;
    }
    if (run->parent >= 0 && close(run->parent) != 0) {
        valid = false;
    }
    run->directory = run->parent = -1;
    return valid;
}

/** Delete only fixed owned entries; a renamed/replaced directory is never
 * removed by path.
 * @param run Validated held identities. @return Whether exact cleanup
 * completed.
 */
static bool remove_run(RunDirectory *run) {
    assert(run != NULL);
    assert(run->directory >= 0 && run->parent >= 0);
    bool valid = unlinkat(run->directory, "program", 0) == 0;
    if (unlinkat(run->directory, "owner", 0) != 0) {
        valid = false;
    }
    if (unlinkat(run->directory, "supervisor", 0) != 0 && errno != ENOENT) {
        valid = false;
    }
    struct stat current;
    if (fstatat(run->parent, run->name, &current, AT_SYMLINK_NOFOLLOW) != 0 ||
        current.st_dev != run->identity.st_dev || current.st_ino != run->identity.st_ino) {
        valid = false;
    } else if (unlinkat(run->parent, run->name, AT_REMOVEDIR) != 0) {
        valid = false;
    }
    return close_run(run) && valid;
}

/** Spawn the final checker with inherited stdio and independent signal/group
 * state.
 * @param state Operation. @param argv Validated executable arguments.
 */
static void spawn_foreground(Supervisor *state, char **argv) {
    assert(state != NULL);
    assert(argv != NULL);
    posix_spawnattr_t attributes;
    if (posix_spawnattr_init(&attributes) != 0) {
        failure(state, SUP_IO);
        return;
    }
    int error = spawn_attributes(&attributes);
    if (error) {
        failure(state, SUP_IO);
    } else if (remaining(state) > 0 && state->error == SUP_OK) {
        error = posix_spawn(&state->child, argv[0], NULL, &attributes, argv, environ);
        if (error) {
            failure(state, SUP_SPAWN);
        } else {
            state->retained = true;
        }
    }
    if (posix_spawnattr_destroy(&attributes) != 0) {
        failure(state, SUP_IO);
    }
}

/** Wait for final CLI completion; only explicit interruption initiates early
 * cleanup.
 * @param state Retained child, with no build deadline or output capture policy.
 */
static void foreground_wait(Supervisor *state) {
    assert(state != NULL);
    assert(state->deadline == INT64_MAX);
    while (state->retained && !state->finished && state->error == SUP_OK) {
        remaining(state);
        observe(state);
        if (poll(NULL, 0, 10) < 0 && errno != EINTR) {
            failure(state, SUP_IO);
        }
    }
}

/** Select the first nonzero native/interruption status before performing
 * directory cleanup.
 * @param state Completed/interrupted child. @param run Held invocation
 * directory. @return Exit status.
 */
static int foreground_finish(Supervisor *state, RunDirectory *run) {
    assert(state != NULL);
    assert(run != NULL);
    int primary = state->error == SUP_OK       ? state->status
                  : state->error == SUP_SIGNAL ? 128 + state->signal
                                               : 125;
    cleanup_child(state);
    if (!remove_run(run)) {
        failure(state, SUP_IO);
    }
    int status = result(state);
    return primary != 0 ? primary : status;
}

/** Validate the private foreground protocol, preserving ordinary checker argv
 * unchanged.
 * @param argc OS count. @param argv Run directory followed by literal command.
 * @return Native status.
 */
static int foreground(int argc, char **argv) {
    assert(argc >= 0);
    assert(argv != NULL);
    Supervisor state = {.streams = {{-1, -1, 0}, {-1, -1, 0}}, .input = -1};
    if (argc < 5 || argc > 4100) {
        failure(&state, SUP_INVALID);
        return result(&state);
    }
    char *checked[4101];
    for (int i = 0; i < argc; i++) {
        checked[i] = argv[i];
    }
    checked[argc] = NULL;
    checked[1] = "600000";
    checked[2] = "0";
    RunDirectory run = {.parent = -1, .directory = -1};
    if (!arguments(argc, checked, &state) || !hold_run(&run, argv[2], argv[4])) {
        close_run(&run);
        failure(&state, SUP_INVALID);
        return result(&state);
    }
    state.deadline = INT64_MAX;
    if (!signals()) {
        failure(&state, SUP_REAPER);
    } else {
        spawn_foreground(&state, argv + 4);
        foreground_wait(&state);
    }
    return foreground_finish(&state, &run);
}
