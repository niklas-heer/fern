/* CLI Parse Command Helpers
 *
 * Shared implementation for the `fern parse` command.
 */

#ifndef FERN_CLI_PARSE_H
#define FERN_CLI_PARSE_H

#include "arena.h"
#include <stdio.h>

/**
 * Parse source and print AST in the standard CLI format.
 * @param arena The arena for allocations.
 * @param filename The logical filename for diagnostics/output.
 * @param source The source code to parse.
 * @param out The output stream for AST printing.
 * @return Exit code (0 on success, 1 on error).
 */
int fern_parse_source(Arena* arena, const char* filename, const char* source, FILE* out);

#endif /* FERN_CLI_PARSE_H */
