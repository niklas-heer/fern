/* CLI Parse Command Implementation */

#include "cli_parse.h"
#include "ast_print.h"
#include "ast_validate.h"
#include "errors.h"
#include "fern_string.h"
#include "parser.h"
#include <assert.h>

/**
 * Parse source and print AST in the standard CLI format.
 * @param arena The arena for allocations.
 * @param filename The logical filename for diagnostics/output.
 * @param source The source code to parse.
 * @param out The output stream for AST printing.
 * @return Exit code (0 on success, 1 on error).
 */
int fern_parse_source(Arena* arena, const char* filename, const char* source, FILE* out) {
    // FERN_STYLE: allow(function-length) central CLI parse path
    assert(arena != NULL);
    assert(filename != NULL);
    assert(source != NULL);
    assert(out != NULL);

    Parser* parser = parser_new(arena, source);
    StmtVec* stmts = parse_stmts(parser);

    if (parser_had_error(parser)) {
        error_print("parse error in %s", filename);
        note_print("while parsing %s", filename);
        help_print("fix the highlighted syntax and rerun: fern parse %s", filename);
        return 1;
    }

    AstValidationError v_err = {0};
    if (!ast_validate_program(stmts, &v_err)) {
        const char* err_file = filename;
        int err_line = 1;
        int err_col = 0;
        if (v_err.loc.filename) err_file = string_cstr(v_err.loc.filename);
        if (v_err.loc.line > 0) err_line = (int)v_err.loc.line;
        if (v_err.loc.column > 0) err_col = (int)v_err.loc.column;
        error_location(err_file, err_line, err_col);
        error_print("invalid AST: %s", v_err.message ? v_err.message : "unknown error");
        return 1;
    }

    fprintf(out, "AST for %s:\n\n", filename);
    for (size_t i = 0; i < stmts->len; i++) {
        ast_print_stmt(out, stmts->data[i], 0);
        fprintf(out, "\n");
    }

    return 0;
}
