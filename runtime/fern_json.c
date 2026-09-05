/** Immutable, bounded JSON DOM. See docs/JSON_NATIVE_CORE.md for the ABI profile. */
#define _GNU_SOURCE
#define _DARWIN_C_SOURCE
#include "fern_runtime.h"
#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif

#define JSON_INPUT_MAX ((size_t)1048576)
#define JSON_DEPTH_MAX ((size_t)128)
#define JSON_NODES_MAX ((size_t)100000)
#define JSON_ALLOC_MAX ((size_t)33554432)
#define JSON_OUTPUT_MAX ((size_t)16777216)

_Static_assert(sizeof(FernJsonMember) == 16, "JSON member ABI requires two 64-bit pointers");
_Static_assert(offsetof(FernJsonMember, value) == 8, "JSON member value ABI offset");

typedef enum { J_NULL, J_BOOL, J_NUMBER, J_STRING, J_ARRAY, J_OBJECT } JsonKind;

/* Private fixed fields deliberately avoid publishing a source-language layout. */
struct FernJsonValue {
    JsonKind kind;
    bool boolean;
    size_t length;
    char* text;
    struct FernJsonValue** children;
    size_t* index;
    size_t offset;
    size_t height;
    size_t nodes;
    size_t encoded;
};

struct FernJsonError { int64_t code; int64_t offset; };

typedef struct {
    const unsigned char* text;
    size_t length;
    size_t position;
    size_t nodes;
    size_t allocated;
    size_t work;
    int code;
    size_t error_offset;
} JsonParser;

/** Record the first error; @param p parser; @param code stable code; @param at byte offset. */
static void json_fail(JsonParser* p, int code, size_t at) {
    assert(p != NULL);
    assert(at <= p->length);
    if (p->code == 0) { p->code = code; p->error_offset = at; }
}

/** Charge bounded work before execution. @param p parser; @param n units; @return available. */
static bool json_work(JsonParser* p, size_t n) {
    assert(p != NULL);
    assert(p->position <= p->length);
    if (n > p->work) { json_fail(p, 4, p->position); return false; }
    p->work -= n;
    return p->code == 0;
}

/** Allocate scanned GC storage. @param p parser; @param n bytes; @return storage or error state. */
static void* json_allocate(JsonParser* p, size_t n) {
    assert(p != NULL);
    assert(p->allocated <= JSON_ALLOC_MAX);
    if (p->code != 0) return NULL;
    if (n == 0) n = 1;
    if (n > JSON_ALLOC_MAX - p->allocated) { json_fail(p, 4, p->position); return NULL; }
    p->allocated += n;
    void* memory = fern_alloc(n);
    memset(memory, 0, n);
    return memory;
}

/** Construct a failure Result. @param code stable code; @param offset input offset or -1; @return Err. */
static int64_t json_error(int code, int64_t offset) {
    assert(code >= 1 && code <= 11);
    assert(offset >= -1);
    FernJsonError* error = fern_alloc(sizeof(*error));
    error->code = code;
    error->offset = offset;
    return fern_result_err((int64_t)(intptr_t)error);
}

/** Allocate one budgeted node. @param p parser; @param kind validated kind; @return node or error state. */
static FernJsonValue* json_node(JsonParser* p, JsonKind kind) {
    assert(p != NULL);
    assert(kind <= J_OBJECT);
    if (p->nodes == JSON_NODES_MAX) { json_fail(p, 4, p->position); return NULL; }
    FernJsonValue* value = json_allocate(p, sizeof(*value));
    if (!value) return NULL;
    p->nodes++;
    value->kind = kind;
    value->offset = p->position;
    value->height = 1;
    value->nodes = 1;
    return value;
}

/** Skip exactly JSON whitespace. @param p parser. */
static void json_space(JsonParser* p) {
    assert(p != NULL);
    assert(p->position <= p->length);
    for (; p->position < p->length; p->position++) {
        unsigned char c = p->text[p->position];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    }
}

/** Read a four-hex-digit escape. @param p parser; @param scalar output; @return valid. */
static bool json_hex(JsonParser* p, unsigned* scalar) {
    assert(p != NULL);
    assert(scalar != NULL);
    *scalar = 0;
    for (size_t i = 0; i < 4; i++) {
        if (p->position == p->length) { json_fail(p, 1, p->position); return false; }
        unsigned c = p->text[p->position++];
        unsigned digit = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
        if (digit == 16) { json_fail(p, 1, p->position - 1); return false; }
        *scalar = *scalar * 16 + digit;
    }
    return true;
}

/** Encode a Unicode scalar. @param c scalar; @param out four-byte buffer; @return byte count. */
static size_t json_utf8_encode(unsigned c, unsigned char out[4]) {
    assert(out != NULL);
    assert(c <= 0x10ffff && !(c >= 0xd800 && c <= 0xdfff));
    if (c < 0x80) { out[0] = (unsigned char)c; return 1; }
    size_t n = c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
    for (size_t i = n - 1; i > 0; i--) { out[i] = 0x80 | (c & 63); c >>= 6; }
    out[0] = (n == 2 ? 0xc0 : n == 3 ? 0xe0 : 0xf0) | c;
    return n;
}

