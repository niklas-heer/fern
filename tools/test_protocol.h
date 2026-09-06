/** Private native-test spooling and version-one framed pipe publication.
 * Included only by test_supervisor.c, which owns all descriptors and buffers.
 */
static unsigned char published[2][LOG_LIMIT];

/** Validate a held private directory descriptor before relative operations.
 * @param fd Held descriptor. @return Whether ownership, type and mode match. */
static bool private_directory(int fd) {
    assert(fd >= 0);
    struct stat metadata;
    if (fstat(fd, &metadata) != 0) {
        return false;
    }
    assert(sizeof(metadata.st_mode) > 0);
    return S_ISDIR(metadata.st_mode) && metadata.st_uid == geteuid() &&
           (metadata.st_mode & 0777) == 0700;
}

/** Reject a closed or non-pipe protocol channel before spawning user code.
 * @return Both liveness stdin and framed stdout are pipes with correct access.
 */
static bool protocol_channels(void) {
    struct stat input, output;
    int in_flags = fcntl(0, F_GETFL), out_flags = fcntl(1, F_GETFL);
    assert(STDIN_FILENO == 0);
    assert(STDOUT_FILENO == 1);
    return in_flags >= 0 && out_flags >= 0 && fstat(0, &input) == 0 && fstat(1, &output) == 0 &&
           S_ISFIFO(input.st_mode) && S_ISFIFO(output.st_mode) &&
           (in_flags & O_ACCMODE) == O_RDONLY && (out_flags & O_ACCMODE) == O_WRONLY;
}

/** Create only exclusive fixed files below one held invocation-owned directory.
 * @param state Empty file slots. @param path Private parent directory path.
 * @return Success; partial ownership is recorded for cleanup on failure. */
