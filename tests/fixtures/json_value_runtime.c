/** Native JSON DOM contract tests; CHECK remains active in release builds. */
#include "fern_runtime.h"
#include "fern_gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <locale.h>

static size_t checks;
#define CHECK(condition) do { checks++; if (!(condition)) { \
    fprintf(stderr, "JSON check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(1); } } while (0)

/** Extract a successful full-width Result payload. */
static int64_t ok(int64_t result) {
    CHECK(fern_result_is_ok(result));
    return fern_result_unwrap(result);
}

/** Check an error's stable code, offset and nonempty message. */
static void error(int64_t result, int code, int64_t offset) {
    CHECK(!fern_result_is_ok(result));
    const FernJsonError* value = (void*)(intptr_t)fern_result_unwrap(result);
    CHECK(fern_json_value_error_code(value) == code);
    CHECK(fern_json_value_error_offset(value) == offset);
    CHECK(fern_json_value_error_message(value)[0] != '\0');
}

/** Parse a valid JSON document. */
static const FernJsonValue* parse(const char* text) {
    return (void*)(intptr_t)ok(fern_json_value_parse(text));
}

/** Check canonical compact serialization without changing number lexemes. */
static void roundtrip(const char* input, const char* expected) {
    const FernJsonValue* value = parse(input);
    const char* encoded = (void*)(intptr_t)ok(fern_json_value_stringify(value));
    CHECK(strcmp(encoded, expected) == 0);
    CHECK(strcmp((void*)(intptr_t)ok(fern_json_value_stringify(parse(encoded))), expected) == 0);
}

/** Exercise every JSON kind, scalar root, UTF-8 and escape. */
static void valid_documents(void) {
    roundtrip("null", "null");
    roundtrip(" \t\r\ntrue \n", "true");
    roundtrip("false", "false");
    roundtrip("-0.123400e+009", "-0.123400e+009");
    roundtrip("[1,true,null,{},[]]", "[1,true,null,{},[]]");
    roundtrip("{\"z\": 0, \"a\": [2,3]}", "{\"z\":0,\"a\":[2,3]}");
    roundtrip("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", "\"\\\"\\\\/\\b\\f\\n\\r\\t\"");
    roundtrip("\"\\uD83C\\uDF3F\\u0061🌿\"", "\"🌿a🌿\"");
    roundtrip("\"a\\u0000b\"", "\"a\\u0000b\"");
    roundtrip("{\"a\\u0000b\":1}", "{\"a\\u0000b\":1}");
    roundtrip("\357\273\277[1]", "[1]");
    CHECK(fern_json_value_is_null(parse("null")) == 1);
    CHECK(fern_json_value_is_null(parse("false")) == 0);
}

/** Check exact error locations on malformed syntax and Unicode. */
static void invalid_documents(void) {
    struct { const char* text; int code; int offset; } cases[] = {
        {"", 1, 0}, {"[", 1, 1}, {"[1,]", 1, 3}, {"{\"a\":}", 1, 5},
        {"{\"a\":1,}", 1, 7}, {"null x", 1, 5}, {"01", 1, 1},
        {"+1", 1, 0}, {"1.", 1, 2}, {"1e+", 1, 3}, {"NaN", 1, 0},
        {"[1 2]", 1, 3}, {"\vnull", 1, 0}, {"/*x*/null", 1, 0},
        {"\"a\n\"", 1, 2}, {"\"\\x\"", 1, 2}, {"\"abc", 1, 4},
        {"\"\\uD800\"", 2, 1}, {"\"\\uDC00\"", 2, 1},
        {"\"\\uD800\\u0041\"", 2, 1}, {"\"\300\200\"", 2, 1},
        {"\"\355\240\200\"", 2, 1}, {"\"\364\220\200\200\"", 2, 1},
        {"\"\342\202\"", 2, 1}, {"\"\200\"", 2, 1},
        {"{\"a\":1,\"\\u0061\":2}", 3, 7},
        {"\357\273\277\357\273\277null", 1, 3}
    };
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        error(fern_json_value_parse(cases[i].text), cases[i].code, cases[i].offset);
    }
}

