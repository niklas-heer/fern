#include "fern_runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const FernJsonCodec integer_codec = {0,0,{.children=NULL},NULL};
static const FernJsonCodec string_codec = {3,0,{.children=NULL},NULL};
static const FernJsonCodec* list_children[] = {&integer_codec};
static const FernJsonCodec list_codec = {6,1,{.children=list_children},NULL};
static const FernJsonCodec* fields[] = {&integer_codec,&string_codec};
static const char* names[] = {"age","name"};
static const FernJsonCodec record_codec = {10,2,{.children=fields},names};
static int fail(const char* message) { fprintf(stderr,"%s\n",message); return 1; }
int fern_main(void) {
    int64_t value=fern_json_codec_decode(&integer_codec,"9007199254740993");
    if (!fern_result_is_ok(value) || fern_result_unwrap(value)!=9007199254740993LL) return fail("Int64 decode");
    value=fern_json_codec_decode(&list_codec,"[1,1.5]");
    if (fern_result_is_ok(value)) return fail("fraction accepted");
    FernJsonError* error=(void*)(intptr_t)fern_result_unwrap(value);
    if (fern_json_value_error_code(error)!=9 || fern_json_value_error_offset(error)!=-1 || strcmp(fern_json_value_error_path(error),"/1")) return fail("fraction path/code/offset");
    value=fern_json_codec_decode(&record_codec,"{\"age\":7,\"name\":\"Fern\",\"extra\":0}");
    if (fern_result_is_ok(value)) return fail("unknown field accepted");
    error=(void*)(intptr_t)fern_result_unwrap(value);
    if (fern_json_value_error_code(error)!=12 || strcmp(fern_json_value_error_path(error),"/extra")) return fail("unknown field path");
    int64_t record[]={0,42,(int64_t)(intptr_t)"Fern"};
    value=fern_json_codec_encode(&record_codec,(int64_t)(intptr_t)record);
    if (!fern_result_is_ok(value) || strcmp((void*)(intptr_t)fern_result_unwrap(value),"{\"age\":42,\"name\":\"Fern\"}")) return fail("record encode");
    puts("typed JSON runtime cases passed");
    return 0;
}
