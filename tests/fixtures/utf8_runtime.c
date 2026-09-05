/** Exercise byte slicing and scalar splitting in the actual shared runtime. */
#include "fern_runtime.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Print a string as bytes so malformed UTF-8 cannot hide a regression. */
static void print_bytes(const char* text) {
    assert(text != NULL);
    size_t len = strlen(text);
    assert(len < 1024);
    for (size_t i = 0; i < len; i++) printf("%02x", (unsigned char)text[i]);
    printf("\n");
}

/** Run the requested slice or split operation; arguments come from the harness. */
int fern_main(void) {
    if (fern_args_count() < 4) return 2;
    assert(fern_args_count() >= 4);
    assert(fern_arg(1) != NULL);
    if (strcmp(fern_arg(1), "validate_split") == 0) {
        printf("%" PRId64 "\n", fern_str_split_is_valid(fern_arg(2), fern_arg(3)));
    } else if (strcmp(fern_arg(1), "slice") == 0) {
        assert(fern_args_count() == 5);
        print_bytes(fern_str_slice(fern_arg(2), strtoll(fern_arg(3), NULL, 10),
                                   strtoll(fern_arg(4), NULL, 10)));
    } else {
        FernStringList* parts = fern_str_split(fern_arg(2), fern_arg(3));
        printf("%" PRId64 "\n", parts->len);
        for (int64_t i = 0; i < parts->len; i++) print_bytes(parts->data[i]);
    }
    return 0;
}
