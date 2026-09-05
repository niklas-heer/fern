/** Probe shared-runtime list preconditions without relying on debug assertions. */
#include "fern_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

/** Turn legacy assertion failures into a bounded test result, avoiding OS crash UI. */
static void assertion_failed(int signal_number) {
    (void)signal_number;
    _Exit(99);
}

/** Run one fixed valid or invalid access scenario selected by the first argument. */
int fern_main(void) {
    signal(SIGABRT, assertion_failed);
    assert(fern_args_count() == 2);
    const char* mode = fern_arg(1);
    assert(mode != NULL);
    FernList* list = fern_list_new();
    if (strcmp(mode, "head_empty") == 0) return (int)fern_list_head(list);
    fern_list_push_mut(list, INT64_MAX);
    int64_t index = 0;
    if (strcmp(mode, "negative") == 0) index = -1;
    if (strcmp(mode, "end") == 0) index = 1;
    if (strcmp(mode, "huge") == 0) index = INT64_MAX;
    int64_t value = strcmp(mode, "head") == 0 ? fern_list_head(list) : fern_list_get(list, index);
    printf("%lld\n", (long long)value);
    return 0;
}