/** Decode a JSON escape. @param p parser after backslash; @param out bytes; @param at slash offset; @return length. */
static size_t json_escape(JsonParser* p, unsigned char out[4], size_t at) {
    assert(p != NULL);
    assert(out != NULL);
    if (p->position == p->length) { json_fail(p, 1, p->position); return 0; }
    unsigned c = p->text[p->position++];
    const char* names = "\"\\/bfnrt";
    const char* bytes = "\"\\/\b\f\n\r\t";
    for (size_t i = 0; i < 8; i++) if (c == (unsigned)names[i]) { out[0] = bytes[i]; return 1; }
    if (c != 'u') { json_fail(p, 1, p->position - 1); return 0; }
    if (!json_hex(p, &c)) return 0;
    if (c >= 0xd800 && c <= 0xdbff) {
        if (p->length - p->position < 6 || p->text[p->position] != '\\' || p->text[p->position+1] != 'u') {
            json_fail(p, 2, at); return 0;
        }
        p->position += 2;
        unsigned low;
        if (!json_hex(p, &low)) return 0;
        if (low < 0xdc00 || low > 0xdfff) { json_fail(p, 2, at); return 0; }
        c = 0x10000 + (c - 0xd800) * 1024 + low - 0xdc00;
    } else if (c >= 0xdc00 && c <= 0xdfff) { json_fail(p, 2, at); return 0; }
    return json_utf8_encode(c, out);
}

/** Validate one raw UTF-8 scalar. @param p parser; @param out bytes; @return length or error state. */
static size_t json_raw_scalar(JsonParser* p, unsigned char out[4]) {
    assert(p != NULL);
    assert(p->position < p->length);
    size_t at = p->position;
    unsigned c = p->text[at];
    size_t n = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 :
        c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
    if (n == 0 || n > p->length - at) { json_fail(p, 2, at); return 0; }
    unsigned scalar = c & (n == 1 ? 127 : n == 2 ? 31 : n == 3 ? 15 : 7);
    for (size_t i = 1; i < n; i++) {
        unsigned byte = p->text[at+i];
        if ((byte & 0xc0) != 0x80) { json_fail(p, 2, at); return 0; }
        scalar = scalar * 64 + (byte & 63);
    }
    if ((n == 2 && scalar < 0x80) || (n == 3 && scalar < 0x800) ||
        (n == 4 && scalar < 0x10000) || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) {
        json_fail(p, 2, at); return 0;
    }
    memcpy(out, p->text + at, n);
    p->position += n;
    return n;
}

/** Decode one string unit. @param p parser; @param out bytes; @return length or error state. */
static size_t json_string_unit(JsonParser* p, unsigned char out[4]) {
    assert(p != NULL);
    assert(p->position < p->length);
    size_t at = p->position;
    if (!json_work(p, 1)) return 0;
    if (p->text[at] < 0x20) { json_fail(p, 1, at); return 0; }
    if (p->text[at] == '\\') { p->position++; return json_escape(p, out, at); }
    return json_raw_scalar(p, out);
}

