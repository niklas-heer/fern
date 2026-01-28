/* Fern Compiler - Main Entry Point */

#include <stdio.h>
#include <stdlib.h>
#include "arena.h"
#include "fern_string.h"

int main(int argc, char** argv) {
    // FERN_STYLE: allow(assertion-density) main entry point - handles args and setup
    printf("Fern Compiler v0.0.1\n");
    
    if (argc < 2) {
        fprintf(stderr, "Usage: fern <source.fn>\n");
        fprintf(stderr, "       fern <source.🌿>\n");
        fprintf(stderr, "\nBoth .fn and .🌿 file extensions are supported.\n");
        return 1;
    }
    
    // Create arena for compiler session
    Arena* arena = arena_create(1024 * 1024);  // 1MB initial
    if (!arena) {
        fprintf(stderr, "Error: Failed to initialize memory\n");
        return 1;
    }
    
    String* filename = string_new(arena, argv[1]);
    printf("Compiling: %s\n", string_cstr(filename));
    
    // TODO: Implement compilation pipeline
    printf("TODO: Lexer -> Parser -> Type Checker -> Codegen\n");
    
    arena_destroy(arena);
    return 0;
}
