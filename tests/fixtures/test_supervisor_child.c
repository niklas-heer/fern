/** Finite supervised-child oracles with explicit descendant readiness. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define assert(x)                                                                                  \
    do {                                                                                           \
        if (!(x))                                                                                  \
            _exit(90);                                                                             \
    } while (0)

/** Write a finite fixture payload, retrying only interrupted writes.
 * @param fd Output descriptor. @param text Bytes. @param length Byte count.
 */
static void bytes(int fd, const char *text, size_t length) {
    assert(fd >= 0);
    assert(text != NULL);
    for (size_t offset = 0; offset < length;) {
        ssize_t count = write(fd, text + offset, length - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        assert(count > 0);
        offset += (size_t)count;
    }
}

/** Sleep for a finite duration. @param ms Duration in milliseconds, at most
 * 3000. */
static void pause_ms(long ms) {
    assert(ms >= 0);
    assert(ms <= 3000);
    struct timespec remaining = {ms / 1000, (ms % 1000) * 1000000};
    for (unsigned attempts = 0; attempts < 128; attempts++) {
        if (nanosleep(&remaining, &remaining) == 0) {
            return;
        }
        assert(errno == EINTR);
    }
    assert(0);
}

/** Publish a ready descendant before allowing its leader to exit or pause.
 * @param path Readiness file. @param leader_waits Whether the direct child
 * pauses.
 */
static void descendant(const char *path, int leader_waits) {
    int ready[2];
    assert(pipe(ready) == 0);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        if (leader_waits == 2) {
            assert(setsid() >= 0);
        }
        close(ready[0]);
        int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        assert(fd >= 0);
        char text[40];
        int n = snprintf(text, sizeof(text), "%ld\n", (long)getpid());
        bytes(fd, text, (size_t)n);
        assert(close(fd) == 0);
        bytes(ready[1], "r", 1);
        close(ready[1]);
        pause_ms(3000);
        _exit(0);
    }
    close(ready[1]);
    char marker;
    assert(read(ready[0], &marker, 1) == 1);
    close(ready[0]);
    if (leader_waits == 1) {
        pause_ms(3000);
    }
}

/** Execute bounded foreground-specific fd, signal and readiness fixtures.
 * @param argc Count. @param argv Fixture arguments. @return Status, or -1 for
 * another mode.
 */
static int foreground_fixture(int argc, char **argv) {
    assert(argc >= 2);
    assert(argv != NULL);
    if (strcmp(argv[1], "leaks") == 0) {
        for (int fd = 3; fd < 4096; fd++) {
            assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
        }
        return 0;
    }
    if (strcmp(argv[1], "fds") == 0) {
        int mask = 0;
        for (int fd = 0; fd < 3; fd++) {
            if (fcntl(fd, F_GETFD) == -1 && errno == EBADF) {
                mask |= 1 << fd;
            }
        }
        return mask;
    }
    if (strcmp(argv[1], "stdin") == 0) {
        char buffer[32];
        ssize_t count = read(0, buffer, sizeof(buffer));
        assert(count >= 0);
        bytes(1, buffer, (size_t)count);
        return 7;
    }
    if (strcmp(argv[1], "die") == 0) {
        raise(SIGTERM);
        return 90;
    }
    if (strcmp(argv[1], "wait_exit") == 0) {
        assert(argc == 5);
        int fd = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0600);
        assert(fd >= 0);
        bytes(fd, "ready\n", 6);
        assert(close(fd) == 0);
        for (unsigned attempt = 0; attempt < 2000; attempt++) {
            if (access(argv[3], F_OK) == 0) {
                return atoi(argv[4]);
            }
            pause_ms(1);
        }
        return 90;
    }
    return -1;
}

/** Execute one finite mode; all descendants expire even if supervision is
 * broken.
 * @param argc Count. @param argv Fixture arguments. @return Explicit status or
 * 90 on failed assertion.
 */
int main(int argc, char **argv) {
    assert(argc >= 2);
    int selected = foreground_fixture(argc, argv);
    if (selected >= 0) {
        return selected;
    }
    if (strcmp(argv[1], "exit") == 0) {
        assert(argc == 3);
        return atoi(argv[2]);
    }
    if (strcmp(argv[1], "streams") == 0) {
        bytes(1, "out\0🌿", 8);
        bytes(2, "err\n", 4);
        return 7;
    }
    if (strcmp(argv[1], "args") == 0) {
        char eof;
        assert(read(0, &eof, 1) == 0);
        for (int i = 2; i < argc; i++) {
            char count[40];
            int n = snprintf(count, sizeof(count), "%zu:", strlen(argv[i]));
            bytes(1, count, (size_t)n);
            bytes(1, argv[i], strlen(argv[i]));
            bytes(1, "\n", 1);
        }
        return 0;
    }
    if (strcmp(argv[1], "dual") == 0) {
        char buffer[4096];
        memset(buffer, 'x', sizeof(buffer));
        for (unsigned i = 0; i < 100; i++) {
            bytes(1, buffer, sizeof(buffer));
            bytes(2, buffer, sizeof(buffer));
        }
        return 0;
    }
    if (strcmp(argv[1], "emit") == 0) {
        assert(argc == 4);
        size_t size = (size_t)strtoul(argv[2], NULL, 10);
        assert(size <= 1000000);
        int fd = atoi(argv[3]);
        char buffer[1024];
        memset(buffer, 'x', sizeof(buffer));
        for (size_t left = size; left;) {
            size_t n = left < sizeof(buffer) ? left : sizeof(buffer);
            bytes(fd, buffer, n);
            left -= n;
        }
        return 0;
    }
    if (strcmp(argv[1], "escaped") == 0) {
        assert(argc == 3);
        descendant(argv[2], 2);
        return 0;
    }
    if (strcmp(argv[1], "close_wait") == 0) {
        close(1);
        close(2);
        pause_ms(3000);
        return 0;
    }
    if (strcmp(argv[1], "descendant") == 0 || strcmp(argv[1], "descendant_wait") == 0) {
        assert(argc == 3);
        descendant(argv[2], strcmp(argv[1], "descendant_wait") == 0);
        return 0;
    }
    assert(0);
    return 90;
}
