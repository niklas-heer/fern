/** Exercise the actual shared runtime repetition boundary without large output. */
#include "fern_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Repeat argv[1] argv[2] times and report length; requires both arguments. */
int fern_main(void) {
    assert(fern_args_count() == 3);
    assert(fern_arg(1) != NULL);
    char* end = NULL;
    int64_t count = strtoll(fern_arg(2), &end, 10);
    assert(end != NULL && *end == '\0');
    const char* result = fern_str_repeat(fern_arg(1), count);
    printf("%zu\n", strlen(result));
    return 0;
}