/** Count encoded bytes for one decoded byte. @param c byte; @return escaped size. */
static size_t json_escape_size(unsigned char c) {
    assert(JSON_OUTPUT_MAX >= 6);
    assert(sizeof(c) == 1);
    if (c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') return 2;
    return c < 0x20 ? 6 : 1;
}

/** Two bounded passes allocate only decoded size, not remaining input per string. @param p parser; @return node. */
static FernJsonValue* json_string(JsonParser* p) {
    assert(p != NULL);
    assert(p->position < p->length && p->text[p->position] == '"');
    FernJsonValue* value = json_node(p, J_STRING);
    if (!value) return NULL;
    size_t start = ++p->position;
    unsigned char bytes[4];
    for (size_t units = 0; units <= p->length && p->code == 0; units++) {
        if (p->position == p->length) { json_fail(p, 1, p->position); break; }
        if (p->text[p->position] == '"') break;
        value->length += json_string_unit(p, bytes);
    }
    if (p->code != 0) return NULL;
    size_t end = p->position++;
    value->text = json_allocate(p, value->length + 1);
    if (!value->text) return NULL;
    p->position = start;
    size_t written = 0;
    value->encoded = 2;
    for (size_t units = 0; p->position < end && units <= p->length; units++) {
        size_t n = json_string_unit(p, bytes);
        if (p->code != 0) return NULL;
        assert(n <= value->length - written);
        memcpy(value->text + written, bytes, n);
        for (size_t i = 0; i < n; i++) value->encoded += json_escape_size(bytes[i]);
        written += n;
    }
    assert(written == value->length);
    p->position = end + 1;
    return value;
}

/** Test the JSON digit class. @param c byte; @return digit membership. */
static bool json_digit(unsigned char c) {
    assert('9' - '0' == 9);
    assert(sizeof(c) == 1);
    return c >= '0' && c <= '9';
}

/** Consume at least one decimal digit. @param p parser; @return nonempty run. */
static bool json_digits(JsonParser* p) {
    assert(p != NULL);
    assert(p->position <= p->length);
    size_t start = p->position;
    for (; p->position < p->length && json_digit(p->text[p->position]); p->position++) {}
    if (p->position == start) { json_fail(p, 1, start); return false; }
    return true;
}

/** Validate and preserve one number lexeme. @param p parser; @return number or error state. */
static FernJsonValue* json_number(JsonParser* p) {
    assert(p != NULL);
    assert(p->position < p->length);
    FernJsonValue* value = json_node(p, J_NUMBER);
    if (!value) return NULL;
    size_t start = p->position;
    if (p->text[p->position] == '-') p->position++;
    if (p->position < p->length && p->text[p->position] == '0') p->position++;
    else if (!json_digits(p)) return NULL;
    if (p->position < p->length && p->text[p->position] == '.') {
        p->position++;
        if (!json_digits(p)) return NULL;
    }
    if (p->position < p->length && (p->text[p->position] == 'e' || p->text[p->position] == 'E')) {
        p->position++;
        if (p->position < p->length && (p->text[p->position] == '+' || p->text[p->position] == '-')) p->position++;
        if (!json_digits(p)) return NULL;
    }
    value->length = p->position - start;
    value->text = json_allocate(p, value->length + 1);
    if (!value->text) return NULL;
    memcpy(value->text, p->text + start, value->length);
    value->encoded = value->length;
    return value;
}

/** Append a child with geometric budgeted growth. @param p parser; @param v container; @param cap capacity; @param child node; @return success. */
static bool json_append(JsonParser* p, FernJsonValue* v, size_t* cap, FernJsonValue* child) {
    assert(p != NULL && v != NULL);
    assert(cap != NULL && child != NULL);
    if (v->length == *cap) {
        size_t next = *cap == 0 ? 4 : *cap * 2;
        assert(next <= JSON_NODES_MAX * 2);
        FernJsonValue** storage = json_allocate(p, next * sizeof(*storage));
        if (!storage) return false;
        if (v->length) memcpy(storage, v->children, v->length * sizeof(*storage));
        if (!json_work(p, v->length)) return false;
        v->children = storage;
        *cap = next;
    }
    v->children[v->length++] = child;
    return true;
}

/** Compare decoded object names with byte work charging. @param p parser; @param a key; @param b key; @return lexical order. */
static int json_key_compare(JsonParser* p, const FernJsonValue* a, const FernJsonValue* b) {
    assert(a != NULL && a->kind == J_STRING);
    assert(b != NULL && b->kind == J_STRING);
    size_t length = a->length < b->length ? a->length : b->length;
    for (size_t i = 0; i < length; i++) {
        if (!json_work(p, 1)) return 0;
        unsigned char x = (unsigned char)a->text[i], y = (unsigned char)b->text[i];
        if (x != y) return x < y ? -1 : 1;
    }
    return a->length < b->length ? -1 : a->length != b->length;
}

/** Merge one bounded pair of index runs. @param p parser; @param v object; @param tmp scratch; @param base run start; @param width run width. */
static void json_index_merge(JsonParser* p, FernJsonValue* v, size_t* tmp, size_t base, size_t width) {
    assert(p != NULL && v != NULL);
    assert(base < v->length && width > 0);
    size_t middle = base + width < v->length ? base + width : v->length;
    size_t end = middle + width < v->length ? middle + width : v->length;
    size_t a = base, b = middle;
    for (size_t i = base; i < end && p->code == 0; i++) {
        if (!json_work(p, 1)) return;
        bool left = b == end || (a < middle && json_key_compare(p, v->children[v->index[a]*2], v->children[v->index[b]*2]) <= 0);
        tmp[i] = v->index[left ? a++ : b++];
    }
}

/** Build deterministic lookup index and reject decoded duplicate keys. @param p parser; @param v object; @return success. */
static bool json_object_index(JsonParser* p, FernJsonValue* v) {
    assert(p != NULL);
    assert(v != NULL && v->kind == J_OBJECT);
    if (v->length == 0) return true;
    v->index = json_allocate(p, v->length * sizeof(size_t));
    size_t* temporary = json_allocate(p, v->length * sizeof(size_t));
    if (!v->index || !temporary) return false;
    for (size_t i = 0; i < v->length; i++) v->index[i] = i;
    for (size_t width = 1; width < v->length && p->code == 0; width *= 2) {
        for (size_t base = 0; base < v->length && p->code == 0; base += width * 2) json_index_merge(p, v, temporary, base, width);
        size_t* swap = v->index; v->index = temporary; temporary = swap;
    }
    for (size_t i = 1; i < v->length && p->code == 0; i++) {
        const FernJsonValue* a = v->children[v->index[i-1]*2];
        const FernJsonValue* b = v->children[v->index[i]*2];
        if (json_key_compare(p, a, b) == 0 && p->code == 0) json_fail(p, 3, b->offset);
    }
    return p->code == 0;
}

static FernJsonValue* json_value(JsonParser* p, size_t depth);

/** Parse a member's key/colon prefix. @param p parser; @param v object; @param cap capacity; @return success. */
static bool json_member_key(JsonParser* p, FernJsonValue* v, size_t* cap) {
    assert(p != NULL);
    assert(v != NULL && v->kind == J_OBJECT);
    if (p->position == p->length || p->text[p->position] != '"') { json_fail(p, 1, p->position); return false; }
    FernJsonValue* key = json_string(p);
    if (!key || !json_append(p, v, cap, key)) return false;
    json_space(p);
    if (p->position == p->length || p->text[p->position] != ':') { json_fail(p, 1, p->position); return false; }
    p->position++;
    return true;
}

/** Compute immutable subtree metadata. @param p parser; @param v container; @return success. */
static bool json_seal(JsonParser* p, FernJsonValue* v) {
    assert(p != NULL);
    assert(v != NULL && (v->kind == J_ARRAY || v->kind == J_OBJECT));
    size_t count = v->length;
    v->encoded = 2 + (count ? count - 1 : 0);
    for (size_t i = 0; i < count; i++) {
        const FernJsonValue* child = v->children[i];
        if (child->encoded > JSON_OUTPUT_MAX - v->encoded) { json_fail(p, 4, p->position); return false; }
        v->encoded += child->encoded;
        if (child->nodes > JSON_NODES_MAX - v->nodes || child->height >= JSON_DEPTH_MAX) { json_fail(p, 4, p->position); return false; }
        v->nodes += child->nodes;
        if (child->height >= v->height) v->height = child->height + 1;
    }
    if (v->kind == J_OBJECT) { v->length /= 2; return json_object_index(p, v); }
    return true;
}

/** Parse a container with bounded recursive children. @param p parser; @param depth root-based depth; @param object kind; @return value. */
static FernJsonValue* json_container(JsonParser* p, size_t depth, bool object) {
    assert(p != NULL);
    assert(depth <= JSON_DEPTH_MAX);
    FernJsonValue* value = json_node(p, object ? J_OBJECT : J_ARRAY);
    if (!value) return NULL;
    p->position++;
    json_space(p);
    unsigned char closing = object ? '}' : ']';
    size_t capacity = 0;
    if (p->position < p->length && p->text[p->position] == closing) {
        p->position++; value->encoded = 2; return value;
    }
    for (size_t count = 0; count < JSON_NODES_MAX && p->code == 0; count++) {
        if (object && !json_member_key(p, value, &capacity)) return NULL;
        FernJsonValue* child = json_value(p, depth + 1);
        if (!child || !json_append(p, value, &capacity, child)) return NULL;
        json_space(p);
        if (p->position == p->length) { json_fail(p, 1, p->position); return NULL; }
        unsigned char separator = p->text[p->position++];
        if (separator == closing) return json_seal(p, value) ? value : NULL;
        if (separator != ',') { json_fail(p, 1, p->position - 1); return NULL; }
        json_space(p);
    }
    json_fail(p, 4, p->position);
    return NULL;
}

/** Parse one literal token. @param p parser; @param spelling expected token; @param kind tag; @return value. */
static FernJsonValue* json_literal(JsonParser* p, const char* spelling, JsonKind kind) {
    assert(p != NULL);
    assert(spelling != NULL);
    size_t length = strlen(spelling);
    for (size_t i = 0; i < length; i++) {
        if (p->position + i == p->length || p->text[p->position+i] != (unsigned char)spelling[i]) {
            json_fail(p, 1, p->position + i); return NULL;
        }
    }
    FernJsonValue* value = json_node(p, kind);
    if (!value) return NULL;
    value->boolean = spelling[0] == 't';
    value->encoded = length;
    p->position += length;
    return value;
}

/** Dispatch one value after checking depth. @param p parser; @param depth root-based depth; @return value or first error. */
static FernJsonValue* json_value(JsonParser* p, size_t depth) {
    assert(p != NULL);
    assert(p->position <= p->length);
    json_space(p);
    if (!json_work(p, 1)) return NULL;
    if (depth > JSON_DEPTH_MAX) { json_fail(p, 4, p->position); return NULL; }
    if (p->position == p->length) { json_fail(p, 1, p->position); return NULL; }
    unsigned char c = p->text[p->position];
    if (c == '[' || c == '{') return json_container(p, depth, c == '{');
    if (c == '"') return json_string(p);
    if (c == '-' || json_digit(c)) return json_number(p);
    if (c == 'n') return json_literal(p, "null", J_NULL);
    if (c == 't') return json_literal(p, "true", J_BOOL);
    if (c == 'f') return json_literal(p, "false", J_BOOL);
    json_fail(p, 1, p->position);
    return NULL;
}

/** Parse a bounded NUL-terminated document. @param text non-NULL input; @return Result(Value*, Error*). */
int64_t fern_json_value_parse(const char* text) {
    assert(text != NULL);
    assert(JSON_INPUT_MAX < JSON_ALLOC_MAX);
    size_t length = strnlen(text, JSON_INPUT_MAX + 1);
    if (length > JSON_INPUT_MAX) return json_error(4, JSON_INPUT_MAX);
    JsonParser parser = {.text = (const unsigned char*)text, .length = length,
        .work = 8 * length + 64 * JSON_NODES_MAX};
    /* Reserve the linear scan/copy bound; repeated comparisons charge separately. */
    (void)json_work(&parser, 8 * length);
    if (length >= 3 && memcmp(text, "\357\273\277", 3) == 0) parser.position = 3;
    FernJsonValue* value = json_value(&parser, 1);
    json_space(&parser);
    if (!parser.code && parser.position != length) json_fail(&parser, 1, parser.position);
    if (parser.code) return json_error(parser.code, (int64_t)parser.error_offset);
    assert(value != NULL);
    return fern_result_ok((int64_t)(intptr_t)value);
}

typedef struct { char* data; size_t position; size_t capacity; } JsonWriter;

/** Append known bounded bytes. @param out bounded destination; @param bytes source; @param length count. */
static void json_write(JsonWriter* out, const char* bytes, size_t length) {
    assert(out != NULL && bytes != NULL);
    assert(out->position <= out->capacity && length <= out->capacity - out->position);
    memcpy(out->data + out->position, bytes, length);
    out->position += length;
}

/** Encode a decoded JSON String. @param v string; @param out bounded output. */
static void json_write_string(const FernJsonValue* v, JsonWriter* out) {
    assert(v != NULL && v->kind == J_STRING);
    assert(v->length <= JSON_INPUT_MAX);
    json_write(out, "\"", 1);
    for (size_t i = 0; i < v->length; i++) {
        unsigned char c = (unsigned char)v->text[i];
        const char* escape = c == '"' ? "\\\"" : c == '\\' ? "\\\\" :
            c == '\b' ? "\\b" : c == '\f' ? "\\f" : c == '\n' ? "\\n" : c == '\r' ? "\\r" : c == '\t' ? "\\t" : NULL;
        if (escape) json_write(out, escape, 2);
        else if (c < 0x20) {
            const char* hex = "0123456789abcdef";
            char bytes[] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            json_write(out, bytes, 6);
        } else json_write(out, v->text + i, 1);
    }
    json_write(out, "\"", 1);
}

/** Encode an immutable validated subtree. @param v value; @param out bounded output; @param depth bounded recursion. */
static void json_write_value(const FernJsonValue* v, JsonWriter* out, size_t depth) {
    assert(v != NULL);
    assert(depth <= JSON_DEPTH_MAX);
    if (v->kind == J_NULL) json_write(out, "null", 4);
    else if (v->kind == J_BOOL) json_write(out, v->boolean ? "true" : "false", v->boolean ? 4 : 5);
    else if (v->kind == J_NUMBER) json_write(out, v->text, v->length);
    else if (v->kind == J_STRING) json_write_string(v, out);
    else {
        bool object = v->kind == J_OBJECT;
        json_write(out, object ? "{" : "[", 1);
        for (size_t i = 0; i < v->length; i++) {
            if (i) json_write(out, ",", 1);
            if (object) {
                json_write_string(v->children[i*2], out);
                json_write(out, ":", 1);
            }
            json_write_value(v->children[object ? i*2+1 : i], out, depth + 1);
        }
        json_write(out, object ? "}" : "]", 1);
    }
}

/** Compact insertion-order encoding. @param value valid opaque value; @return Result(String, Error*). */
int64_t fern_json_value_stringify(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->height <= JSON_DEPTH_MAX && value->nodes <= JSON_NODES_MAX);
    if (value->encoded > JSON_OUTPUT_MAX) return json_error(4, -1);
    char* output = fern_alloc(value->encoded + 1);
    JsonWriter writer = {.data = output, .capacity = value->encoded};
    json_write_value(value, &writer, 1);
    assert(writer.position == value->encoded);
    output[writer.position] = 0;
    return fern_result_ok((int64_t)(intptr_t)output);
}

