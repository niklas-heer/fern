/**
 * Fern Compiler - Error Reporting Implementation
 */

// Enable POSIX functions like fileno() in strict C11 mode
#define _POSIX_C_SOURCE 200809L

#include "errors.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ========== Color Detection ========== */
static ErrorsColorMode g_color_mode = ERRORS_COLOR_AUTO;

/**
 * Set color output mode override.
 * @param mode Requested color mode.
 */
void errors_set_color_mode(ErrorsColorMode mode) {
    assert(mode == ERRORS_COLOR_AUTO || mode == ERRORS_COLOR_ALWAYS || mode == ERRORS_COLOR_NEVER);
    assert(g_color_mode == ERRORS_COLOR_AUTO || g_color_mode == ERRORS_COLOR_ALWAYS || g_color_mode == ERRORS_COLOR_NEVER);
    g_color_mode = mode;
}

/**
 * Get current configured color mode.
 * @return The current color mode.
 */
ErrorsColorMode errors_get_color_mode(void) {
    assert(g_color_mode == ERRORS_COLOR_AUTO || g_color_mode == ERRORS_COLOR_ALWAYS || g_color_mode == ERRORS_COLOR_NEVER);
    assert(g_color_mode >= ERRORS_COLOR_AUTO && g_color_mode <= ERRORS_COLOR_NEVER);
    return g_color_mode;
}

/**
 * Check if stderr supports colors.
 * @return true if colors should be used.
 */
bool errors_use_color(void) {
    // FERN_STYLE: allow(assertion-density) environment checks
    if (g_color_mode == ERRORS_COLOR_ALWAYS) {
        return true;
    }
    if (g_color_mode == ERRORS_COLOR_NEVER) {
        return false;
    }
    
    // Check NO_COLOR (https://no-color.org/)
    if (getenv("NO_COLOR") != NULL) {
        return false;
    }
    
    // Check FORCE_COLOR
    if (getenv("FORCE_COLOR") != NULL) {
        return true;
    }
    
    // Check if stderr is a terminal
    return isatty(fileno(stderr));
}

/* ========== Internal Helpers ========== */

/**
 * Get color code or empty string based on color support.
 * @param code The ANSI color code to return if colors enabled.
 * @return The color code or empty string.
 */
static const char* color(const char* code) {
    // FERN_STYLE: allow(assertion-density) simple helper
    return errors_use_color() ? code : "";
}

/* ========== Error Reporting ========== */

/**
 * Print an error message with optional color.
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void error_print(const char* fmt, ...) {
    // FERN_STYLE: allow(assertion-density) variadic print function
    fprintf(stderr, "%serror:%s ", color(ANSI_BOLD_RED), color(ANSI_RESET));
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/**
 * Print a warning message with optional color.
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void warning_print(const char* fmt, ...) {
    // FERN_STYLE: allow(assertion-density) variadic print function
    fprintf(stderr, "%swarning:%s ", color(ANSI_BOLD_YELLOW), color(ANSI_RESET));
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/**
 * Print a note/hint message with optional color.
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void note_print(const char* fmt, ...) {
    // FERN_STYLE: allow(assertion-density) variadic print function
    fprintf(stderr, "%snote:%s ", color(ANSI_BOLD_CYAN), color(ANSI_RESET));
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

/**
 * Print a help/fix hint message with optional color.
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void help_print(const char* fmt, ...) {
    // FERN_STYLE: allow(assertion-density) variadic print function
    fprintf(stderr, "%shelp:%s ", color(ANSI_BOLD_GREEN), color(ANSI_RESET));

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

/**
 * Print a source location header.
 * @param filename The source file name.
 * @param line The line number (1-based).
 * @param col The column number (1-based), or 0 to omit.
 */
void error_location(const char* filename, int line, int col) {
    // FERN_STYLE: allow(assertion-density) simple print function
    fprintf(stderr, "%s%s:%d", color(ANSI_BOLD), filename, line);
    if (col > 0) {
        fprintf(stderr, ":%d", col);
    }
    fprintf(stderr, ":%s ", color(ANSI_RESET));
}

/**
 * Print a source line with error indicator.
 * @param source_line The source code line (without newline).
 * @param col The column to highlight (1-based).
 * @param len The length of the highlight (default 1 if 0).
 */
void error_source_line(const char* source_line, int col, int len) {
    // FERN_STYLE: allow(assertion-density) formatting function
    if (!source_line) return;
    if (len <= 0) len = 1;
    size_t line_len = strcspn(source_line, "\n");
    
    // Print the source line
    fprintf(stderr, "  %s%.*s%s\n", color(ANSI_DIM), (int)line_len, source_line, color(ANSI_RESET));
    
    // Print the indicator line
    fprintf(stderr, "  ");
    for (int i = 1; i < col && i <= (int)line_len; i++) {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "%s^", color(ANSI_BOLD_RED));
    for (int i = 1; i < len; i++) {
        fprintf(stderr, "~");
    }
    fprintf(stderr, "%s\n", color(ANSI_RESET));
}

/**
 * Print a complete error with location and source context.
 * @param filename The source file name.
 * @param line The line number (1-based).
 * @param col The column number (1-based).
 * @param source_line The source code line.
 * @param fmt Printf-style format string for error message.
 * @param ... Format arguments.
 */
void error_at(const char* filename, int line, int col,
              const char* source_line, const char* fmt, ...) {
    // FERN_STYLE: allow(assertion-density) compound error function
    
    // Print location
    error_location(filename, line, col);
    
    // Print "error: " prefix
    fprintf(stderr, "%serror:%s ", color(ANSI_BOLD_RED), color(ANSI_RESET));
    
    // Print message
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    
    // Print source context
    if (source_line) {
        error_source_line(source_line, col, 1);
    }
}
