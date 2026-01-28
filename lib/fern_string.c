/* Dynamic String Implementation */

#include "fern_string.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct String {
    size_t len;
    char data[];
};

/**
 * Create a new string from data with explicit length.
 * @param arena The arena for allocation.
 * @param data The source data.
 * @param len The length in bytes.
 * @return The new string or NULL on failure.
 */
String* string_new_len(Arena* arena, const char* data, size_t len) {
    assert(arena != NULL);
    assert(data != NULL || len == 0);
    
    String* s = arena_alloc(arena, sizeof(String) + len + 1);
    if (!s) {
        return NULL;
    }
    
    s->len = len;
    if (len > 0) {
        memcpy(s->data, data, len);
    }
    s->data[len] = '\0';
    
    return s;
}

/**
 * Create a new string from a null-terminated C string.
 * @param arena The arena for allocation.
 * @param cstr The null-terminated source string.
 * @return The new string or NULL on failure.
 */
String* string_new(Arena* arena, const char* cstr) {
    assert(arena != NULL);
    assert(cstr != NULL);
    
    return string_new_len(arena, cstr, strlen(cstr));
}

/**
 * Create an empty string.
 * @param arena The arena for allocation.
 * @return The new empty string.
 */
String* string_empty(Arena* arena) {
    assert(arena != NULL);
    String* result = string_new_len(arena, "", 0);
    assert(result != NULL);
    return result;
}

/**
 * Get the null-terminated C string data.
 * @param s The string.
 * @return The C string data.
 */
const char* string_cstr(const String* s) {
    assert(s != NULL);
    assert(s->data != NULL);
    return s->data;
}

/**
 * Get the length of the string in bytes.
 * @param s The string.
 * @return The length in bytes.
 */
size_t string_len(const String* s) {
    assert(s != NULL);
    assert(s->data != NULL);
    return s->len;
}

/**
 * Check if the string is empty.
 * @param s The string.
 * @return True if empty.
 */
bool string_is_empty(const String* s) {
    assert(s != NULL);
    assert(s->data != NULL);
    return s->len == 0;
}

/**
 * Concatenate two strings into a new string.
 * @param arena The arena for allocation.
 * @param a The first string.
 * @param b The second string.
 * @return The concatenated string or NULL on failure.
 */
String* string_concat(Arena* arena, const String* a, const String* b) {
    assert(arena != NULL);
    assert(a != NULL);
    assert(b != NULL);
    
    size_t total_len = a->len + b->len;
    String* result = arena_alloc(arena, sizeof(String) + total_len + 1);
    if (!result) {
        return NULL;
    }
    
    result->len = total_len;
    memcpy(result->data, a->data, a->len);
    memcpy(result->data + a->len, b->data, b->len);
    result->data[total_len] = '\0';
    
    return result;
}

/**
 * Check if two strings are equal.
 * @param a The first string.
 * @param b The second string.
 * @return True if equal.
 */
bool string_equal(const String* a, const String* b) {
    assert(a != NULL);
    assert(b != NULL);
    
    if (a->len != b->len) {
        return false;
    }
    
    return memcmp(a->data, b->data, a->len) == 0;
}

/**
 * Compare two strings lexicographically.
 * @param a The first string.
 * @param b The second string.
 * @return Negative if a < b, 0 if equal, positive if a > b.
 */
int string_compare(const String* a, const String* b) {
    assert(a != NULL);
    assert(b != NULL);
    
    size_t min_len = a->len < b->len ? a->len : b->len;
    int cmp = memcmp(a->data, b->data, min_len);
    
    if (cmp != 0) {
        return cmp;
    }
    
    // Equal up to min_len, compare lengths
    if (a->len < b->len) {
        return -1;
    } else if (a->len > b->len) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * Create a formatted string using printf-style format.
 * @param arena The arena for allocation.
 * @param fmt The format string.
 * @param ... The format arguments.
 * @return The formatted string or NULL on failure.
 */
String* string_format(Arena* arena, const char* fmt, ...) {
    assert(arena != NULL);
    assert(fmt != NULL);
    
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);
    
    // Calculate required size
    int size = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);
    
    if (size < 0) {
        va_end(args2);
        return NULL;
    }
    
    String* s = arena_alloc(arena, sizeof(String) + size + 1);
    if (!s) {
        va_end(args2);
        return NULL;
    }
    
    s->len = size;
    vsnprintf(s->data, size + 1, fmt, args2);
    va_end(args2);
    
    return s;
}