/** Inspect JSON null. @param value valid opaque value; @return zero or one. */
int64_t fern_json_value_is_null(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->kind <= J_OBJECT);
    return value->kind == J_NULL;
}

/** Binary-search a decoded key. @param value object; @param key NUL-terminated key; @return Result(Value*, Error*). */
int64_t fern_json_value_get(const FernJsonValue* value, const char* key) {
    assert(value != NULL);
    assert(key != NULL);
    if (value->kind != J_OBJECT) return json_error(5, -1);
    size_t length = strnlen(key, JSON_INPUT_MAX + 1);
    if (length > JSON_INPUT_MAX) return json_error(4, -1);
    size_t low = 0, high = value->length;
    for (size_t steps = 0; low < high && steps < 32; steps++) {
        size_t middle = low + (high - low) / 2;
        size_t slot = value->index[middle];
        const FernJsonValue* name = value->children[slot*2];
        size_t common = length < name->length ? length : name->length;
        int order = memcmp(key, name->text, common);
        if (order == 0) order = length < name->length ? -1 : length != name->length;
        if (order == 0) return fern_result_ok((int64_t)(intptr_t)value->children[slot*2+1]);
        if (order < 0) high = middle;
        else low = middle + 1;
    }
    return json_error(6, -1);
}

/** Bounds-checked array access. @param value opaque value; @param index signed index; @return Result(Value*, Error*). */
int64_t fern_json_value_at(const FernJsonValue* value, int64_t index) {
    assert(value != NULL);
    assert(value->kind <= J_OBJECT);
    if (value->kind != J_ARRAY) return json_error(5, -1);
    if (index < 0 || (uint64_t)index >= value->length) return json_error(7, -1);
    return fern_result_ok((int64_t)(intptr_t)value->children[index]);
}