static bool spool_files(Supervisor *state, const char *path) {
    assert(state != NULL);
    assert(state->parent == -1 && state->directory == -1);
    state->parent = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (state->parent < 0 || !private_directory(state->parent) ||
        mkdirat(state->parent, "capture", 0700) != 0) {
        return false;
    }
    state->created = true;
    state->directory = openat(state->parent, "capture",
                              O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (state->directory < 0 || !private_directory(state->directory) ||
        fstat(state->directory, &state->identity) != 0) {
        return false;
    }
    const char *names[2] = {"stdout", "stderr"};
    for (unsigned i = 0; i < 2; i++) {
        state->streams[i].file =
            openat(state->directory, names[i],
                   O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
        if (state->streams[i].file < 0) {
            return false;
        }
    }
    return true;
}

/** Validate a held spool inode, including its exact completed stream length.
 * @param stream Captured stream. @param metadata Output inode identity.
 * @return Whether the file is an owned, private, single-link regular file. */
static bool spool_valid(const SupStream *stream, struct stat *metadata) {
    assert(stream != NULL);
    assert(metadata != NULL);
    return fstat(stream->file, metadata) == 0 && S_ISREG(metadata->st_mode) &&
           metadata->st_uid == geteuid() && (metadata->st_mode & 0777) == 0600 &&
           metadata->st_nlink == 1 && metadata->st_size >= 0 &&
           (uint64_t)metadata->st_size == stream->length;
}

/** Copy a bounded completed stream into fixed storage before closing and
 * publishing.
 * @param state Completed operation. @param index Stream number, zero or one. */
static void load_spool(Supervisor *state, unsigned index) {
    assert(state != NULL);
    assert(index < 2);
    SupStream *stream = &state->streams[index];
    struct stat metadata;
    if (stream->file < 0 || !spool_valid(stream, &metadata)) {
        stream->length = 0;
        failure(state, SUP_IO);
        return;
    }
    size_t offset = 0;
    for (unsigned attempt = 0; offset < stream->length && attempt < 65536; attempt++) {
        ssize_t count =
            pread(stream->file, published[index] + offset, stream->length - offset, (off_t)offset);
        if (count > 0) {
            offset += (size_t)count;
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    if (offset != stream->length) {
        stream->length = 0;
        failure(state, SUP_IO);
    }
}

/** Unlink only a fixed name still referring to the held spool, then close it.
 * @param state Operation. @param index Stream zero or one. */
static void remove_spool(Supervisor *state, unsigned index) {
    assert(state != NULL);
    assert(index < 2);
    SupStream *stream = &state->streams[index];
    if (stream->file < 0) {
        return;
    }
    struct stat held, named;
    const char *name = index == 0 ? "stdout" : "stderr";
    if (fstat(stream->file, &held) != 0 ||
        fstatat(state->directory, name, &named, AT_SYMLINK_NOFOLLOW) != 0 ||
        named.st_dev != held.st_dev || named.st_ino != held.st_ino ||
        unlinkat(state->directory, name, 0) != 0) {
        failure(state, SUP_IO);
    }
    close_owned(state, &stream->file);
}

/** Clean fixed files using held identity; never recursively remove an input
 * path.
 * @param state Partial or complete invocation; primary failure is preserved. */
static void remove_spools(Supervisor *state) {
    assert(state != NULL);
    assert(!state->retained);
    for (unsigned i = 0; i < 2; i++) {
        remove_spool(state, i);
    }
    if (state->created && state->directory >= 0) {
        struct stat named;
        if (fstatat(state->parent, "capture", &named, AT_SYMLINK_NOFOLLOW) != 0 ||
            named.st_dev != state->identity.st_dev || named.st_ino != state->identity.st_ino ||
            unlinkat(state->parent, "capture", AT_REMOVEDIR) != 0) {
            failure(state, SUP_IO);
        }
    }
    close_owned(state, &state->directory);
    close_owned(state, &state->parent);
}

/** Write a framed chunk with nonblocking pipe IO and a separate publication
 * deadline.
 * @param data Bytes. @param length Bounded count. @param deadline Absolute
 * millisecond limit.
 * @param attempts Shared write/poll budget. @return Whether the entire chunk
 * was published. */
static bool publish_bytes(const void *data, size_t length, int64_t deadline, unsigned *attempts) {
    assert(data != NULL);
    assert(length <= LOG_LIMIT);
    size_t offset = 0;
    while (offset < length && ++*attempts <= 65536) {
        int64_t now = milliseconds();
        if (now < 0 || now >= deadline) {
            return false;
        }
        ssize_t count = write(1, (const char *)data + offset, length - offset);
        if (count > 0) {
            offset += (size_t)count;
        } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd output = {1, POLLOUT, 0};
            if (poll(&output, 1, deadline - now > 10 ? 10 : (int)(deadline - now)) < 0 &&
                errno != EINTR) {
                return false;
            }
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            return false;
        }
    }
    return offset == length;
}

/** Publish an unambiguous complete record after all cleanup has finished.
 * @param state Final result. @return Zero for a complete record, 125 on
 * transport failure. */
static int publish(const Supervisor *state) {
    assert(state != NULL);
    assert(!state->retained);
    int flags = fcntl(1, F_GETFL);
    int64_t now = milliseconds();
    if (flags < 0 || fcntl(1, F_SETFL, flags | O_NONBLOCK) != 0 || now < 0 ||
        now > INT64_MAX - PUBLICATION_MS) {
        return 125;
    }
    char header[128];
    int size = snprintf(header, sizeof(header), "FERN_TEST 1 %c %u %zu %zu\n",
                        state->error == SUP_OK ? 'N' : 'E',
                        state->error == SUP_OK ? (unsigned)state->status : (unsigned)state->error,
                        state->streams[0].length, state->streams[1].length);
    if (size < 0 || (size_t)size >= sizeof(header)) {
        return 125;
    }
    unsigned attempts = 0;
    int64_t deadline = now + PUBLICATION_MS;
    const char trailer[] = "\nFERN_TEST_END 1\n";
    if (!publish_bytes(header, (size_t)size, deadline, &attempts) ||
        !publish_bytes(published[0], state->streams[0].length, deadline, &attempts) ||
        !publish_bytes(published[1], state->streams[1].length, deadline, &attempts) ||
        !publish_bytes(trailer, sizeof(trailer) - 1, deadline, &attempts)) {
        return 125;
    }
    return 0;
}
