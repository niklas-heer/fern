/**
 * Fern Compiler - Error Reporting
 * 
 * Provides colored, formatted error messages with source context.
 */

#ifndef FERN_ERRORS_H
#define FERN_ERRORS_H

#include <stdio.h>
#include <stdbool.h>

/* ========== ANSI Color Codes ========== */

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"

#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_WHITE   "\033[37m"

#define ANSI_BOLD_RED     "\033[1;31m"
#define ANSI_BOLD_GREEN   "\033[1;32m"
#define ANSI_BOLD_YELLOW  "\033[1;33m"
#define ANSI_BOLD_BLUE    "\033[1;34m"
#define ANSI_BOLD_MAGENTA "\033[1;35m"
#define ANSI_BOLD_CYAN    "\033[1;36m"

/* ========== Color Detection ========== */

/**
 * Check if stderr supports colors.
 * Respects NO_COLOR and FORCE_COLOR environment variables.
 * @return true if colors should be used.
 */
bool errors_use_color(void);

/* ========== Error Reporting ========== */

/**
 * Print an error message with optional color.
 * Format: "error: <message>"
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void error_print(const char* fmt, ...);

/**
 * Print a warning message with optional color.
 * Format: "warning: <message>"
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void warning_print(const char* fmt, ...);

/**
 * Print a note/hint message with optional color.
 * Format: "note: <message>"
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
void note_print(const char* fmt, ...);

/**
 * Print a source location header.
 * Format: "<file>:<line>:<col>: "
 * @param filename The source file name.
 * @param line The line number (1-based).
 * @param col The column number (1-based), or 0 to omit.
 */
void error_location(const char* filename, int line, int col);

/**
 * Print a source line with error indicator.
 * @param source_line The source code line (without newline).
 * @param col The column to highlight (1-based).
 * @param len The length of the highlight (default 1).
 */
void error_source_line(const char* source_line, int col, int len);

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
              const char* source_line, const char* fmt, ...);

#endif /* FERN_ERRORS_H */