/** Count array elements or object members. @param value opaque value; @return Result(Int, Error*). */
int64_t fern_json_value_length(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->length <= JSON_INPUT_MAX);
    if (value->kind != J_ARRAY && value->kind != J_OBJECT) return json_error(5, -1);
    return fern_result_ok((int64_t)value->length);
}

/** Extract a Boolean. @param value opaque value; @return Result(Bool, Error*). */
int64_t fern_json_value_as_bool(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->kind <= J_OBJECT);
    if (value->kind != J_BOOL) return json_error(5, -1);
    return fern_result_ok(value->boolean);
}

/** Expose a NUL-free decoded String. @param value opaque value; @return Result(String, Error*). */
int64_t fern_json_value_as_string(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->kind <= J_OBJECT);
    if (value->kind != J_STRING) return json_error(5, -1);
    if (memchr(value->text, 0, value->length)) return json_error(10, -1);
    return fern_result_ok((int64_t)(intptr_t)value->text);
}

/** Expose exact validated number spelling. @param value opaque value; @return Result(String, Error*). */
int64_t fern_json_value_number_text(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->kind <= J_OBJECT);
    if (value->kind != J_NUMBER) return json_error(5, -1);
    return fern_result_ok((int64_t)(intptr_t)value->text);
}

typedef struct {
    size_t first;
    size_t last;
    size_t digits;
    size_t fraction;
    int64_t exponent;
    bool negative;
} JsonDecimal;

