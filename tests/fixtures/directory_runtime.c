/** Directory Result ABI regression fixture; runtime owns all returned allocations. */
#include "fern_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

/** Print either a concrete error code or all successful directory entries. */
int fern_main(void) {
    assert(fern_args_count() == 2);
    const char* path = fern_arg(1);
    assert(path != NULL);
    int64_t result = fern_read_dir_result(path);
    if (!fern_result_is_ok(result)) {
        printf("err:%lld\n", (long long)fern_result_unwrap(result));
        return 0;
    }
    FernStringList* list = (FernStringList*)(intptr_t)fern_result_unwrap(result);
    assert(list != NULL);
    printf("ok:%lld\n", (long long)list->len);
    for (int64_t index = 0; index < list->len; index++) puts(list->data[index]);
    return 0;
}
