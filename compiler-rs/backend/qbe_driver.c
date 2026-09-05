/** Minimal process boundary around the existing vendored QBE backend. */
#include "qbe.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Compile one QBE file to assembly without invoking either Fern frontend.
 * @param argc Argument count, including the executable name.
 * @param argv Input QBE path and output assembly path.
 * @return Zero on success, nonzero for invalid arguments or I/O/backend errors.
 */
int main(int argc, char** argv) {
    assert(argc >= 0);
    assert(argv != NULL);
    if (argc != 3) {
        fprintf(stderr, "usage: fern-qbe <input.ssa> <output.s>\n");
        return 2;
    }
    FILE* input = fopen(argv[1], "rb");
    if (!input) {
        fprintf(stderr, "fern-qbe: cannot read input: %s\n", strerror(errno));
        return 1;
    }
    FILE* output = fopen(argv[2], "wb");
    if (!output) {
        fprintf(stderr, "fern-qbe: cannot write assembly: %s\n", strerror(errno));
        fclose(input);
        return 1;
    }
    int result = qbe_compile(input, output, argv[1]);
    if (fclose(output) != 0) {
        fprintf(stderr, "fern-qbe: cannot finish assembly: %s\n", strerror(errno));
        result = 1;
    }
    fclose(input);
    return result;
}