/** Read a saturated exponent without magnitude-proportional loops. @param v number; @param at exponent start; @return signed exponent. */
static int64_t json_exponent(const FernJsonValue* v, size_t at) {
    assert(v != NULL && v->kind == J_NUMBER);
    assert(at <= v->length);
    if (at == v->length) return 0;
    bool negative = v->text[at] == '-';
    if (v->text[at] == '-' || v->text[at] == '+') at++;
    int64_t exponent = 0;
    const int64_t cap = (int64_t)JSON_INPUT_MAX + 100;
    for (size_t i = at; i < v->length; i++) {
        assert(json_digit((unsigned char)v->text[i]));
        if (exponent < cap) exponent = exponent * 10 + v->text[i] - '0';
        if (exponent > cap) exponent = cap;
    }
    return negative ? -exponent : exponent;
}

/** Analyze nonzero significant decimal positions exactly. @param v number; @return decimal shape. */
static JsonDecimal json_decimal(const FernJsonValue* v) {
    assert(v != NULL && v->kind == J_NUMBER);
    assert(v->length <= JSON_INPUT_MAX);
    JsonDecimal decimal = {.first = SIZE_MAX, .negative = v->text[0] == '-'};
    bool fraction = false;
    for (size_t i = decimal.negative ? 1 : 0; i < v->length; i++) {
        unsigned char c = (unsigned char)v->text[i];
        if (c == 'e' || c == 'E') { decimal.exponent = json_exponent(v, i + 1); break; }
        if (c == '.') { fraction = true; continue; }
        if (c != '0') {
            if (decimal.first == SIZE_MAX) decimal.first = decimal.digits;
            decimal.last = decimal.digits;
        }
        decimal.digits++;
        if (fraction) decimal.fraction++;
    }
    return decimal;
}

/** Accumulate at most nineteen significant digits. @param v number; @param d decimal shape; @return unsigned significand. */
static uint64_t json_significand(const FernJsonValue* v, JsonDecimal d) {
    assert(v != NULL);
    assert(d.last - d.first < 19);
    size_t digit = 0;
    uint64_t result = 0;
    for (size_t i = d.negative ? 1 : 0; i < v->length && digit <= d.last; i++) {
        if (v->text[i] == '.') continue;
        if (digit >= d.first) result = result * 10 + (unsigned)(v->text[i] - '0');
        digit++;
    }
    return result;
}

/** Convert mathematically integral decimals to signed64 without rounding. @param value opaque value; @return Result(Int, Error*). */
int64_t fern_json_value_as_int(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->kind <= J_OBJECT);
    if (value->kind != J_NUMBER) return json_error(5, -1);
    JsonDecimal d = json_decimal(value);
    if (d.first == SIZE_MAX) return fern_result_ok(0);
    int64_t scale = d.exponent - (int64_t)d.fraction + (int64_t)(d.digits - d.last - 1);
    if (scale < 0) return json_error(9, -1);
    size_t significant = d.last - d.first + 1;
    if (significant > 19 || scale > 19 - (int64_t)significant) return json_error(8, -1);
    uint64_t magnitude = json_significand(value, d);
    for (int64_t i = 0; i < scale; i++) magnitude *= 10;
    uint64_t maximum = (uint64_t)INT64_MAX + (d.negative ? 1 : 0);
    if (magnitude > maximum) return json_error(8, -1);
    int64_t result = magnitude == (uint64_t)INT64_MAX + 1 ? INT64_MIN :
        d.negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return fern_result_ok(result);
}

/** Convert using a private C locale, preserving binary64 Result payload bits. @param value opaque value; @return Result(Float bits, Error*). */
int64_t fern_json_value_as_float(const FernJsonValue* value) {
    assert(value != NULL);
    assert(sizeof(double) == sizeof(int64_t));
    if (value->kind != J_NUMBER) return json_error(5, -1);
    locale_t locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (!locale) return json_error(4, -1);
    char* end = NULL;
    double converted = strtod_l(value->text, &end, locale);
    freelocale(locale);
    assert(end == value->text + value->length);
    if (!isfinite(converted) || (converted == 0.0 && json_decimal(value).first != SIZE_MAX)) return json_error(8, -1);
    int64_t bits;
    memcpy(&bits, &converted, sizeof(bits));
    return fern_result_ok(bits);
}

/** Read a stable error code. @param error valid opaque error; @return code1..11. */
int64_t fern_json_value_error_code(const FernJsonError* error) {
    assert(error != NULL);
    assert(error->code >= 1 && error->code <= 11);
    return error->code;
}

/** Read the original input byte offset. @param error valid opaque error; @return offset or -1. */
int64_t fern_json_value_error_offset(const FernJsonError* error) {
    assert(error != NULL);
    assert(error->offset >= -1);
    return error->offset;
}

/** Read a stable nonallocating message. @param error valid opaque error; @return static NUL-terminated message. */
const char* fern_json_value_error_message(const FernJsonError* error) {
    assert(error != NULL);
    assert(error->code >= 1 && error->code <= 11);
    static const char* messages[] = {"", "invalid JSON syntax", "invalid JSON Unicode", "duplicate JSON object key",
        "JSON resource limit exceeded", "JSON value has wrong type", "JSON object key not found",
        "JSON array index out of bounds", "JSON number out of range", "JSON number is not an integer",
        "JSON string contains NUL", "JSON number is not finite"};
    return messages[error->code];
}

/** Initialize bounded builder state. @param bytes newly scanned bytes; @return local budgets. */
static JsonParser json_builder(size_t bytes) {
    assert(bytes <= JSON_OUTPUT_MAX);
    assert(JSON_NODES_MAX <= SIZE_MAX / 64);
    JsonParser parser = {.work = 8 * bytes + 64 * JSON_NODES_MAX};
    (void)json_work(&parser, 8 * bytes);
    return parser;
}

