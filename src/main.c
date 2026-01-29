/* Fern Compiler - Main Entry Point */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "arena.h"
#include "fern_string.h"
#include "lexer.h"
#include "parser.h"
#include "checker.h"
#include "codegen.h"
#include "ast_print.h"
#include "version.h"
#include "errors.h"

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

/** All available commands. */
static const Command COMMANDS[] = {
    {"build", "<file>", "Compile to executable",       cmd_build},
    {"run",   "<file>", "Compile and run immediately", cmd_run},
    {"check", "<file>", "Type check only",             cmd_check},
    {"emit",  "<file>", "Emit QBE IR to stdout",       cmd_emit},
    {"lex",   "<file>", "Show tokens (debug)",         cmd_lex},
    {"parse", "<file>", "Show AST (debug)",            cmd_parse},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

/** All available options. */
static const Option OPTIONS[] = {
    {"-h", "--help",    "Show this help message"},
    {"-v", "--version", "Show version information"},
    {"-o", "--output",  "Output file (build only)"},
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
        snprintf(flags, sizeof(flags), "%s, %s", opt->short_flag, opt->long_flag);
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
 * Compile a Fern source file to QBE IR.
 * @param arena The arena for allocations.
 * @param source The source code.
 * @param filename The source filename (for error messages).
 * @return The codegen instance, or NULL on error.
 */
static Codegen* compile_to_qbe(Arena* arena, const char* source, const char* filename) {
    // FERN_STYLE: allow(assertion-density) pipeline orchestration
    
    // Parse
    Parser* parser = parser_new(arena, source);
    StmtVec* stmts = parse_stmts(parser);
    
    if (parser_had_error(parser)) {
        error_location(filename, 1, 0);
        error_print("parse error");
        return NULL;
    }
    
    if (!stmts || stmts->len == 0) {
        error_location(filename, 1, 0);
        error_print("no statements found");
        return NULL;
    }
    
    // Type check
    Checker* checker = checker_new(arena);
    bool check_ok = checker_check_stmts(checker, stmts);
    
    if (!check_ok || checker_has_errors(checker)) {
        const char* err = checker_first_error(checker);
        error_print("%s", err ? err : "type error");
        return NULL;
    }
    
    // Generate QBE IR
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

/**
 * Run QBE compiler and linker to create executable.
 * @param ssa_file Path to QBE IR file.
 * @param output_file Path for output executable.
 * @return 0 on success, non-zero on error.
 */
static int run_qbe_and_link(const char* ssa_file, const char* output_file) {
    // FERN_STYLE: allow(assertion-density) external process invocation
    char cmd[1024];
    int ret;
    
    // Generate object file with QBE
    char obj_file[256];
    snprintf(obj_file, sizeof(obj_file), "%s.o", output_file);
    
    snprintf(cmd, sizeof(cmd), "qbe -o %s.s %s 2>&1", output_file, ssa_file);
    ret = system(cmd);
    if (ret != 0) {
        error_print("QBE compilation failed (is qbe installed?)");
        note_print("install QBE from https://c9x.me/compile/");
        return 1;
    }
    
    // Assemble
    snprintf(cmd, sizeof(cmd), "cc -c -o %s %s.s 2>&1", obj_file, output_file);
    ret = system(cmd);
    if (ret != 0) {
        error_print("assembly failed");
        return 1;
    }
    
    // Find and link with runtime library
    char runtime_lib[512];
    if (g_exe_path && find_runtime_lib(g_exe_path, runtime_lib, sizeof(runtime_lib))) {
        snprintf(cmd, sizeof(cmd), "cc -o %s %s %s 2>&1", output_file, obj_file, runtime_lib);
    } else {
        // Fall back to linking without runtime (will fail if runtime functions used)
        snprintf(cmd, sizeof(cmd), "cc -o %s %s 2>&1", output_file, obj_file);
    }
    
    ret = system(cmd);
    if (ret != 0) {
        error_print("linking failed");
        return 1;
    }
    
    // Clean up intermediate files
    snprintf(cmd, sizeof(cmd), "%s.s", output_file);
    unlink(cmd);
    unlink(obj_file);
    
    return 0;
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
    
    printf("Compiling %s...\n", filename);
    
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
        printf("Created executable: %s\n", output_file);
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
        error_location(filename, 1, 0);
        error_print("parse error");
        return 1;
    }
    
    // Type check
    Checker* checker = checker_new(arena);
    bool check_ok = checker_check_stmts(checker, stmts);
    
    if (!check_ok || checker_has_errors(checker)) {
        const char* err = checker_first_error(checker);
        error_print("%s", err ? err : "type error");
        return 1;
    }
    
    printf("✓ %s: No type errors\n", filename);
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
    
    // Parse
    Parser* parser = parser_new(arena, source);
    StmtVec* stmts = parse_stmts(parser);
    
    if (parser_had_error(parser)) {
        error_location(filename, 1, 0);
        error_print("parse error");
        return 1;
    }
    
    // Print AST
    printf("AST for %s:\n\n", filename);
    for (size_t i = 0; i < stmts->len; i++) {
        ast_print_stmt(stdout, stmts->data[i], 0);
        printf("\n");
    }
    
    return 0;
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
    
    // Store executable path for runtime library lookup
    g_exe_path = argv[0];
    
    // Handle global options (before command)
    if (argc >= 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage();
            return 0;
        }
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            print_version();
            return 0;
        }
    }
    
    // Need at least command and file
    if (argc < 3) {
        print_usage();
        return 1;
    }
    
    // Find command
    const Command* cmd = find_command(argv[1]);
    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        print_usage();
        return 1;
    }
    
    // Parse command-specific options
    int arg_index = 2;
    while (arg_index < argc && argv[arg_index][0] == '-') {
        if ((strcmp(argv[arg_index], "-o") == 0 || strcmp(argv[arg_index], "--output") == 0)) {
            if (arg_index + 1 >= argc) {
                error_print("-o requires an argument");
                return 1;
            }
            g_output_file = argv[arg_index + 1];
            arg_index += 2;
        } else {
            error_print("unknown option '%s'", argv[arg_index]);
            return 1;
        }
    }
    
    // Need a file argument
    if (arg_index >= argc) {
        error_print("missing file argument");
        fprintf(stderr, "\n");
        print_usage();
        return 1;
    }
    
    const char* filename = argv[arg_index];
    
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
