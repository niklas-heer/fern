/*
 * QBE Compiler Backend - Library Interface
 *
 * This header exposes QBE as a library for the Fern compiler.
 * QBE is embedded directly into the fern binary - no external dependency needed.
 */

#ifndef QBE_H
#define QBE_H

#include <stdio.h>

/*
 * Compile QBE IR from input FILE* to assembly output FILE*.
 *
 * @param input    FILE* containing QBE IR (.ssa format)
 * @param output   FILE* to write assembly output
 * @param filename Name of input file (for error messages)
 * @return 0 on success, non-zero on error
 */
int qbe_compile(FILE *input, FILE *output, const char *filename);

/*
 * Compile QBE IR from a string buffer to assembly output FILE*.
 *
 * @param ssa_input  String containing QBE IR
 * @param output     FILE* to write assembly output
 * @param filename   Name for error messages
 * @return 0 on success, non-zero on error
 */
int qbe_compile_str(const char *ssa_input, FILE *output, const char *filename);

#endif /* QBE_H */
