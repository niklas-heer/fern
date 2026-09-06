/** Cold-launch controller owns the retained worker; its handoff is literal
 * private data. */
typedef struct {
    int directory, parent, saved[3];
    bool owned, started, logs[2];
    char name[PATH_MAX];
    struct stat identity;
    char run_path[PATH_MAX];
    RunDirectory run;
} Launch;

/** Read one bounded regular owned file through a held directory.
 * @param directory Held fd.
 * @param name Fixed file. @param bytes Output storage. @param capacity Bound.
 * @return Bytes or -1.
 */
static ssize_t launch_read(int directory, const char *name, char bytes[], size_t capacity) {
    assert(directory >= 0 && name != NULL);
    assert(bytes != NULL && capacity > 0);
    int fd = openat(directory, name, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    struct stat status;
    bool valid = fstat(fd, &status) == 0 && S_ISREG(status.st_mode) && status.st_uid == geteuid() &&
                 (status.st_mode & 022) == 0 && status.st_size >= 0 &&
                 (uint64_t)status.st_size < capacity;
    size_t used = 0;
    bool ended = false;
    for (unsigned attempts = 0; valid && attempts < 65536; attempts++) {
        ssize_t count = read(fd, bytes + used, capacity - used);
        if (count == 0) {
            ended = true;
            break;
        }
        if (count < 0) {
            valid = errno == EINTR;
        } else {
            used += (size_t)count;
            valid = used < capacity;
        }
    }
    valid = valid && ended && used == (uint64_t)status.st_size;
    if (close(fd) != 0 || !valid) {
        return -1;
    }
    return (ssize_t)used;
}

/** Pin invocation identity and exact marker before running any worker.
 * @param launch Empty state.
 * @param path Absolute directory. @return Whether ownership and protocol are
 * valid.
 */
static bool launch_hold(Launch *launch, const char *path) {
    assert(launch != NULL);
    assert(path != NULL);
    if (path[0] != '/' || strnlen(path, PATH_MAX) == PATH_MAX) {
        return false;
    }
    const char *name = strrchr(path, '/');
    if (!name || strncmp(name + 1, "work.", 5) != 0 || strlen(name + 1) <= 5) {
        return false;
    }
    launch->directory = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (launch->directory >= 0) {
        launch->directory = move_fd(launch->directory);
    }
    if (launch->directory < 0 || !private_directory(launch->directory)) {
        return false;
    }
    char marker[64];
    const char expected[] = "FERN_STYLE_WORK_V1\n";
    ssize_t size = launch_read(launch->directory, "owner", marker, sizeof(marker));
    if (size != sizeof(expected) - 1 || memcmp(marker, expected, sizeof(expected) - 1) != 0 ||
        fstat(launch->directory, &launch->identity) != 0) {
        return false;
    }
    struct stat existing;
    if (fstatat(launch->directory, "handoff", &existing, AT_SYMLINK_NOFOLLOW) == 0 ||
        errno != ENOENT) {
        return false;
    }
    char parent[PATH_MAX];
    memcpy(parent, path, (size_t)(name - path));
    parent[name - path] = 0;
    strcpy(launch->name, name + 1);
    launch->parent = open(parent, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (launch->parent >= 0) {
        launch->parent = move_fd(launch->parent);
    }
    launch->owned = launch->parent >= 0 && private_directory(launch->parent);
    return launch->owned;
}

/** Preserve open/closed stdio and redirect build output to exclusive private
 * regular files.
 * @param launch Held invocation. @return Whether all descriptors were prepared.
 */
static bool launch_logs(Launch *launch) {
    assert(launch != NULL);
    assert(launch->directory >= 0);
    for (int i = 0; i < 3; i++) {
        launch->saved[i] = fcntl(i, F_DUPFD_CLOEXEC, 10);
        if (launch->saved[i] < 0 && errno != EBADF) {
            return false;
        }
    }
    const char *names[] = {"launch.stdout", "launch.stderr"};
    for (int i = 0; i < 2; i++) {
        int fd = openat(launch->directory, names[i],
                        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd < 0) {
            return false;
        }
        launch->logs[i] = true;
        bool valid = dup2(fd, i + 1) >= 0;
        if (fd != i + 1 && close(fd) != 0) {
            valid = false;
        }
        if (!valid) {
            return false;
        }
    }
    return true;
}

/** Restore stdio, including original closed descriptors, without changing the
 * first failure.
 * @param launch Owned saved descriptors. @param state Worker result.
 */
static void launch_restore(Launch *launch, Supervisor *state) {
    assert(launch != NULL);
    assert(state != NULL);
    for (int i = 0; i < 3; i++) {
        if (launch->saved[i] >= 0) {
            if (dup2(launch->saved[i], i) < 0 || close(launch->saved[i]) != 0) {
                failure(state, SUP_IO);
            }
        } else if (close(i) != 0 && errno != EBADF) {
            failure(state, SUP_IO);
        }
        launch->saved[i] = -1;
    }
}

/** Match decimal filesystem identity fields without accepting trailing or
 * executable text.
 * @param line Literal dev:inode line. @param status Actual metadata. @return
 * Exact match.
 */
static bool launch_identity(const char *line, const struct stat *status) {
    assert(line != NULL);
    assert(status != NULL);
    const char *colon = strchr(line, ':');
    if (!colon || colon - line >= 20) {
        return false;
    }
    char prefix[20];
    memcpy(prefix, line, (size_t)(colon - line));
    prefix[colon - line] = 0;
    uint64_t device, inode;
    return number(prefix, UINT64_MAX, &device) && number(colon + 1, UINT64_MAX, &inode) &&
        device == (uint64_t)status->st_dev && inode == (uint64_t)status->st_ino;
}

/** Require executable identities from the completed worker, rejecting
 * replacement/symlinks.
 * @param launch Held run. @param lines Directory/program/supervisor identity
 * lines. @return Validity.
 */
static bool launch_artifacts(Launch *launch, const char *lines[]) {
    assert(launch != NULL);
    assert(lines != NULL);
    if (!launch_identity(lines[0], &launch->run.identity)) {
        return false;
    }
    const char *names[] = {"program", "supervisor"};
    for (unsigned i = 0; i < 2; i++) {
        struct stat status;
        if (fstatat(launch->run.directory, names[i], &status, AT_SYMLINK_NOFOLLOW) != 0 ||
            !S_ISREG(status.st_mode) || status.st_uid != geteuid() || (status.st_mode & 022) != 0 ||
            (status.st_mode & 0100) == 0 || !launch_identity(lines[i + 1], &status)) {
            return false;
        }
    }
    return true;
}

/** Decode exactly four newline-delimited fields from the held invocation's
 * completed handoff.
 * @param launch Owned directories. @param work Original path. @return Valid
 * final artifact identity.
 */
static bool launch_handoff(Launch *launch, const char *work) {
    assert(launch != NULL);
    assert(work != NULL);
    char bytes[PATH_MAX + 128];
    ssize_t size = launch_read(launch->directory, "handoff", bytes, sizeof(bytes));
    if (size < 1 || memchr(bytes, 0, (size_t)size) || memchr(bytes, '\r', (size_t)size)) {
        return false;
    }
    bytes[size] = 0;
    char *fields[4], *next = bytes;
    for (unsigned i = 0; i < 4; i++) {
        fields[i] = next;
        char *end = strchr(next, '\n');
        if (!end) {
            return false;
        }
        *end = 0;
        next = end + 1;
    }
    size_t parent = (size_t)(strrchr(work, '/') - work) + 1;
    if (*next || strlen(fields[0]) >= PATH_MAX - 9 || strncmp(fields[0], work, parent) != 0 ||
        strchr(fields[0] + parent, '/')) {
        return false;
    }
    strcpy(launch->run_path, fields[0]);
    char program[PATH_MAX];
    snprintf(program, sizeof(program), "%s/program", launch->run_path);
    const char *identities[] = {fields[1], fields[2], fields[3]};
    return hold_run(&launch->run, launch->run_path, program) &&
           launch_artifacts(launch, identities);
}

/** Drain the worker using existing bounded capture and retained-child cleanup
 * rules.
 * @param state Prepared operation. @param argv Literal worker argv.
 */
static void launch_worker(Supervisor *state, char *argv[]) {
    assert(state != NULL);
    assert(argv != NULL);
    if (!descriptors(state)) {
        failure(state, SUP_IO);
    }
    if (state->error == SUP_OK) {
        spawn_child(state, argv);
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
    remaining(state);
    if (state->status != 0) {
        failure(state, SUP_IO);
    }
}

/** Delete fixed created files and remove the held invocation only if its
 * original name still agrees.
 * @param launch Held identities. @param state First result; cleanup never
 * replaces a prior failure.
 */
static void launch_release(Launch *launch, Supervisor *state) {
    assert(launch != NULL);
    assert(state != NULL);
    if (launch->directory >= 0 && launch->owned) {
        const char *names[] = {"launch.stdout", "launch.stderr", "handoff",     "owner",
                               "supervisor",    "seed.stdout",   "seed.stderr", "retry"};
        for (unsigned i = 0; i < 8; i++) {
            bool owned = i < 2 ? launch->logs[i] : launch->started;
            if (owned && unlinkat(launch->directory, names[i], 0) != 0 && errno != ENOENT) {
                failure(state, SUP_IO);
            }
        }
        struct stat current;
        if (launch->started &&
            (fstatat(launch->parent, launch->name, &current, AT_SYMLINK_NOFOLLOW) != 0 ||
             current.st_dev != launch->identity.st_dev ||
             current.st_ino != launch->identity.st_ino ||
             unlinkat(launch->parent, launch->name, AT_REMOVEDIR) != 0)) {
            failure(state, SUP_IO);
        }
    }
    if (launch->directory >= 0 && close(launch->directory) != 0) {
        failure(state, SUP_IO);
    }
    if (launch->parent >= 0 && close(launch->parent) != 0) {
        failure(state, SUP_IO);
    }
    launch->directory = launch->parent = -1;
}

/** Check final argv bounds before any build work, without interpreting user
 * arguments.
 * @param argc OS count. @param argv Controller arguments. @param state Output
 * deadline/cap.
 * @return Valid final argv and finite worker settings.
 */
static bool launch_arguments(int argc, char **argv, Supervisor *state) {
    assert(argv != NULL);
    assert(state != NULL);
    if (argc < 8 || argc > 4103 || strcmp(argv[7], "--") != 0) {
        return false;
    }
    char *checked[4100] = {argv[0], "120000", "16777216", "--", "/program"};
    for (int i = 8; i < argc; i++) {
        checked[i - 3] = argv[i];
    }
    return arguments(argc - 3, checked, state);
}

/** Publish at most two chunks of useful build diagnostics after restoring the
 * caller's stderr.
 * @param launch Held logs. @param state First failure, never replaced by
 * diagnostic IO.
 */
static void launch_diagnostics(Launch *launch, Supervisor *state) {
    assert(launch != NULL);
    assert(state != NULL && state->error != SUP_OK);
    const char *names[] = {"launch.stderr", "launch.stdout"};
    for (unsigned i = 0; i < 2; i++) {
        int fd = openat(launch->directory, names[i], O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        struct stat status;
        if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0) {
            (void)close(fd);
            continue;
        }
        if (status.st_size > CHUNK) {
            const char note[] = "fern style: build log truncated (last 4096 bytes)\n";
            forward(state, 2, note, sizeof(note) - 1);
            if (lseek(fd, -(off_t)CHUNK, SEEK_END) < 0) {
                (void)close(fd);
                continue;
            }
        }
        char bytes[CHUNK];
        ssize_t count = read(fd, bytes, sizeof(bytes));
        if (count > 0) {
            forward(state, 2, bytes, (size_t)count);
        }
        (void)close(fd);
    }
}

/** Run one bounded cold worker, then the checker with original stdio and
 * literal arguments.
 * @param argc OS argument count. @param argv Fixed controller protocol and
 * checker arguments.
 * @return Exact native status, interrupted status, or bootstrap 125.
 */
static int launch(int argc, char **argv) {
    assert(argc >= 0);
    assert(argv != NULL);
    Supervisor state = {.streams = {{-1, -1, 0}, {-1, -1, 0}}, .input = -1};
    Launch context = {.directory = -1,
                      .parent = -1,
                      .saved = {-1, -1, -1},
                      .run = {.parent = -1, .directory = -1}};
    if (!launch_arguments(argc, argv, &state)) {
        failure(&state, SUP_INVALID);
        return result(&state);
    }
    char *command[] = {"/bin/bash", argv[3], argv[2], argv[4], argv[5], argv[6], NULL};
    char *checked[] = {argv[0],    "120000",   "16777216", "--",       command[0],
                       command[1], command[2], command[3], command[4], command[5]};
    if (!arguments(10, checked, &state) || !launch_hold(&context, argv[2]) || !signals()) {
        failure(&state, SUP_INVALID);
        launch_release(&context, &state);
        return result(&state);
    }
    bool logging = launch_logs(&context);
    if (logging) {
        context.started = true;
        launch_worker(&state, command);
    } else {
        failure(&state, SUP_IO);
    }
    launch_restore(&context, &state);
    if (state.error == SUP_OK && !launch_handoff(&context, argv[2])) {
        failure(&state, SUP_INVALID);
    }
    if (state.error != SUP_OK) {
        launch_diagnostics(&context, &state);
    }
    launch_release(&context, &state);
    if (state.error != SUP_OK) {
        close_run(&context.run);
        return result(&state);
    }
    state.deadline = INT64_MAX;
    state.status = 0;
    state.finished = false;
    char program[PATH_MAX];
    snprintf(program, sizeof(program), "%s/program", context.run_path);
    char *final[4097];
    final[0] = program;
    for (int i = 8; i < argc; i++) {
        final[i - 7] = argv[i];
    }
    final[argc - 7] = NULL;
    spawn_foreground(&state, final);
    foreground_wait(&state);
    return foreground_finish(&state, &context.run);
}
