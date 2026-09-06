#include "fern_runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static const FernJsonCodec sum_codec;
static const FernJsonCodec integer = {.kind=0};
static const FernJsonCodec* fields[] = {&integer,&sum_codec};
static const FernJsonCodecVariant variants[] = {{"End",0,NULL},{"Link",2,fields}};
static const FernJsonCodec sum_codec = {.kind=12,.count=2,.variants=variants};
/** Pin original tag names, strict wire errors, full64 values and cyclic input limits. */
int fern_main(void) {
    int64_t end[]={0}; int64_t link[]={1,INT64_C(9007199254740993),(int64_t)(intptr_t)end};
    int64_t result=fern_json_codec_encode(&sum_codec,(int64_t)(intptr_t)link);
    if (!fern_result_is_ok(result)) return 1;
    const char* expected="{\"tag\":\"Link\",\"fields\":[9007199254740993,{\"tag\":\"End\",\"fields\":[]}]}";
    if(strcmp((const char*)(intptr_t)fern_result_unwrap(result),expected))return 2;
    result=fern_json_codec_decode(&sum_codec,expected);if(!fern_result_is_ok(result))return 3;
    const int64_t* decoded=(const void*)(intptr_t)fern_result_unwrap(result);
    if(decoded[0]!=1 || decoded[1]!=link[1] || *(const int64_t*)(intptr_t)decoded[2]!=0)return 4;
    const char* invalid[]={"{\"tag\":\"Bad\",\"fields\":[]}","{\"tag\":\"End\"}","{\"tag\":\"End\",\"fields\":[],\"extra\":0}","{\"tag\":\"Link\",\"fields\":[1.2,null]}"};
    int codes[]={13,6,12,9};const char* paths[]={"/tag","/fields","/extra","/fields/0"};
    for(size_t i=0;i<4;i++) {
        result=fern_json_codec_decode(&sum_codec,invalid[i]);if(fern_result_is_ok(result))return 5;
        FernJsonError* error=(void*)(intptr_t)fern_result_unwrap(result);
        if(fern_json_value_error_code(error)!=codes[i] || fern_json_value_error_offset(error)!=-1 || strcmp(fern_json_value_error_path(error),paths[i]))return 6;
    }
    link[2]=(int64_t)(intptr_t)link;
    result=fern_json_codec_encode(&sum_codec,(int64_t)(intptr_t)link);if(fern_result_is_ok(result))return 7;
    if(fern_json_value_error_code((void*)(intptr_t)fern_result_unwrap(result))!=4)return 8;
    end[0]=9;result=fern_json_codec_encode(&sum_codec,(int64_t)(intptr_t)end);
    if(fern_result_is_ok(result) || fern_json_value_error_code((void*)(intptr_t)fern_result_unwrap(result))!=13)return 9;
    puts("sum JSON wire, numeric, path and cycle contracts passed");return 0;
}
