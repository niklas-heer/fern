#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
/** Standalone bounded lexical decoder; output is literal NUL-delimited
 * argv/path data. */
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define INPUT_LIMIT (1024u * 1024u)
#define WORD_LIMIT 16384u
static unsigned char input[INPUT_LIMIT + 1];

typedef struct {
    unsigned char word[WORD_LIMIT + 1];
    size_t length, count, limit;
    bool present, dependencies, includes, failed;
    unsigned char quote;
} Decoder;

/** Reject malformed input once; callers must discard all output on nonzero
 * exit.
 * @param state Decoder. @param reason Static diagnostic.
 */
static void reject(Decoder *state, const char *reason) {
    assert(state != NULL);
    assert(reason != NULL);
    if (!state->failed) {
        fprintf(stderr, "fern style: invalid build metadata: %s\n", reason);
    }
    state->failed = true;
}

/** Append one decoded non-control byte within the token bound.
 * @param state Decoder. @param byte Literal byte.
 */
static void append(Decoder *state, unsigned char byte) {
    assert(state != NULL);
    assert(state->length <= state->limit);
    if (byte == 0 || byte == '\r' || byte == '\n') {
        reject(state, "control byte");
    } else if (state->length == state->limit) {
        reject(state, "word too long");
    } else {
        state->word[state->length++] = byte;
        state->present = true;
    }
}

/** Publish one complete literal word after validating its count.
 * @param state Decoder; partial output is never a success result.
 */
static void emit(Decoder *state) {
    assert(state != NULL);
    assert(state->length <= state->limit);
    if (!state->present || state->failed) {
        return;
    }
    if (++state->count > ((state->dependencies || state->includes) ? 16384u : 4096u)) {
        reject(state, "too many words");
        return;
    }
    if (fwrite(state->word, 1, state->length, stdout) != state->length || fputc(0, stdout) == EOF) {
        reject(state, "output failed");
    }
    state->length = 0;
    state->present = false;
}

/** Consume a flag byte with lexical quoting only; expansions are never
 * performed.
 * @param state Decoder. @param index Input cursor. @param size Input length.
 */
static void flag_byte(Decoder *state, size_t *index, size_t size) {
    assert(state != NULL && index != NULL);
    assert(*index < size);
    unsigned char byte = input[*index];
    if (state->quote == '\'') {
        if (byte == '\'') {
            state->quote = 0;
        } else {
            append(state, byte);
        }
    } else if (byte == '\\') {
        if (++*index == size) {
            reject(state, "unterminated escape");
        } else {
            append(state, input[*index]);
        }
    } else if (state->quote == '"') {
        if (byte == '"') {
            state->quote = 0;
        } else {
            append(state, byte);
        }
    } else if (byte == '\'' || byte == '"') {
        state->quote = byte;
        state->present = true;
    } else if (byte == ' ' || byte == '\t') {
        emit(state);
    } else {
        append(state, byte);
    }
}

/** Decode a dependency byte emitted by Clang -MD -MT fern-object.
 * @param state Decoder. @param index Input cursor. @param size Input length.
 */
static void dependency_byte(Decoder *state, size_t *index, size_t size) {
    assert(state != NULL && index != NULL);
    assert(*index < size);
    unsigned char byte = input[*index];
    if (byte == '\\') {
        if (++*index == size) {
            reject(state, "unterminated escape");
        } else if (input[*index] == '\n' && *index + 1 == size) {
            reject(state, "unterminated continuation");
        } else if (input[*index] != '\n') {
            append(state, input[*index]);
        }
    } else if (byte == '$') {
        if (++*index == size || input[*index] != '$') {
            reject(state, "unexpected Make expansion");
        } else {
            append(state, '$');
        }
    } else if (byte == '#') {
        reject(state, "unexpected Make comment");
    } else if (byte == ' ' || byte == '\t') {
        emit(state);
    } else {
        append(state, byte);
    }
}