/** Navigation distinguishes wrong type, missing key, JSON null and bad index. */
static void accessors(void) {
    const FernJsonValue* object = parse("{\"z\":null,\"a\":[true,\"🌿\",12]}");
    CHECK(ok(fern_json_value_length(object)) == 2);
    const FernJsonValue* array = (void*)(intptr_t)ok(fern_json_value_get(object, "a"));
    CHECK(ok(fern_json_value_length(array)) == 3);
    CHECK(ok(fern_json_value_as_bool((void*)(intptr_t)ok(fern_json_value_at(array, 0)))) == 1);
    const char* text = (void*)(intptr_t)ok(fern_json_value_as_string((void*)(intptr_t)ok(fern_json_value_at(array, 1))));
    CHECK(strcmp(text, "🌿") == 0);
    CHECK(fern_json_value_is_null((void*)(intptr_t)ok(fern_json_value_get(object, "z"))));
    error(fern_json_value_get(object, "missing"), 6, -1);
    error(fern_json_value_get(array, "a"), 5, -1);
    error(fern_json_value_at(array, -1), 7, -1);
    error(fern_json_value_at(array, INT64_MAX), 7, -1);
    error(fern_json_value_at(object, 0), 5, -1);
    error(fern_json_value_length(parse("\"x\"")), 5, -1);
    error(fern_json_value_as_bool(parse("1")), 5, -1);
    error(fern_json_value_as_string(parse("\"a\\u0000b\"")), 10, -1);
    error(fern_json_value_number_text(parse("null")), 5, -1);
    CHECK(strcmp((void*)(intptr_t)ok(fern_json_value_number_text(parse("1.00e99"))), "1.00e99") == 0);
}

/** Integer conversion operates on exact decimal digits, never binary64. */
static void integers(void) {
    struct { const char* text; int64_t value; } valid[] = {
        {"9223372036854775807", INT64_MAX}, {"-9223372036854775808", INT64_MIN},
        {"9007199254740993", INT64_C(9007199254740993)}, {"1.000e2", 100},
        {"1200e-2", 12}, {"0e999999999999999999999", 0}, {"-0.000", 0},
        {"0.000001e6", 1}, {"1e18", INT64_C(1000000000000000000)}
    };
    for (size_t i = 0; i < sizeof(valid)/sizeof(valid[0]); i++) {
        CHECK(ok(fern_json_value_as_int(parse(valid[i].text))) == valid[i].value);
    }
    error(fern_json_value_as_int(parse("9223372036854775808")), 8, -1);
    error(fern_json_value_as_int(parse("-9223372036854775809")), 8, -1);
    error(fern_json_value_as_int(parse("1e999999999999999999999")), 8, -1);
    error(fern_json_value_as_int(parse("1e-99999999999999999999")), 9, -1);
    error(fern_json_value_as_int(parse("1.01")), 9, -1);
    error(fern_json_value_as_int(parse("true")), 5, -1);
}

/** Binary64 payloads preserve bits and reject overflow/nonzero underflow. */
static void floats(void) {
    const char* texts[] = {"0.5", "-0", "1e308", "5e-324"};
    double expected[] = {0.5, -0.0, 1e308, 5e-324};
    for (size_t i = 0; i < 4; i++) {
        int64_t bits = ok(fern_json_value_as_float(parse(texts[i])));
        double value;
        memcpy(&value, &bits, sizeof(value));
        CHECK(value == expected[i]);
        CHECK(signbit(value) == signbit(expected[i]));
    }
    error(fern_json_value_as_float(parse("1e99999")), 8, -1);
    error(fern_json_value_as_float(parse("1e-99999")), 8, -1);
    CHECK(ok(fern_json_value_as_float(parse("0e9999999"))) == 0);
    const char* previous = setlocale(LC_NUMERIC, NULL);
    char saved[128];
    CHECK(strlen(previous) < sizeof(saved));
    memcpy(saved, previous, strlen(previous) + 1);
    (void)setlocale(LC_NUMERIC, "de_DE.UTF-8");
    int64_t bits = ok(fern_json_value_as_float(parse("1.5")));
    double value;
    memcpy(&value, &bits, sizeof(value));
    CHECK(value == 1.5);
    CHECK(setlocale(LC_NUMERIC, saved) != NULL);
}

/** Enforce depth/input/node limits at the exact boundary. */
static void limits(void) {
    char* text = fern_alloc(1048578);
    memset(text, ' ', 1048576);
    memcpy(text, "null", 4);
    text[1048576] = 0;
    CHECK(fern_result_is_ok(fern_json_value_parse(text)));
    text[1048576] = ' '; text[1048577] = 0;
    error(fern_json_value_parse(text), 4, 1048576);
    for (size_t depth = 127; depth <= 128; depth++) {
        memset(text, '[', depth);
        text[depth] = '0';
        memset(text + depth + 1, ']', depth);
        text[depth * 2 + 1] = 0;
        int64_t result = fern_json_value_parse(text);
        if (depth == 127) CHECK(fern_result_is_ok(result));
        else error(result, 4, 128);
    }
    text[0] = '[';
    for (size_t i = 0; i < 100000; i++) { text[1+i*2] = '0'; text[2+i*2] = ','; }
    text[199998] = ']'; text[199999] = 0;
    CHECK(fern_result_is_ok(fern_json_value_parse(text)));
    text[199998] = ','; text[199999] = '0'; text[200000] = ']'; text[200001] = 0;
    error(fern_json_value_parse(text), 4, 199999);
}