/** Publish only a valid sealed value. @param p builder; @param value candidate; @return ordinary Result. */
static int64_t json_built(JsonParser* p, FernJsonValue* value) {
    assert(p != NULL);
    assert(value != NULL || p->code != 0);
    if (p->code != 0) return json_error(p->code, -1);
    if (value->height > JSON_DEPTH_MAX || value->nodes > JSON_NODES_MAX || value->encoded > JSON_OUTPUT_MAX) return json_error(4, -1);
    return fern_result_ok((int64_t)(intptr_t)value);
}

/** Construct a JSON null. @return runtime-owned immutable value. */
FernJsonValue* fern_json_value_null(void) {
    JsonParser parser = json_builder(0);
    FernJsonValue* value = json_node(&parser, J_NULL);
    assert(value != NULL);
    assert(value->nodes == 1);
    value->encoded = 4;
    return value;
}

/** Construct a JSON Boolean. @param boolean zero or one; @return immutable value. */
FernJsonValue* fern_json_value_from_bool(int64_t boolean) {
    assert(boolean == 0 || boolean == 1);
    JsonParser parser = json_builder(0);
    FernJsonValue* value = json_node(&parser, J_BOOL);
    assert(value != NULL);
    value->boolean = boolean != 0;
    value->encoded = boolean ? 4 : 5;
    return value;
}

/** Parse exactly one number token for builders. @param text NUL-terminated number; @return Result(Value*, Error*). */
int64_t fern_json_value_from_number_text(const char* text) {
    assert(text != NULL);
    assert(JSON_INPUT_MAX < JSON_ALLOC_MAX);
    size_t length = strnlen(text, JSON_INPUT_MAX + 1);
    if (length > JSON_INPUT_MAX) return json_error(4, -1);
    if (!length || (text[0] != '-' && !json_digit((unsigned char)text[0]))) return json_error(1, -1);
    JsonParser parser = json_builder(length);
    parser.text = (const unsigned char*)text;
    parser.length = length;
    FernJsonValue* value = json_number(&parser);
    if (!parser.code && parser.position != length) json_fail(&parser, 1, parser.position);
    return json_built(&parser, value);
}

/** Construct an exact signed64 JSON number. @param number signed integer; @return immutable value. */
FernJsonValue* fern_json_value_from_int(int64_t number) {
    char text[32];
    int length = snprintf(text, sizeof(text), "%lld", (long long)number);
    assert(length > 0 && (size_t)length < sizeof(text));
    (void)length;
    int64_t result = fern_json_value_from_number_text(text);
    assert(fern_result_is_ok(result));
    return (FernJsonValue*)(intptr_t)fern_result_unwrap(result);
}

/** Format binary64 in a temporary thread-local C locale. @param number finite Float; @param text bounded output; @return success. */
static bool json_float_text(double number, char text[64]) {
    assert(isfinite(number));
    assert(text != NULL);
    locale_t locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (!locale) return false;
    locale_t previous = uselocale(locale);
    if (!previous) { freelocale(locale); return false; }
    int length = snprintf(text, 64, "%.17g", number);
    (void)uselocale(previous);
    freelocale(locale);
    return length > 0 && length < 64;
}

/** Construct a finite binary64 JSON number. @param number Float; @return Result(Value*, Error*). */
int64_t fern_json_value_from_float(double number) {
    assert(sizeof(number) == 8);
    assert(JSON_INPUT_MAX >= 64);
    if (!isfinite(number)) return json_error(11, -1);
    char text[64];
    if (!json_float_text(number, text)) return json_error(4, -1);
    return fern_json_value_from_number_text(text);
}

/** Validate and copy decoded UTF-8, including legal JSON controls. @param p budget; @param text bytes; @param length byte count; @return String node. */
static FernJsonValue* json_text_node(JsonParser* p, const char* text, size_t length) {
    assert(p != NULL && text != NULL);
    assert(length <= JSON_INPUT_MAX);
    JsonParser scan = {.text = (const unsigned char*)text, .length = length};
    unsigned char bytes[4];
    for (size_t i = 0; scan.position < length && i < length; i++) {
        (void)json_raw_scalar(&scan, bytes);
        if (scan.code) { json_fail(p, scan.code, p->position); return NULL; }
    }
    FernJsonValue* value = json_node(p, J_STRING);
    if (!value) return NULL;
    value->length = length;
    value->text = json_allocate(p, length + 1);
    if (!value->text) return NULL;
    memcpy(value->text, text, length);
    value->encoded = 2;
    for (size_t i = 0; i < length; i++) value->encoded += json_escape_size((unsigned char)text[i]);
    return value;
}

/** Construct a validated JSON String. @param text NUL-terminated text; @return Result(Value*, Error*). */
int64_t fern_json_value_from_string(const char* text) {
    assert(text != NULL);
    assert(JSON_INPUT_MAX < JSON_OUTPUT_MAX);
    size_t length = strnlen(text, JSON_INPUT_MAX + 1);
    if (length > JSON_INPUT_MAX) return json_error(4, -1);
    JsonParser parser = json_builder(length);
    return json_built(&parser, json_text_node(&parser, text, length));
}

/** Validate list dimensions before accessing data. @param list native list; @param maximum length limit; @return permitted dimensions. */
static bool json_list_size(const FernList* list, size_t maximum) {
    assert(list != NULL);
    assert(maximum <= JSON_NODES_MAX);
    return list->len >= 0 && (uint64_t)list->len <= maximum && list->cap >= list->len;
}

