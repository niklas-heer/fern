/* Fern Compiler - Main Entry Point */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "arena.h"
#include "qbe.h"  /* Embedded QBE compiler backend */
#include "fern_string.h"
#include "lexer.h"
#include "parser.h"
#include "checker.h"
#include "codegen.h"
#include "ast_print.h"
#include "ast_validate.h"
#include "cli_parse.h"
#include "version.h"
#include "errors.h"
#include "lsp.h"
#include "repl.h"

/* ========== File Utilities ========== */

/**
 * Read entire file contents into a string.
 * @param arena The arena to allocate from.
 * @param filename The file path to read.
 * @return The file contents, or NULL on error.
 */
static char* read_file(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) file I/O with multiple error paths
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    
    // Allocate buffer
    char* buffer = arena_alloc(arena, (size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    // Read file
    size_t read_size = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    
    if ((long)read_size != size) {
        return NULL;
    }
    
    buffer[size] = '\0';
    return buffer;
}

/**
 * Get the base name of a file path (without extension).
 * @param arena The arena to allocate from.
 * @param filename The file path.
 * @return The base name without extension.
 */
static String* get_basename(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) simple string manipulation
    // Find last slash
    const char* base = filename;
    for (const char* p = filename; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    
    // Find extension
    const char* dot = NULL;
    for (const char* p = base; *p; p++) {
        if (*p == '.') {
            dot = p;
        }
    }
    
    // Create base name
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    return string_new_len(arena, base, len);
}

/* ========== CLI Definition ========== */

/** Command handler function type. */
typedef int (*CmdHandler)(Arena* arena, const char* filename);

/** CLI command definition. */
typedef struct {
    const char* name;        /* Command name (e.g., "build") */
    const char* args;        /* Argument placeholder (e.g., "<file>") */
    const char* description; /* Short description */
    CmdHandler handler;      /* Function to execute */
} Command;

/** CLI option definition. */
typedef struct {
    const char* short_flag;  /* Short flag (e.g., "-h") */
    const char* long_flag;   /* Long flag (e.g., "--help") */
    const char* description; /* Short description */
} Option;

/* Forward declarations for command handlers */
static int cmd_build(Arena* arena, const char* filename);
static int cmd_run(Arena* arena, const char* filename);
static int cmd_check(Arena* arena, const char* filename);
static int cmd_emit(Arena* arena, const char* filename);
static int cmd_lex(Arena* arena, const char* filename);
static int cmd_parse(Arena* arena, const char* filename);
static int cmd_fmt(Arena* arena, const char* filename);
static int cmd_test(Arena* arena, const char* filename);
static int cmd_doc(Arena* arena, const char* filename);
static int cmd_lsp(Arena* arena, const char* filename);
static int cmd_repl(Arena* arena, const char* filename);
static void report_parse_failure(const char* filename);
static void report_type_failure(const char* filename, const char* source, const char* err);
static void log_info(const char* fmt, ...);
static void log_verbose(const char* fmt, ...);

/** All available commands. */
static const Command COMMANDS[] = {
    {"build", "<file>", "Compile to executable",       cmd_build},
    {"run",   "<file>", "Compile and run immediately", cmd_run},
    {"check", "<file>", "Type check only",             cmd_check},
    {"emit",  "<file>", "Emit QBE IR to stdout",       cmd_emit},
    {"lex",   "<file>", "Show tokens (debug)",         cmd_lex},
    {"parse", "<file>", "Show AST (debug)",            cmd_parse},
    {"fmt",   "<file>", "Format source deterministically", cmd_fmt},
    {"test",  "",       "Run project tests",           cmd_test},
    {"doc",   "",       "Generate documentation",      cmd_doc},
    {"lsp",   "",       "Start language server",       cmd_lsp},
    {"repl",  "",       "Interactive mode",            cmd_repl},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

/** All available options. */
static const Option OPTIONS[] = {
    {"-h", "--help",    "Show this help message"},
    {"-v", "--version", "Show version information"},
    {"-o", "--output",  "Output file (build only)"},
    {"",   "--open",    "Open generated docs (doc only)"},
    {"",   "--html",    "Emit HTML docs (doc only)"},
    {"",   "--color",   "Color output: --color=auto|always|never"},
    {"",   "--quiet",   "Suppress non-error output"},
    {"",   "--verbose", "Enable verbose diagnostic output"},
    {NULL, NULL, NULL}  /* Sentinel */
};

/* ========== CLI Help Generation ========== */

/**
 * Print usage information (auto-generated from COMMANDS and OPTIONS).
 */
static void print_usage(void) {
    // FERN_STYLE: allow(assertion-density) simple print function
    fprintf(stderr, "%s\n\n", FERN_VERSION);
    fprintf(stderr, "Usage: fern <command> [options] <file>\n\n");
    
    /* Print commands */
    fprintf(stderr, "Commands:\n");
    for (const Command* cmd = COMMANDS; cmd->name != NULL; cmd++) {
        fprintf(stderr, "  %-14s %s\n", cmd->name, cmd->description);
    }
    
    /* Print options */
    fprintf(stderr, "\nOptions:\n");
    for (const Option* opt = OPTIONS; opt->short_flag != NULL; opt++) {
        char flags[32];
        if (opt->short_flag[0] != '\0') {
            snprintf(flags, sizeof(flags), "%s, %s", opt->short_flag, opt->long_flag);
        } else {
            snprintf(flags, sizeof(flags), "%s", opt->long_flag);
        }
        fprintf(stderr, "  %-14s %s\n", flags, opt->description);
    }
    
    fprintf(stderr, "\nFile extensions: .fn, .🌿\n");
}

/**
 * Print version information.
 */
static void print_version(void) {
    // FERN_STYLE: allow(assertion-density) simple print function
    printf("%s\n", FERN_VERSION);
}

/**
 * Find a command by name.
 * @param name The command name to look up.
 * @return The command, or NULL if not found.
 */
static const Command* find_command(const char* name) {
    // FERN_STYLE: allow(assertion-density) simple lookup
    for (const Command* cmd = COMMANDS; cmd->name != NULL; cmd++) {
        if (strcmp(cmd->name, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

/**
 * Parse a positive integer segment from a substring.
 * @param start Segment start (inclusive).
 * @param end Segment end (exclusive).
 * @param value_out Parsed integer output.
 * @return True if parsing succeeded.
 */
static bool parse_positive_int_segment(const char* start, const char* end, int* value_out) {
    // FERN_STYLE: allow(assertion-density) bounded numeric parsing helper
    assert(start != NULL);
    assert(end != NULL);
    assert(value_out != NULL);
    if (!start || !end || !value_out || start >= end) {
        return false;
    }

    size_t len = (size_t)(end - start);
    if (len >= 32) {
        return false;
    }

    char buf[32];
    memcpy(buf, start, len);
    buf[len] = '\0';

    char* parse_end = NULL;
    long value = strtol(buf, &parse_end, 10);
    if (!parse_end || *parse_end != '\0') {
        return false;
    }
    if (value <= 0 || value > INT_MAX) {
        return false;
    }

    *value_out = (int)value;
    return true;
}

/**
 * Parse checker error text and extract location/message when available.
 * Supports "file:line:col: message" and "line:col: message".
 * @param err Raw checker error message.
 * @param line_out Parsed line number.
 * @param col_out Parsed column number.
 * @param message_out Parsed message (or original err).
 * @return True if a valid location was extracted.
 */
static bool parse_checker_error_location(const char* err, int* line_out, int* col_out, const char** message_out) {
    // FERN_STYLE: allow(assertion-density) structured parse with guarded fallbacks
    assert(err != NULL);
    assert(line_out != NULL);
    assert(col_out != NULL);
    assert(message_out != NULL);
    if (!err || !line_out || !col_out || !message_out) {
        return false;
    }

    *line_out = 0;
    *col_out = 0;
    *message_out = err;

    const char* c1 = strchr(err, ':');
    if (!c1) {
        return false;
    }
    const char* c2 = strchr(c1 + 1, ':');
    if (!c2) {
        return false;
    }
    const char* c3 = strchr(c2 + 1, ':');

    int line = 0;
    int col = 0;
    if (c3 &&
        parse_positive_int_segment(c1 + 1, c2, &line) &&
        parse_positive_int_segment(c2 + 1, c3, &col)) {
        *line_out = line;
        *col_out = col;
        *message_out = c3 + 1;
        while (**message_out == ' ') {
            (*message_out)++;
        }
        return true;
    }

    if (parse_positive_int_segment(err, c1, &line) &&
        parse_positive_int_segment(c1 + 1, c2, &col)) {
        *line_out = line;
        *col_out = col;
        *message_out = c2 + 1;
        while (**message_out == ' ') {
            (*message_out)++;
        }
        return true;
    }

    return false;
}

/**
 * Get the start pointer for a specific 1-indexed source line.
 * @param source Full source text.
 * @param target_line The target line number (1-indexed).
 * @return Pointer to line start, or NULL if line does not exist.
 */
static const char* source_line_start(const char* source, int target_line) {
    // FERN_STYLE: allow(assertion-density) simple bounded scan over input text
    // FERN_STYLE: allow(no-raw-char) source is immutable UTF-8 source input
    assert(source != NULL);
    assert(target_line > 0);
    if (!source || target_line <= 0) {
        return NULL;
    }

    int line = 1;
    const char* p = source;
    while (*p && line < target_line) {
        if (*p == '\n') {
            line++;
        }
        p++;
    }

    if (line != target_line) {
        return NULL;
    }
    return p;
}

/**
 * Print parse failure guidance after parser diagnostics are emitted.
 * @param filename Source filename used in CLI context.
 */
static void report_parse_failure(const char* filename) {
    // FERN_STYLE: allow(assertion-density) concise diagnostic guidance
    assert(filename != NULL);
    assert(filename[0] != '\0');
    if (!filename || filename[0] == '\0') {
        return;
    }

    note_print("while parsing %s", filename);
    help_print("fix the highlighted syntax and rerun: fern check %s", filename);
}

/**
 * Print type-check failure with snippet, note, and fix hint.
 * @param filename Source filename used in CLI context.
 * @param source Full source text.
 * @param err Raw checker error message.
 */
static void report_type_failure(const char* filename, const char* source, const char* err) {
    // FERN_STYLE: allow(assertion-density) centralized type diagnostics formatting
    assert(filename != NULL);
    assert(source != NULL);
    assert(filename[0] != '\0');
    int line = 0;
    int col = 0;
    const char* message = err ? err : "type error";

    if (err && parse_checker_error_location(err, &line, &col, &message)) {
        if (line > 0 && col > 0) {
            error_location(filename, line, col);
            error_print("%s", message);
            const char* line_text = source_line_start(source, line);
            if (line_text) {
                error_source_line(line_text, col, 1);
            }
        } else {
            error_print("%s", message);
        }
    } else {
        error_print("%s", message);
    }

    note_print("type checking failed for %s", filename);
    if (err && strstr(err, "Unhandled Result value") != NULL) {
        help_print("handle the Result with match, with, or ? before continuing");
        return;
    }
    if (err && strstr(err, "declared return type") != NULL) {
        help_print("return a value that matches the function return type, or update the annotation");
        return;
    }
    if (err && strstr(err, "Type mismatch") != NULL) {
        help_print("align the expression type with the expected type annotation");
        return;
    }
    help_print("adjust the highlighted expression or nearby type annotations");
}

/**
 * Compile a Fern source file to QBE IR.
 * @param arena The arena for allocations.
 * @param source The source code.
 * @param filename The source filename (for error messages).
 * @return The codegen instance, or NULL on error.
 */
static Codegen* compile_to_qbe(Arena* arena, const char* source, const char* filename) {
    // FERN_STYLE: allow(assertion-density) pipeline orchestration
    log_verbose("verbose: parsing %s\n", filename);
    
    // Parse
    Parser* parser = parser_new(arena, source);
    StmtVec* stmts = parse_stmts(parser);
    
    if (parser_had_error(parser)) {
        report_parse_failure(filename);
        return NULL;
    }
    
    if (!stmts || stmts->len == 0) {
        error_location(filename, 1, 0);
        error_print("no statements found");
        return NULL;
    }
    
    // Type check
    log_verbose("verbose: type checking %s\n", filename);
    Checker* checker = checker_new(arena);
    bool check_ok = checker_check_stmts(checker, stmts);
    
    if (!check_ok || checker_has_errors(checker)) {
        const char* err = checker_first_error(checker);
        report_type_failure(filename, source, err);
        return NULL;
    }
    
    // Generate QBE IR
    log_verbose("verbose: generating QBE IR for %s\n", filename);
    Codegen* cg = codegen_new(arena);
    codegen_program(cg, stmts);
    
    return cg;
}

/**
 * Find the runtime library path relative to fern executable.
 * @param exe_path Path to the fern executable (argv[0]).
 * @param buf Buffer to write the runtime library path (output parameter).
 * @param buf_size Size of the buffer.
 * @return true if found, false otherwise.
 */
static bool find_runtime_lib(const char* exe_path, char* buf, size_t buf_size) {  // FERN_STYLE: allow(no-raw-char) buf is output parameter
    // FERN_STYLE: allow(assertion-density) path manipulation with multiple checks
    
    // Try to find runtime library in same directory as fern executable
    // Find the directory containing the executable
    const char* last_slash = NULL;
    for (const char* p = exe_path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_slash = p;
        }
    }
    
    if (last_slash) {
        size_t dir_len = (size_t)(last_slash - exe_path);
        if (dir_len + 20 < buf_size) {
            memcpy(buf, exe_path, dir_len);
            strcpy(buf + dir_len, "/libfern_runtime.a");
            
            // Check if file exists
            struct stat st;
            if (stat(buf, &st) == 0) {
                return true;
            }
        }
    }
    
    // Try current directory
    snprintf(buf, buf_size, "./bin/libfern_runtime.a");
    struct stat st;
    if (stat(buf, &st) == 0) {
        return true;
    }
    
    return false;
}

/* Global variable to store exe path from main */
static const char* g_exe_path = NULL;

/* Global variable for output filename (-o flag) */
static const char* g_output_file = NULL;
static bool g_test_doc_mode = false;
static bool g_doc_open_mode = false;
static bool g_doc_html_mode = false;

typedef enum {
    LOG_NORMAL = 0,
    LOG_QUIET = 1,
    LOG_VERBOSE = 2
} LogLevel;

static LogLevel g_log_level = LOG_NORMAL;

/**
 * Print a normal informational message unless quiet mode is active.
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
static void log_info(const char* fmt, ...) {
    assert(fmt != NULL);
    assert(g_log_level >= LOG_NORMAL && g_log_level <= LOG_VERBOSE);
    if (g_log_level == LOG_QUIET) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

/**
 * Print verbose diagnostics only when verbose mode is active.
 * @param fmt Printf-style format string.
 * @param ... Format arguments.
 */
static void log_verbose(const char* fmt, ...) {
    assert(fmt != NULL);
    assert(g_log_level >= LOG_NORMAL && g_log_level <= LOG_VERBOSE);
    if (g_log_level != LOG_VERBOSE) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

/**
 * Parse a global --color=<mode> flag.
 * @param arg CLI argument token.
 * @param mode_out Output color mode when parsing succeeds.
 * @return true when arg is a recognized color flag, false otherwise.
 */
static bool parse_color_flag(const char* arg, ErrorsColorMode* mode_out) {
    assert(arg != NULL);
    assert(mode_out != NULL);
    if (!arg || !mode_out) {
        return false;
    }

    if (strcmp(arg, "--color=auto") == 0) {
        *mode_out = ERRORS_COLOR_AUTO;
        return true;
    }
    if (strcmp(arg, "--color=always") == 0) {
        *mode_out = ERRORS_COLOR_ALWAYS;
        return true;
    }
    if (strcmp(arg, "--color=never") == 0) {
        *mode_out = ERRORS_COLOR_NEVER;
        return true;
    }
    return false;
}

/**
 * Run QBE compiler and linker to create executable.
 * Uses embedded QBE backend - no external qbe binary needed.
 * @param ssa_file Path to QBE IR file.
 * @param output_file Path for output executable.
 * @return 0 on success, non-zero on error.
 */
static int run_qbe_and_link(const char* ssa_file, const char* output_file) {
    // FERN_STYLE: allow(assertion-density) compilation pipeline
    // FERN_STYLE: allow(function-length) QBE/link pipeline is cohesive
    char cmd[1024];
    char asm_file[256];
    char obj_file[256];
    int ret;

    snprintf(asm_file, sizeof(asm_file), "%s.s", output_file);
    snprintf(obj_file, sizeof(obj_file), "%s.o", output_file);

    // Open SSA input file
    FILE* ssa_input = fopen(ssa_file, "r");
    if (!ssa_input) {
        error_print("cannot open SSA file '%s'", ssa_file);
        return 1;
    }

    // Open assembly output file
    FILE* asm_output = fopen(asm_file, "w");
    if (!asm_output) {
        fclose(ssa_input);
        error_print("cannot create assembly file '%s'", asm_file);
        return 1;
    }

    // Compile QBE IR to assembly using embedded QBE
    log_verbose("verbose: qbe compile %s -> %s\n", ssa_file, asm_file);
    ret = qbe_compile(ssa_input, asm_output, ssa_file);
    fclose(ssa_input);
    fclose(asm_output);

    if (ret != 0) {
        error_print("QBE compilation failed");
        unlink(asm_file);
        return 1;
    }

    // Assemble using system compiler
    log_verbose("verbose: assembling %s -> %s\n", asm_file, obj_file);
    snprintf(cmd, sizeof(cmd), "cc -c -o %s %s 2>&1", obj_file, asm_file);
    ret = system(cmd);
    if (ret != 0) {
        error_print("assembly failed");
        unlink(asm_file);
        return 1;
    }

    // Find and link with runtime library + GC + sqlite3 runtime dependencies.
    char runtime_lib[512];
    const char* gc_link = "$(pkg-config --variable=libdir bdw-gc 2>/dev/null | xargs -I{} echo {}/libgc.a || "
                          "for d in /opt/homebrew/lib /usr/local/lib /usr/lib /usr/lib/x86_64-linux-gnu; do "
                          "[ -f $d/libgc.a ] && echo $d/libgc.a && break; done)";
    const char* sqlite_link = "$(pkg-config --libs sqlite3 2>/dev/null || echo -lsqlite3)";
    const char* thread_link = "-pthread";
    if (g_exe_path && find_runtime_lib(g_exe_path, runtime_lib, sizeof(runtime_lib))) {
        log_verbose("verbose: linking with runtime %s\n", runtime_lib);
        snprintf(cmd, sizeof(cmd), "cc -o %s %s %s %s %s %s 2>&1",
            output_file, obj_file, runtime_lib, gc_link, sqlite_link, thread_link);
    } else {
        // Fall back to linking without runtime (will fail if runtime functions used)
        log_verbose("verbose: runtime library not found near executable, linking fallback path\n");
        snprintf(cmd, sizeof(cmd), "cc -o %s %s %s %s %s 2>&1",
            output_file, obj_file, gc_link, sqlite_link, thread_link);
    }

    ret = system(cmd);
    if (ret != 0) {
        error_print("linking failed");
        unlink(asm_file);
        unlink(obj_file);
        return 1;
    }

    // Clean up intermediate files
    unlink(asm_file);
    unlink(obj_file);

    return 0;
}

/**
 * Run a shell command and return its process exit code.
 * @param command Shell command to execute.
 * @return Exit code from child process, or 1 on execution failure.
 */
static int run_shell_command(const char* command) {
    assert(command != NULL);
    assert(command[0] != '\0');
    if (!command || command[0] == '\0') {
        return 1;
    }

    int status = system(command);
    if (status < 0) {
        error_print("failed to execute command: %s", command);
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

/**
 * Shell-quote a single argument using POSIX-safe single-quote escaping.
 * @param input Raw argument text.
 * @param output Destination buffer for quoted output.
 * @param output_size Capacity of output buffer.
 * @return True on success, false if output buffer is too small.
 */
static bool shell_quote_arg(const char* input, char* output, size_t output_size) {
    // FERN_STYLE: allow(assertion-density, no-raw-char) output buffer is an explicit out-parameter
    assert(input != NULL);
    assert(output != NULL);
    assert(output_size > 2);
    if (!input || !output || output_size <= 2) {
        return false;
    }

    size_t out = 0;
    output[out++] = '\'';

    for (const char* p = input; *p != '\0'; p++) {
        if (*p == '\'') {
            if (out + 4 >= output_size) {
                return false;
            }
            output[out++] = '\'';
            output[out++] = '\\';
            output[out++] = '\'';
            output[out++] = '\'';
        } else {
            if (out + 1 >= output_size) {
                return false;
            }
            output[out++] = *p;
        }
    }

    if (out + 2 > output_size) {
        return false;
    }
    output[out++] = '\'';
    output[out] = '\0';
    return true;
}

/* ========== Commands ========== */

/**
 * Build command: compile source to executable.
 * @param arena The arena for allocations.
 * @param filename The source file path.
 * @return Exit code.
 */
static int cmd_build(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command handler
    
    // Read source file
    char* source = read_file(arena, filename);
    if (!source) {
        error_print("cannot read file '%s'", filename);
        return 1;
    }
    
    log_info("Compiling %s...\n", filename);
    
    // Compile to QBE IR
    Codegen* cg = compile_to_qbe(arena, source, filename);
    if (!cg) {
        return 1;
    }
    
    // Determine output filename
    char output_file[256];
    if (g_output_file) {
        snprintf(output_file, sizeof(output_file), "%s", g_output_file);
    } else {
        String* basename = get_basename(arena, filename);
        snprintf(output_file, sizeof(output_file), "%s", string_cstr(basename));
    }
    
    // Write QBE IR to temp file
    char ssa_file[256];
    snprintf(ssa_file, sizeof(ssa_file), "%s.ssa", output_file);
    
    if (!codegen_write(cg, ssa_file)) {
        error_print("cannot write QBE IR to '%s'", ssa_file);
        return 1;
    }
    
    // Run QBE and link
    int ret = run_qbe_and_link(ssa_file, output_file);
    
    // Clean up SSA file on success
    if (ret == 0) {
        unlink(ssa_file);
        log_info("Created executable: %s\n", output_file);
    }
    
    return ret;
}

/**
 * Check command: type check only (no code generation).
 * @param arena The arena for allocations.
 * @param filename The source file path.
 * @return Exit code.
 */
static int cmd_check(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command handler
    
    // Read source file
    char* source = read_file(arena, filename);
    if (!source) {
        error_print("cannot read file '%s'", filename);
        return 1;
    }
    
    // Parse
    Parser* parser = parser_new(arena, source);
    StmtVec* stmts = parse_stmts(parser);
    
    if (parser_had_error(parser)) {
        report_parse_failure(filename);
        return 1;
    }
    
    // Type check
    Checker* checker = checker_new(arena);
    bool check_ok = checker_check_stmts(checker, stmts);
    
    if (!check_ok || checker_has_errors(checker)) {
        const char* err = checker_first_error(checker);
        report_type_failure(filename, source, err);
        return 1;
    }
    
    log_info("✓ %s: No type errors\n", filename);
    return 0;
}

/**
 * Emit command: output QBE IR to stdout.
 * @param arena The arena for allocations.
 * @param filename The source file path.
 * @return Exit code.
 */
static int cmd_emit(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command handler
    
    // Read source file
    char* source = read_file(arena, filename);
    if (!source) {
        error_print("cannot read file '%s'", filename);
        return 1;
    }
    
    // Compile to QBE IR
    Codegen* cg = compile_to_qbe(arena, source, filename);
    if (!cg) {
        return 1;
    }
    
    // Output to stdout
    codegen_emit(cg, stdout);
    return 0;
}

/**
 * Run command: compile and execute immediately.
 * @param arena The arena for allocations.
 * @param filename The source file path.
 * @return Exit code from the compiled program.
 */
static int cmd_run(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command handler
    
    // Read source file
    char* source = read_file(arena, filename);
    if (!source) {
        error_print("cannot read file '%s'", filename);
        return 1;
    }
    
    // Compile to QBE IR
    Codegen* cg = compile_to_qbe(arena, source, filename);
    if (!cg) {
        return 1;
    }
    
    // Write QBE IR to temp file
    String* basename = get_basename(arena, filename);
    char ssa_file[256];
    snprintf(ssa_file, sizeof(ssa_file), "/tmp/fern_%s.ssa", string_cstr(basename));
    
    if (!codegen_write(cg, ssa_file)) {
        error_print("cannot write QBE IR to '%s'", ssa_file);
        return 1;
    }
    
    // Run QBE and link to temp executable
    char output_file[256];
    snprintf(output_file, sizeof(output_file), "/tmp/fern_%s", string_cstr(basename));
    
    int ret = run_qbe_and_link(ssa_file, output_file);
    
    // Clean up SSA file
    unlink(ssa_file);
    
    if (ret != 0) {
        return ret;
    }
    
    // Execute the compiled program
    ret = system(output_file);
    
    // Clean up executable
    unlink(output_file);
    
    // Extract actual exit code from system() return value
    if (WIFEXITED(ret)) {
        return WEXITSTATUS(ret);
    }
    return ret;
}

/**
 * Lex command: show tokens (debug).
 * @param arena The arena for allocations.
 * @param filename The source file path.
 * @return Exit code.
 */
static int cmd_lex(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command handler
    
    // Read source file
    char* source = read_file(arena, filename);
    if (!source) {
        error_print("cannot read file '%s'", filename);
        return 1;
    }
    
    // Tokenize and print
    Lexer* lexer = lexer_new(arena, source);
    Token tok;
    int count = 0;
    
    printf("Tokens for %s:\n", filename);
    printf("%-6s %-15s %s\n", "LINE", "TYPE", "VALUE");
    printf("------ --------------- ----------------\n");
    
    do {
        tok = lexer_next(lexer);
        printf("%-6zu %-15s %s\n", 
               tok.loc.line, 
               token_type_name(tok.type),
               tok.text ? string_cstr(tok.text) : "");
        count++;
    } while (tok.type != TOKEN_EOF);
    
    printf("\nTotal: %d tokens\n", count);
    return 0;
}

/**
 * Parse command: show AST (debug).
 * @param arena The arena for allocations.
 * @param filename The source file path.
 * @return Exit code.
 */
static int cmd_parse(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command handler

    // Read source file
    char* source = read_file(arena, filename);
    if (!source) {
        error_print("cannot read file '%s'", filename);
        return 1;
    }

    return fern_parse_source(arena, filename, source, stdout);
}

/**
 * Write text content to a file path.
 * @param filename Output file path.
 * @param text UTF-8 text to write.
 * @return True on success, false on I/O failure.
 */
static bool write_text_file(const char* filename, const char* text) {
    // FERN_STYLE: allow(assertion-density) thin I/O helper with explicit checks
    assert(filename != NULL);
    assert(text != NULL);
    if (!filename || !text) {
        return false;
    }

    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }

    size_t len = strlen(text);
    size_t wrote = fwrite(text, 1, len, file);
    if (fclose(file) != 0) {
        return false;
    }
    return wrote == len;
}

/**
 * Normalize Fern source deterministically.
 * Rules: convert CRLF/CR to LF, trim trailing spaces/tabs per line,
 * and keep exactly one trailing newline.
 * @param arena Arena for output allocation.
 * @param source Input source text.
 * @return Normalized text allocated in arena.
 */
static const char* normalize_source(Arena* arena, const char* source) {
    // FERN_STYLE: allow(function-length) line-oriented normalization with edge-case handling
    assert(arena != NULL);
    assert(source != NULL);
    if (!arena || !source) {
        return "";
    }

    size_t src_len = strlen(source);
    char* out = arena_alloc(arena, src_len + 2);
    size_t out_len = 0;
    const char* p = source;

    while (*p != '\0') {
        const char* line_start = p;
        while (*p != '\0' && *p != '\n' && *p != '\r') {
            p++;
        }

        const char* line_end = p;
        while (line_end > line_start && (line_end[-1] == ' ' || line_end[-1] == '\t')) {
            line_end--;
        }

        size_t trimmed_len = (size_t)(line_end - line_start);
        if (trimmed_len > 0) {
            memcpy(out + out_len, line_start, trimmed_len);
            out_len += trimmed_len;
        }

        if (*p == '\r') {
            p++;
            if (*p == '\n') {
                p++;
            }
            out[out_len++] = '\n';
            continue;
        }
        if (*p == '\n') {
            p++;
            out[out_len++] = '\n';
        }
    }

    while (out_len > 0 && out[out_len - 1] == '\n') {
        out_len--;
    }

    out[out_len++] = '\n';
    out[out_len] = '\0';
    return out;
}

/**
 * Fmt command: normalize source formatting deterministically in-place.
 * @param arena The arena for allocations.
 * @param filename The source file path.
 * @return Exit code.
 */
static int cmd_fmt(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) formatter command orchestration
    assert(arena != NULL);
    assert(filename != NULL);
    if (!arena || !filename) {
        return 1;
    }

    char* source = read_file(arena, filename);
    if (!source) {
        error_print("cannot read file '%s'", filename);
        return 1;
    }

    const char* normalized = normalize_source(arena, source);
    if (!write_text_file(filename, normalized)) {
        error_print("cannot write formatted source to '%s'", filename);
        return 1;
    }

    log_info("Formatted %s\n", filename);
    return 0;
}

/**
 * Test command: run unit tests or doc-style tests.
 * @param arena Unused for test command (may be NULL).
 * @param filename Optional file/directory path for scoped doc tests.
 * @return Exit code from the underlying test command.
 */
static int cmd_test(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command wrapper with env overrides
    (void)arena;

    const char* test_override = getenv("FERN_TEST_CMD");
    const char* doc_override = getenv("FERN_TEST_DOC_CMD");
    const char* test_command = (test_override && test_override[0] != '\0') ? test_override : "just test";
    const char* doc_command_base = (doc_override && doc_override[0] != '\0')
        ? doc_override
        : "python3 scripts/run_doc_tests.py";
    const char* doc_command = doc_command_base;
    char quoted_path[1024];
    char doc_command_buf[2304];

    if (filename && filename[0] != '\0') {
        if (!shell_quote_arg(filename, quoted_path, sizeof(quoted_path))) {
            error_print("failed to process test path argument");
            return 1;
        }

        int wrote = snprintf(
            doc_command_buf,
            sizeof(doc_command_buf),
            "%s --path %s",
            doc_command_base,
            quoted_path
        );
        if (wrote < 0 || (size_t)wrote >= sizeof(doc_command_buf)) {
            error_print("failed to build documentation test command");
            return 1;
        }
        doc_command = doc_command_buf;
    }

    if (g_test_doc_mode) {
        log_info("Running documentation tests...\n");
        log_verbose("verbose: test doc command=%s\n", doc_command);
        return run_shell_command(doc_command);
    }

    log_info("Running tests...\n");
    log_verbose("verbose: test command=%s\n", test_command);
    int test_exit = run_shell_command(test_command);
    if (test_exit != 0) {
        return test_exit;
    }

    log_info("Running documentation tests...\n");
    log_verbose("verbose: test doc command=%s\n", doc_command);
    return run_shell_command(doc_command);
}

/**
 * Doc command: generate project/module documentation.
 * @param arena Unused for doc command (may be NULL).
 * @param filename Optional path to a Fern source file or directory.
 * @return Exit code from the underlying documentation command.
 */
static int cmd_doc(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) command wrapper with env overrides and optional args
    (void)arena;

    const char* override = NULL;
    const char* command = NULL;
    char quoted_path[1024];
    char command_buf[2304];

    if (g_doc_open_mode) {
        override = getenv("FERN_DOC_OPEN_CMD");
        command = (override && override[0] != '\0') ? override : NULL;
        log_info("Generating documentation and opening output...\n");
    } else {
        override = getenv("FERN_DOC_CMD");
        command = (override && override[0] != '\0') ? override : NULL;
        log_info("Generating documentation...\n");
    }

    if (command == NULL) {
        size_t used = 0;
        int wrote = snprintf(command_buf, sizeof(command_buf), "python3 scripts/generate_docs.py");
        if (wrote < 0 || (size_t)wrote >= sizeof(command_buf)) {
            error_print("failed to build documentation command");
            return 1;
        }
        used = (size_t)wrote;

        if (g_doc_open_mode) {
            wrote = snprintf(command_buf + used, sizeof(command_buf) - used, " --open");
            if (wrote < 0 || (size_t)wrote >= sizeof(command_buf) - used) {
                error_print("failed to build documentation command");
                return 1;
            }
            used += (size_t)wrote;
        }

        if (g_doc_html_mode) {
            wrote = snprintf(command_buf + used, sizeof(command_buf) - used, " --html");
            if (wrote < 0 || (size_t)wrote >= sizeof(command_buf) - used) {
                error_print("failed to build documentation command");
                return 1;
            }
            used += (size_t)wrote;
        }

        if (filename && filename[0] != '\0') {
            if (!shell_quote_arg(filename, quoted_path, sizeof(quoted_path))) {
                error_print("failed to process documentation path argument");
                return 1;
            }
            wrote = snprintf(command_buf + used, sizeof(command_buf) - used, " --path %s", quoted_path);
            if (wrote < 0 || (size_t)wrote >= sizeof(command_buf) - used) {
                error_print("failed to build documentation command");
                return 1;
            }
        }
        command = command_buf;
    }

    log_verbose("verbose: doc command=%s\n", command);
    return run_shell_command(command);
}

/**
 * LSP command: start language server on stdio.
 * @param arena The arena for allocations.
 * @param filename Unused for LSP command (may be NULL).
 * @return Exit code.
 */
static int cmd_lsp(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) simple command handler
    (void)filename;  // LSP doesn't take a file argument
    
    // Log to a file in /tmp for debugging (set to NULL to disable logging)
    const char* log_file = "/tmp/fern-lsp.log";
    
    LspServer* server = lsp_server_new(arena, log_file);
    if (!server) {
        error_print("failed to initialize language server");
        return 1;
    }
    
    int result = lsp_server_run(server);
    lsp_server_free(server);
    
    return result;
}

/**
 * REPL command: start interactive mode.
 * @param arena The arena for allocations.
 * @param filename Unused for REPL command (may be NULL).
 * @return Exit code.
 */
static int cmd_repl(Arena* arena, const char* filename) {
    // FERN_STYLE: allow(assertion-density) simple command handler
    (void)filename;  // REPL doesn't take a file argument
    
    Repl* repl = repl_new(arena);
    if (!repl) {
        error_print("failed to initialize REPL");
        return 1;
    }
    
    return repl_run(repl);
}

/* ========== Main Entry Point ========== */

/**
 * Main entry point for the Fern compiler.
 * @param argc The argument count.
 * @param argv The argument values.
 * @return Exit code (0 on success, non-zero on error).
 */
int main(int argc, char** argv) {
    // FERN_STYLE: allow(assertion-density) main entry point - handles args and setup
    // FERN_STYLE: allow(function-length) main() is cohesive arg parsing flow
    
    // Store executable path for runtime library lookup
    g_exe_path = argv[0];
    g_test_doc_mode = false;
    g_doc_open_mode = false;
    g_doc_html_mode = false;
    
    // Need at least a command
    if (argc < 2) {
        print_usage();
        return 1;
    }

    int arg_index = 1;

    // Parse global flags before command.
    while (arg_index < argc && argv[arg_index][0] == '-') {
        if (strcmp(argv[arg_index], "-h") == 0 || strcmp(argv[arg_index], "--help") == 0) {
            print_usage();
            return 0;
        }
        if (strcmp(argv[arg_index], "-v") == 0 || strcmp(argv[arg_index], "--version") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[arg_index], "--quiet") == 0) {
            g_log_level = LOG_QUIET;
            arg_index++;
            continue;
        }
        if (strcmp(argv[arg_index], "--verbose") == 0) {
            g_log_level = LOG_VERBOSE;
            arg_index++;
            continue;
        }

        ErrorsColorMode color_mode = ERRORS_COLOR_AUTO;
        if (parse_color_flag(argv[arg_index], &color_mode)) {
            errors_set_color_mode(color_mode);
            arg_index++;
            continue;
        }
        error_print("unknown option '%s'", argv[arg_index]);
        return 1;
    }

    if (arg_index >= argc) {
        error_print("missing command");
        fprintf(stderr, "\n");
        print_usage();
        return 1;
    }
    
    // Find command
    const Command* cmd = find_command(argv[arg_index]);
    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[arg_index]);
        print_usage();
        return 1;
    }

    arg_index++;
    
    // Check if command requires a file argument (args field is non-empty)
    bool needs_file = cmd->args && cmd->args[0] != '\0';
    
    // Parse command-specific options
    while (arg_index < argc && argv[arg_index][0] == '-') {
        if (strcmp(argv[arg_index], "--quiet") == 0) {
            g_log_level = LOG_QUIET;
            arg_index++;
            continue;
        }
        if (strcmp(argv[arg_index], "--verbose") == 0) {
            g_log_level = LOG_VERBOSE;
            arg_index++;
            continue;
        }

        ErrorsColorMode color_mode = ERRORS_COLOR_AUTO;
        if (parse_color_flag(argv[arg_index], &color_mode)) {
            errors_set_color_mode(color_mode);
            arg_index++;
            continue;
        }

        if ((strcmp(argv[arg_index], "-o") == 0 || strcmp(argv[arg_index], "--output") == 0)) {
            if (strcmp(cmd->name, "build") != 0) {
                error_print("-o is only valid for the build command");
                return 1;
            }
            if (arg_index + 1 >= argc) {
                error_print("-o requires an argument");
                return 1;
            }
            g_output_file = argv[arg_index + 1];
            arg_index += 2;
        } else if (strcmp(argv[arg_index], "--doc") == 0) {
            if (strcmp(cmd->name, "test") != 0) {
                error_print("--doc is only valid for the test command");
                return 1;
            }
            g_test_doc_mode = true;
            arg_index++;
        } else if (strcmp(argv[arg_index], "--open") == 0) {
            if (strcmp(cmd->name, "doc") != 0) {
                error_print("--open is only valid for the doc command");
                return 1;
            }
            g_doc_open_mode = true;
            arg_index++;
        } else if (strcmp(argv[arg_index], "--html") == 0) {
            if (strcmp(cmd->name, "doc") != 0) {
                error_print("--html is only valid for the doc command");
                return 1;
            }
            g_doc_html_mode = true;
            arg_index++;
        } else {
            error_print("unknown option '%s'", argv[arg_index]);
            return 1;
        }
    }

    log_verbose("verbose: command=%s\n", cmd->name);
    
    // Need a file argument for commands that require it
    const char* filename = NULL;
    if (needs_file) {
        if (arg_index >= argc) {
            error_print("missing file argument");
            fprintf(stderr, "\n");
            print_usage();
            return 1;
        }
        filename = argv[arg_index];
        arg_index++;
    } else if (arg_index < argc) {
        filename = argv[arg_index];
        arg_index++;
    }
    if (arg_index < argc) {
        error_print("unexpected argument '%s'", argv[arg_index]);
        return 1;
    }
    
    // Create arena for compiler session
    Arena* arena = arena_create(4 * 1024 * 1024);  // 4MB
    if (!arena) {
        error_print("failed to initialize memory");
        return 1;
    }
    
    int result = cmd->handler(arena, filename);
    
    arena_destroy(arena);
    return result;
}
