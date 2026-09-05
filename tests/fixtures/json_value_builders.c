/** Native immutable builders and collection ABI acceptance checks. */
#include "fern_runtime.h"
#include "fern_gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
static size_t checks;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr,"JSON builder failure line%d: %s\n",__LINE__,#c); exit(1); } } while (0)
/** Read a successful payload. */
static int64_t ok(int64_t result) { CHECK(fern_result_is_ok(result)); return fern_result_unwrap(result); }
/** Check ordinary builder errors have no input offset. */
static void error(int64_t result, int code) {
    CHECK(!fern_result_is_ok(result));
    const FernJsonError* e=(void*)(intptr_t)fern_result_unwrap(result);
    CHECK(fern_json_value_error_code(e)==code);
    CHECK(fern_json_value_error_offset(e)==-1);
}
/** Check exact encoding of a valid value. */
static void encoded(const FernJsonValue* value,const char* expected) {
    const char* text=(void*)(intptr_t)ok(fern_json_value_stringify(value));
    CHECK(strcmp(text,expected)==0);
}
/** Test every scalar builder and exceptional conversion. */
static void scalars(void) {
    encoded(fern_json_value_null(),"null");
    encoded((void*)(intptr_t)ok(fern_json_value_from_string("[]")),"\"[]\"");
    encoded(fern_json_value_from_bool(1),"true");
    encoded(fern_json_value_from_int(INT64_MIN),"-9223372036854775808");
    encoded((void*)(intptr_t)ok(fern_json_value_from_float(-0.0)),"-0");
    encoded((void*)(intptr_t)ok(fern_json_value_from_float(1.5)),"1.5");
    encoded((void*)(intptr_t)ok(fern_json_value_from_string("🌿\n")),"\"🌿\\n\"");
    encoded((void*)(intptr_t)ok(fern_json_value_from_number_text("1.00e99")),"1.00e99");
    error(fern_json_value_from_float(INFINITY),11);
    error(fern_json_value_from_float(NAN),11);
    error(fern_json_value_from_string("\300\200"),2);
    error(fern_json_value_from_number_text(" 1"),1);
    error(fern_json_value_from_number_text("1 "),1);
    error(fern_json_value_from_number_text("true"),1);
    error(fern_json_value_from_number_text("\357\273\2771"),1);
}
/** Arrays copy storage while preserving child aliases across GC. */
static void arrays(void) {
    FernList* list=fern_list_with_capacity(1);
    fern_list_push_mut(list,(int64_t)(intptr_t)fern_json_value_from_int(7));
    const FernJsonValue* value=(void*)(intptr_t)ok(fern_json_value_from_array(list));
    list->data[0]=(int64_t)(intptr_t)fern_json_value_null();
    fern_gc_collect();
    encoded(value,"[7]");
    FernList* elements=(void*)(intptr_t)ok(fern_json_value_elements(value));
    CHECK(elements->len==1);
    CHECK(ok(fern_json_value_as_int((void*)(intptr_t)elements->data[0]))==7);
    elements->data[0]=(int64_t)(intptr_t)fern_json_value_null();
    encoded(value,"[7]");
    encoded((void*)(intptr_t)ok(fern_json_value_from_array(fern_list_with_capacity(1))),"[]");
    error(fern_json_value_elements(fern_json_value_null()),5);
}
/** Native member records retain JSON String keys including NUL. */
static void objects(void) {
    FernList* keys=fern_list_with_capacity(2);
    FernList* values=fern_list_with_capacity(2);
    fern_list_push_mut(keys,(int64_t)(intptr_t)"b");
    fern_list_push_mut(keys,(int64_t)(intptr_t)"a");
    fern_list_push_mut(values,(int64_t)(intptr_t)fern_json_value_from_int(2));
    fern_list_push_mut(values,(int64_t)(intptr_t)fern_json_value_from_int(1));
    const FernJsonValue* object=(void*)(intptr_t)ok(fern_json_value_from_object(keys,values));
    encoded(object,"{\"b\":2,\"a\":1}");
    keys->data[1]=keys->data[0];
    error(fern_json_value_from_object(keys,values),3);
    object=(void*)(intptr_t)ok(fern_json_value_parse("{\"a\\u0000b\":7}"));
    FernList* members=(void*)(intptr_t)ok(fern_json_value_members(object));
    CHECK(members->len==1);
    const FernJsonMember* member=(void*)(intptr_t)members->data[0];
    encoded(member->key,"\"a\\u0000b\"");
    CHECK(ok(fern_json_value_as_int(member->value))==7);
    error(fern_json_value_as_string(member->key),10);
    error(fern_json_value_members(fern_json_value_null()),5);
}
/** Expanded nodes, depth, encoded bytes and adapter preflights are bounded. */
static void bounds(void) {
    const FernJsonValue* value=fern_json_value_null();
    FernList* list=fern_list_with_capacity(2);
    list->len=2;
    for (size_t i=0;i<15;i++) {
        list->data[0]=list->data[1]=(int64_t)(intptr_t)value;
        value=(void*)(intptr_t)ok(fern_json_value_from_array(list));
    }
    list->data[0]=list->data[1]=(int64_t)(intptr_t)value;
    error(fern_json_value_from_array(list),4);
    value=fern_json_value_null(); list->len=1;
    for (size_t i=0;i<127;i++) { list->data[0]=(int64_t)(intptr_t)value; value=(void*)(intptr_t)ok(fern_json_value_from_array(list)); }
    list->data[0]=(int64_t)(intptr_t)value;
    error(fern_json_value_from_array(list),4);
    char* text=fern_alloc(1048578); memset(text,1,1048576); text[1048576]=0;
    value=(void*)(intptr_t)ok(fern_json_value_from_string(text));
    FernList* repeated=fern_list_with_capacity(3);
    for (size_t i=0;i<3;i++) fern_list_push_mut(repeated,(int64_t)(intptr_t)value);
    error(fern_json_value_from_array(repeated),4);
    text[1048576]=1; text[1048577]=0;
    error(fern_json_value_from_string(text),4);
    FernList oversized={.len=100000,.cap=100000,.data=NULL};
    error(fern_json_value_from_array(&oversized),4);
    oversized.len=oversized.cap=50000;
    error(fern_json_value_from_object(&oversized,&oversized),4);
    error(fern_json_value_limit_error(),4);
}
/** Actual shared children reach the exact16 MiB encoded ceiling without expansion. */
static void output_boundary(void) {
    char* text=fern_alloc(1048574);
    memset(text,'x',1048573); text[1048573]=0;
    const FernJsonValue* large=(void*)(intptr_t)ok(fern_json_value_from_string(text));
    text[1048572]=0;
    const FernJsonValue* small=(void*)(intptr_t)ok(fern_json_value_from_string(text));
    FernList* values=fern_list_with_capacity(16);
    for (size_t i=0;i<15;i++) fern_list_push_mut(values,(int64_t)(intptr_t)large);
    fern_list_push_mut(values,(int64_t)(intptr_t)small);
    const FernJsonValue* exact=(void*)(intptr_t)ok(fern_json_value_from_array(values));
    const char* encoded=(void*)(intptr_t)ok(fern_json_value_stringify(exact));
    CHECK(strlen(encoded)==16777216);
    values->data[15]=(int64_t)(intptr_t)large;
    error(fern_json_value_from_array(values),4);
}

/** Run builder ABI tests against the runtime allocator. */
int fern_main(void) { fern_gc_init(); scalars(); arrays(); objects(); bounds(); output_boundary(); printf("JSON builders: %zu checks passed\n",checks); return 0; }