/** Copy an immutable child vector. @param p builder; @param list children; @return array node. */
static FernJsonValue* json_array_node(JsonParser* p, const FernList* list) {
    assert(p != NULL && list != NULL);
    assert(json_list_size(list, JSON_NODES_MAX - 1));
    FernJsonValue* value = json_node(p, J_ARRAY);
    if (!value) return NULL;
    value->length = (size_t)list->len;
    value->children = json_allocate(p, value->length * sizeof(*value->children));
    if (!value->children) return NULL;
    for (size_t i = 0; i < value->length; i++) {
        value->children[i] = (FernJsonValue*)(intptr_t)list->data[i];
        assert(value->children[i] != NULL);
    }
    return json_seal(p, value) ? value : NULL;
}

/** Copy source array storage, retaining immutable children. @param list valid Value-pointer list; @return Result(Value*, Error*). */
int64_t fern_json_value_from_array(const FernList* list) {
    assert(list != NULL);
    assert(JSON_NODES_MAX > 1);
    if (!json_list_size(list, JSON_NODES_MAX - 1)) return json_error(4, -1);
    JsonParser parser = json_builder(0);
    return json_built(&parser, json_array_node(&parser, list));
}

/** Bound aggregate copied key text before object allocations. @param keys String-pointer list; @param total output; @return bytes fit profile. */
static bool json_key_bytes(const FernList* keys, size_t* total) {
    assert(keys != NULL);
    assert(total != NULL);
    *total = 0;
    for (int64_t i = 0; i < keys->len; i++) {
        const char* text = (const char*)(intptr_t)keys->data[i];
        assert(text != NULL);
        size_t length = strnlen(text, JSON_INPUT_MAX + 1);
        if (length > JSON_INPUT_MAX || length > JSON_OUTPUT_MAX - *total) return false;
        *total += length;
    }
    return true;
}

/** Build checked object fields in source order. @param p builder; @param keys String list; @param values Value list; @return sealed object. */
static FernJsonValue* json_object_node(JsonParser* p, const FernList* keys, const FernList* values) {
    assert(p != NULL && keys != NULL && values != NULL);
    assert(keys->len == values->len);
    FernJsonValue* value = json_node(p, J_OBJECT);
    if (!value) return NULL;
    value->length = (size_t)keys->len * 2;
    value->children = json_allocate(p, value->length * sizeof(*value->children));
    if (!value->children) return NULL;
    for (size_t i = 0; i < (size_t)keys->len; i++) {
        const char* key = (const char*)(intptr_t)keys->data[i];
        value->children[i*2] = json_text_node(p, key, strlen(key));
        if (!value->children[i*2]) return NULL;
        value->children[i*2+1] = (FernJsonValue*)(intptr_t)values->data[i];
        assert(value->children[i*2+1] != NULL);
    }
    return json_seal(p, value) ? value : NULL;
}

/** Build from checked parallel lists, reserving the source Map bridge allocation. @param keys String list; @param values Value list; @return Result(Value*, Error*). */
int64_t fern_json_value_from_object(const FernList* keys, const FernList* values) {
    assert(keys != NULL);
    assert(values != NULL);
    size_t maximum = (JSON_NODES_MAX - 1) / 2;
    if (!json_list_size(keys, maximum) || !json_list_size(values, maximum) || keys->len != values->len) return json_error(4, -1);
    size_t bytes;
    if (!json_key_bytes(keys, &bytes)) return json_error(4, -1);
    JsonParser parser = json_builder(bytes);
    size_t capacity = keys->len ? (size_t)keys->len : 1;
    parser.allocated = capacity * 16 + 2 * sizeof(FernList);
    return json_built(&parser, json_object_node(&parser, keys, values));
}

/** Copy array references into fresh list storage. @param value valid opaque value; @return Result(List(Value*), Error*). */
int64_t fern_json_value_elements(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->nodes <= JSON_NODES_MAX);
    if (value->kind != J_ARRAY) return json_error(5, -1);
    FernList* list = fern_list_with_capacity(value->length ? (int64_t)value->length : 1);
    for (size_t i = 0; i < value->length; i++) fern_list_push_mut(list, (int64_t)(intptr_t)value->children[i]);
    return fern_result_ok((int64_t)(intptr_t)list);
}

/** Return native member records, reserving the emitted tuple bridge allocation. @param value valid opaque value; @return Result(List(Member*), Error*). */
int64_t fern_json_value_members(const FernJsonValue* value) {
    assert(value != NULL);
    assert(value->nodes <= JSON_NODES_MAX);
    if (value->kind != J_OBJECT) return json_error(5, -1);
    size_t capacity = value->length ? value->length : 1;
    if (capacity > (JSON_ALLOC_MAX - 2 * sizeof(FernList)) / 56) return json_error(4, -1);
    FernList* list = fern_list_with_capacity((int64_t)capacity);
    for (size_t i = 0; i < value->length; i++) {
        FernJsonMember* member = fern_alloc(sizeof(*member));
        member->key = value->children[i*2];
        member->value = value->children[i*2+1];
        fern_list_push_mut(list, (int64_t)(intptr_t)member);
    }
    return fern_result_ok((int64_t)(intptr_t)list);
}

/** Reject an oversized compiler-side adapter before it allocates. @return Err LimitExceeded without input offset. */
int64_t fern_json_value_limit_error(void) {
    assert(JSON_NODES_MAX > 0);
    assert(JSON_ALLOC_MAX >= sizeof(FernJsonError));
    return json_error(4, -1);
}
