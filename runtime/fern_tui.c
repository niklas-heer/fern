/**
 * Terminal tree rendering, deterministic log lines, and cursor controls.
 */
#define _POSIX_C_SOURCE 200809L
#include "fern_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * Immutable, already rendered children avoid recursion on deeply nested trees.
 */
struct FernTree {
    const char *label;
    const char *branches;
    const char *last;
};

/**
 * Prefix every line of a child with the appropriate tree connector.
 * @param text Rendered child, possibly containing newlines.
 * @param last Whether this is the final child.
 * @return GC-managed child text with branch guides, without a final newline.
 */
static char* fern_tree_indent(const char *text, int last) {
    assert(text != NULL);
    assert(last == 0 || last == 1);
    size_t length = strlen(text);
    size_t lines = 1;
    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\n') lines++;
    }
    const char *first = last ? "└── " : "├── ";
    const char *next = last ? "    " : "│   ";
    size_t first_len = strlen(first), next_len = strlen(next);
    assert(lines <= (SIZE_MAX - length - first_len - 1) / next_len);
    char *result = fern_alloc(length + first_len + (lines - 1) * next_len + 1);
    size_t offset = first_len;
    memcpy(result, first, first_len);
    for (size_t i = 0; i < length; i++) {
        result[offset++] = text[i];
        if (text[i] == '\n') {
            memcpy(result + offset, next, next_len);
            offset += next_len;
        }
    }
    result[offset] = '\0';
    return result;
}

/**
 * Create an immutable root node.
 * @param label Root label; newlines are preserved.
 * @return GC-managed tree containing only the root.
 */
FernTree* fern_tree_new(const char *label) {
    assert(label != NULL);
    FernTree *tree = fern_alloc(sizeof(*tree));
    assert(tree != NULL);
    tree->label = fern_str_concat(label, "");
    tree->branches = "";
    tree->last = NULL;
    return tree;
}

/**
 * Render a tree without terminal escapes or a trailing newline.
 * @param tree Valid tree returned by new or add.
 * @return GC-managed plain text with Unicode branch guides.
 */
char* fern_tree_render(FernTree *tree) {
    assert(tree != NULL);
    assert(tree->label != NULL);
    if (!tree->last) return fern_str_concat(tree->label, "");
    char *body = fern_str_concat(tree->branches, fern_tree_indent(tree->last, 1));
    return fern_str_concat(fern_str_concat(tree->label, "\n"), body);
}

/**
 * Append a child, preserving both input trees for functional composition.
 * @param tree Parent tree to extend.
 * @param child Subtree appended after existing children.
 * @return A new tree; neither input is mutated.
 */
FernTree* fern_tree_add(FernTree *tree, FernTree *child) {
    assert(tree != NULL);
    assert(child != NULL);
    FernTree *result = fern_alloc(sizeof(*result));
    *result = *tree;
    if (tree->last) {
        char *previous = fern_str_concat(fern_tree_indent(tree->last, 0), "\n");
        result->branches = fern_str_concat(tree->branches, previous);
    }
    result->last = fern_tree_render(child);
    return result;
}

/**
 * Format one log record, escaping controls to prevent forged terminal records.
 * @param level Fixed severity name.
 * @param message User text, including UTF-8.
 * @return Plain, deterministic record without a trailing newline or timestamp.
 */
static char* fern_log_format(const char *level, const char *message) {
    assert(level != NULL);
    assert(message != NULL);
    size_t length = strlen(message), prefix = strlen(level) + 3;
    assert(length <= (SIZE_MAX - prefix - 1) / 4);
    char *result = fern_alloc(prefix + length * 4 + 1);
    size_t offset = (size_t)snprintf(result, prefix + 1, "[%s] ", level);
    const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < length; i++) {
        unsigned char byte = (unsigned char)message[i];
        if (byte == '\n' || byte == '\r' || byte == '\t' || byte == '\\') {
            result[offset++] = '\\';
            result[offset++] = byte == '\n' ? 'n' : byte == '\r' ? 'r' : byte == '\t' ? 't' : '\\';
        } else if (byte < 32 || byte == 127) {
            result[offset++] = '\\';
            result[offset++] = 'x';
            result[offset++] = hex[byte >> 4];
            result[offset++] = hex[byte & 15];
        } else {
            result[offset++] = (char)byte;
        }
    }
    result[offset] = '\0';
    return result;
}