/** Every truncated prefix and deterministic byte mutation remains bounded and round-trippable. */
static void malformed_mutations(void) {
    const char* original = "{\"🌿\":[1.25e3,true,null,{\"x\":\"\\uD83C\\uDF3F\"}]}";
    size_t length = strlen(original);
    char input[128];
    for (size_t end = 0; end < length; end++) {
        memcpy(input, original, end); input[end] = 0;
        CHECK(!fern_result_is_ok(fern_json_value_parse(input)));
    }
    for (size_t at = 0; at < length; at++) {
        for (unsigned byte = 1; byte <= 255; byte += 7) {
            memcpy(input, original, length + 1); input[at] = (char)byte;
            int64_t result = fern_json_value_parse(input);
            if (fern_result_is_ok(result)) {
                const FernJsonValue* value = (void*)(intptr_t)fern_result_unwrap(result);
                const char* encoded = (void*)(intptr_t)ok(fern_json_value_stringify(value));
                roundtrip(encoded, encoded);
            } else {
                const FernJsonError* failure = (void*)(intptr_t)fern_result_unwrap(result);
                CHECK(fern_json_value_error_code(failure) >= 1);
                CHECK(fern_json_value_error_offset(failure) <= (int64_t)length);
            }
        }
    }
}

/** Sorted lookup must preserve insertion order and distinguish prefix/NUL names. */
static void object_indices(void) {
    const FernJsonValue* object = parse("{\"abc\":1,\"ab\":2,\"a\":3,\"\":4,\"a\\u0000\":5}");
    const char* keys[] = {"abc", "ab", "a", ""};
    for (size_t i = 0; i < 4; i++) {
        const FernJsonValue* value = (void*)(intptr_t)ok(fern_json_value_get(object, keys[i]));
        CHECK(ok(fern_json_value_as_int(value)) == (int64_t)i + 1);
    }
    error(fern_json_value_parse("{\"🌿\":1,\"\\ud83c\\udf3f\":2}"), 3, 10);
    char* document = fern_alloc(1000000);
    size_t at = 0;
    document[at++] = '{';
    for (size_t i = 0; i < 4000; i++) {
        int n = snprintf(document + at, 1000000 - at, "%s\"shared-prefix-%04zu\":%zu", i ? "," : "", 3999-i, i);
        CHECK(n > 0 && (size_t)n < 1000000 - at);
        at += (size_t)n;
    }
    document[at++] = '}'; document[at] = 0;
    object = parse(document);
    CHECK(strcmp((void*)(intptr_t)ok(fern_json_value_stringify(object)), document) == 0);
    CHECK(ok(fern_json_value_as_int((void*)(intptr_t)ok(fern_json_value_get(object, "shared-prefix-0000")))) == 3999);
}

/** Large common prefixes cannot cause uncharged repeated name comparisons. */
static void comparison_budget(void) {
    char* document = fern_alloc(1000000);
    size_t at = 0;
    document[at++] = '{';
    for (size_t i = 0; i < 4000; i++) {
        if (i) document[at++] = ',';
        document[at++] = '\"';
        memset(document + at, 'a', 220); at += 220;
        int n = snprintf(document + at, 1000000 - at, "%04zu\":0", (i * 1741) % 4000);
        CHECK(n == 7);
        at += (size_t)n;
    }
    document[at++] = '}'; document[at] = 0;
    error(fern_json_value_parse(document), 4, (int64_t)at);
}

/** GC collection must preserve nested values and lookup indices. */
static void retention(void) {
    const FernJsonValue* value = parse("{\"retained\":[9223372036854775807,\"🌿\"]}");
    for (size_t i = 0; i < 2000; i++) (void)parse("{\"temporary\":[1,2,3]}");
    fern_gc_collect();
    const FernJsonValue* array = (void*)(intptr_t)ok(fern_json_value_get(value, "retained"));
    CHECK(ok(fern_json_value_as_int((void*)(intptr_t)ok(fern_json_value_at(array, 0)))) == INT64_MAX);
    CHECK(strcmp((void*)(intptr_t)ok(fern_json_value_stringify(value)), "{\"retained\":[9223372036854775807,\"🌿\"]}") == 0);
    CHECK(strcmp((void*)(intptr_t)ok(fern_json_parse("not json")), "not json") == 0);
    CHECK(strcmp((void*)(intptr_t)ok(fern_json_stringify("")), "") == 0);
}

/** Run the native ABI suite against the actual GC-backed runtime. */
int fern_main(void) {
    fern_gc_init();
    valid_documents(); invalid_documents(); accessors(); integers(); floats(); limits(); malformed_mutations(); object_indices(); comparison_budget(); retention();
    printf("JSON native core: %zu checks passed\n", checks);
    return 0;
}
