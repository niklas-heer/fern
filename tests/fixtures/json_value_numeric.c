/** Stream exact-conversion results for the independent Python Decimal oracle. */
#include "fern_runtime.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
/** Process at most10000 bounded number lines and print raw numeric Result payloads. */
int fern_main(void) {
    char* line = fern_alloc(1048578);
    for (size_t rows = 0; rows < 10000 && fgets(line, 1048578, stdin); rows++) {
        line[strcspn(line, "\n")] = 0;
        int64_t parsed = fern_json_value_parse(line);
        if (!fern_result_is_ok(parsed)) return 2;
        const FernJsonValue* value = (void*)(intptr_t)fern_result_unwrap(parsed);
        int64_t result = fern_json_value_as_int(value);
        if (fern_result_is_ok(result)) printf("ok:%" PRId64 " ", fern_result_unwrap(result));
        else printf("err:%" PRId64 " ", fern_json_value_error_code((void*)(intptr_t)fern_result_unwrap(result)));
        result = fern_json_value_as_float(value);
        if (fern_result_is_ok(result)) printf("ok:%016" PRIx64 "\n", (uint64_t)fern_result_unwrap(result));
        else printf("err:%" PRId64 "\n", fern_json_value_error_code((void*)(intptr_t)fern_result_unwrap(result)));
    }
    return 0;
}
