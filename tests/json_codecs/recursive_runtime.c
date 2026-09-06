#include "fern_runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const FernJsonCodec record_codec;
static const FernJsonCodec* list_children[] = {&record_codec};
static const FernJsonCodec list_codec = {6,1,list_children,NULL};
static const FernJsonCodec* record_children[] = {&list_codec};
static const char* names[] = {"children"};
static const FernJsonCodec record_codec = {10,1,record_children,names};
/** A hostile cyclic value must spend depth/work instead of following the schema cache forever. */
int fern_main(void) {
    int64_t record[2] = {0,0};
    int64_t child = (int64_t)(intptr_t)record;
    FernList list = {.data=&child,.len=1,.cap=1};
    record[1] = (int64_t)(intptr_t)&list;
    int64_t result = fern_json_codec_encode(&record_codec,(int64_t)(intptr_t)record);
    if (fern_result_is_ok(result)) {
        fputs("cyclic codec input succeeded\n",stderr); return 1;
    }
    FernJsonError* error = (void*)(intptr_t)fern_result_unwrap(result);
    if (fern_json_value_error_code(error)!=4 || fern_json_value_error_offset(error)!=-1 || strlen(fern_json_value_error_path(error))!=704) {
        fputs("cyclic codec depth/path budget changed\n",stderr); return 1;
    }
    result = fern_json_codec_decode(&record_codec,"{\"children\":[]}");
    if (!fern_result_is_ok(result)) {
        fputs("finite base rejected by recursive descriptor\n",stderr); return 1;
    }
    puts("recursive JSON runtime depth and finite base passed"); return 0;
}