/**
 * Format debug text.
 * @param message User message.
 * @return One plain log record.
 */
char* fern_log_debug(const char *message) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    return fern_log_format("DEBUG", message);
}
/**
 * Format informational text.
 * @param message User message.
 * @return One plain log record.
 */
char* fern_log_info(const char *message) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    return fern_log_format("INFO", message);
}
/**
 * Format warning text.
 * @param message User message.
 * @return One plain log record.
 */
char* fern_log_warn(const char *message) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    return fern_log_format("WARN", message);
}
/**
 * Format error text.
 * @param message User message.
 * @return One plain log record.
 */
char* fern_log_error(const char *message) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    return fern_log_format("ERROR", message);
}

/**
 * Emit a terminal escape only when stdout is interactive.
 * @param sequence Complete ANSI escape sequence.
 */
static void fern_term_write(const char *sequence) {
    assert(sequence != NULL);
    assert(sequence[0] == '\033');
    if (isatty(STDOUT_FILENO)) {
        fputs(sequence, stdout);
        fflush(stdout);
    }
}

/**
 * Move a relative number of terminal cells; nonpositive counts are no-ops.
 * @param count Requested number of cells.
 * @param direction ANSI direction code.
 */
static void fern_term_move(int64_t count, char direction) {
    assert(strchr("ABCD", direction) != NULL);
    assert(direction != '\0');
    if (count <= 0) return;
    char sequence[64];
    snprintf(sequence, sizeof(sequence), "\033[%lld%c", (long long)count, direction);
    fern_term_write(sequence);
}

/**
 * Move up.
 * @param count Cells; nonpositive counts do nothing.
 */
void fern_term_up(int64_t count) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_move(count, 'A');
}
/**
 * Move down.
 * @param count Cells; nonpositive counts do nothing.
 */
void fern_term_down(int64_t count) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_move(count, 'B');
}
/**
 * Move left.
 * @param count Cells; nonpositive counts do nothing.
 */
void fern_term_left(int64_t count) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_move(count, 'D');
}
/**
 * Move right.
 * @param count Cells; nonpositive counts do nothing.
 */
void fern_term_right(int64_t count) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_move(count, 'C');
}

/**
 * Position the cursor using one-based coordinates, clamped to at least one.
 * @param row Target row.
 * @param column Target column.
 */
void fern_term_move_to(int64_t row, int64_t column) {
    if (row < 1) row = 1;
    if (column < 1) column = 1;
    assert(row >= 1);
    assert(column >= 1);
    char sequence[64];
    snprintf(sequence, sizeof(sequence), "\033[%lld;%lldH", (long long)row, (long long)column);
    fern_term_write(sequence);
}

/**
 * Clear the screen and position the cursor at the top left.
 */
void fern_term_clear(void) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_write("\033[2J\033[H");
}
/**
 * Hide the terminal cursor until show_cursor is called.
 */
void fern_term_hide_cursor(void) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_write("\033[?25l");
}
/**
 * Show the terminal cursor.
 */
void fern_term_show_cursor(void) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_write("\033[?25h");
}
/**
 * Save the current terminal cursor position.
 */
void fern_term_save_cursor(void) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_write("\033[s");
}
/**
 * Restore the saved terminal cursor position.
 */
void fern_term_restore_cursor(void) {
    /* FERN_STYLE: allow(assertion-density) - shared helper validates inputs. */
    fern_term_write("\033[u");
}