/** Read and validate bounded input before decoding, including embedded NUL
 * bytes.
 * @param state Decoder. @return Input length, or zero after failure.
 */
static size_t read_input(Decoder *state) {
    assert(state != NULL);
    assert(sizeof(input) == INPUT_LIMIT + 1);
    size_t size = fread(input, 1, sizeof(input), stdin);
    if (ferror(stdin) || size > INPUT_LIMIT) {
        reject(state, "input too large or unreadable");
        return 0;
    }
    for (size_t i = 0; i < size; i++) {
        if (input[i] == 0 || input[i] == '\r') {
            reject(state, "control byte");
            return 0;
        }
    }
    return size;
}

/** Decode exactly one flags record or compiler dependency rule.
 * @param state Decoder. @param size Validated input size.
 */
static void decode(Decoder *state, size_t size) {
    assert(state != NULL);
    assert(size <= INPUT_LIMIT);
    size_t start = 0;
    if (state->dependencies) {
        const char target[] = "fern-object:";
        if (size < sizeof(target) - 1 || memcmp(input, target, sizeof(target) - 1)) {
            reject(state, "unexpected target");
            return;
        }
        start = sizeof(target) - 1;
    }
    for (size_t i = start; i < size && !state->failed; i++) {
        if (input[i] == '\n') {
            if (i + 1 != size || state->quote != 0) {
                reject(state, "multiple records");
            }
        } else if (state->dependencies) {
            dependency_byte(state, &i, size);
        } else {
            flag_byte(state, &i, size);
        }
    }
    if (state->quote != 0) {
        reject(state, "unterminated quote");
    }
    emit(state);
    if (state->dependencies && state->count == 0) {
        reject(state, "empty dependency rule");
    }
}

/** Decode Clang -H include paths independently of Make filename normalization.
 * @param state Decoder. @param size Validated input size.
 */
static void decode_includes(Decoder *state, size_t size) {
    assert(state != NULL);
    assert(size <= INPUT_LIMIT);
    for (size_t index = 0; index < size && !state->failed;) {
        unsigned depth = 0;
        for (; index < size && input[index] == '.'; index++) {
            depth++;
        }
        if (depth == 0 || depth > 128 || index == size || input[index++] != ' ') {
            reject(state, "unexpected include trace");
            return;
        }
        for (; index < size && input[index] != '\n' && !state->failed; index++) {
            if (input[index] == '\\') {
                index++;
                if (index == size || (input[index] != '\\' && input[index] != '"')) {
                    reject(state, "unexpected include escape");
                    return;
                }
            }
            append(state, input[index]);
        }
        if (!state->present) {
            reject(state, "empty include path");
            return;
        }
        emit(state);
        if (index < size) {
            index++;
        }
    }
}

#include "style_tree.h"

/** Standalone OS argv/stdio boundary, with fixed buffers and owned POSIX
 * directory handles.
 * @param argc Count. @param argv Exactly flags, deps or includes mode. @return
 * Zero or bootstrap failure125.
 */
int main(int argc, char **argv) {
    assert(argc >= 0);
    assert(argv != NULL);
    Decoder state = {.limit = WORD_LIMIT};
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return 125;
    }
    if (argc != 2 || (strcmp(argv[1], "flags") && strcmp(argv[1], "deps") &&
                      strcmp(argv[1], "includes") && strcmp(argv[1], "tree"))) {
        reject(&state, "unknown mode");
        return 125;
    }
    state.dependencies = strcmp(argv[1], "deps") == 0;
    state.includes = strcmp(argv[1], "includes") == 0;
    if (state.dependencies || state.includes) {
        state.limit = 4096;
    }
    size_t size = read_input(&state);
    if (!state.failed) {
        if (strcmp(argv[1], "tree") == 0) {
            decode_tree(&state, size);
        } else if (state.includes) {
            decode_includes(&state, size);
        } else {
            decode(&state, size);
        }
    }
    if (fflush(stdout) != 0) {
        reject(&state, "output failed");
    }
    return state.failed ? 125 : 0;
}
