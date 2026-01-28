/**
 * Fern Runtime Library Implementation
 *
 * Core functions for compiled Fern programs.
 */

#include "fern_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========== I/O Functions ========== */

/**
 * Print an integer to stdout (no newline).
 * @param n The integer to print.
 */
void fern_print_int(int64_t n) {
    printf("%lld", (long long)n);
}

/**
 * Print an integer to stdout with newline.
 * @param n The integer to print.
 */
void fern_println_int(int64_t n) {
    printf("%lld\n", (long long)n);
}

/**
 * Print a string to stdout (no newline).
 * @param s The null-terminated string to print.
 */
void fern_print_str(const char* s) {
    assert(s != NULL);
    printf("%s", s);
}

/**
 * Print a string to stdout with newline.
 * @param s The null-terminated string to print.
 */
void fern_println_str(const char* s) {
    assert(s != NULL);
    printf("%s\n", s);
}

/**
 * Print a boolean to stdout (no newline).
 * @param b The boolean to print (0 = false, non-zero = true).
 */
void fern_print_bool(int64_t b) {
    printf("%s", b ? "true" : "false");
}

/**
 * Print a boolean to stdout with newline.
 * @param b The boolean to print.
 */
void fern_println_bool(int64_t b) {
    printf("%s\n", b ? "true" : "false");
}

/* ========== String Functions ========== */

/**
 * Get the length of a string.
 * @param s The null-terminated string.
 * @return The length in bytes.
 */
int64_t fern_str_len(const char* s) {
    assert(s != NULL);
    return (int64_t)strlen(s);
}

/**
 * Concatenate two strings.
 * @param a First string.
 * @param b Second string.
 * @return Newly allocated concatenated string (caller must free).
 */
char* fern_str_concat(const char* a, const char* b) {
    assert(a != NULL);
    assert(b != NULL);
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    char* result = malloc(len_a + len_b + 1);
    assert(result != NULL);
    memcpy(result, a, len_a);
    memcpy(result + len_a, b, len_b + 1);
    return result;
}

/**
 * Compare two strings for equality.
 * @param a First string.
 * @param b Second string.
 * @return 1 if equal, 0 otherwise.
 */
int64_t fern_str_eq(const char* a, const char* b) {
    assert(a != NULL);
    assert(b != NULL);
    return strcmp(a, b) == 0 ? 1 : 0;
}

/**
 * Check if string starts with prefix.
 * @param s The string.
 * @param prefix The prefix to check.
 * @return 1 if s starts with prefix, 0 otherwise.
 */
int64_t fern_str_starts_with(const char* s, const char* prefix) {
    assert(s != NULL);
    assert(prefix != NULL);
    size_t prefix_len = strlen(prefix);
    return strncmp(s, prefix, prefix_len) == 0 ? 1 : 0;
}

/**
 * Check if string ends with suffix.
 * @param s The string.
 * @param suffix The suffix to check.
 * @return 1 if s ends with suffix, 0 otherwise.
 */
int64_t fern_str_ends_with(const char* s, const char* suffix) {
    assert(s != NULL);
    assert(suffix != NULL);
    size_t s_len = strlen(s);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > s_len) return 0;
    return strcmp(s + s_len - suffix_len, suffix) == 0 ? 1 : 0;
}

/**
 * Check if string contains substring.
 * @param s The string.
 * @param substr The substring to find.
 * @return 1 if s contains substr, 0 otherwise.
 */
int64_t fern_str_contains(const char* s, const char* substr) {
    assert(s != NULL);
    assert(substr != NULL);
    return strstr(s, substr) != NULL ? 1 : 0;
}

/**
 * Find index of substring.
 * @param s The string.
 * @param substr The substring to find.
 * @return Option: Some(index) if found, None otherwise.
 */
int64_t fern_str_index_of(const char* s, const char* substr) {
    assert(s != NULL);
    assert(substr != NULL);
    const char* found = strstr(s, substr);
    if (found == NULL) {
        return fern_option_none();
    }
    return fern_option_some((int64_t)(found - s));
}

/**
 * Get substring from start to end (exclusive).
 * @param s The string.
 * @param start Start index.
 * @param end End index (exclusive).
 * @return New string containing the substring.
 */
char* fern_str_slice(const char* s, int64_t start, int64_t end) {
    assert(s != NULL);
    size_t len = strlen(s);
    
    /* Clamp indices */
    if (start < 0) start = 0;
    if (end < start) end = start;
    if ((size_t)start > len) start = (int64_t)len;
    if ((size_t)end > len) end = (int64_t)len;
    
    size_t slice_len = (size_t)(end - start);
    char* result = malloc(slice_len + 1);
    assert(result != NULL);
    memcpy(result, s + start, slice_len);
    result[slice_len] = '\0';
    return result;
}

/* Helper: check if character is whitespace */
static int is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Trim whitespace from both ends.
 * @param s The string.
 * @return New string with whitespace trimmed.
 */
char* fern_str_trim(const char* s) {
    assert(s != NULL);
    size_t len = strlen(s);
    
    /* Find start (skip leading whitespace) */
    size_t start = 0;
    while (start < len && is_whitespace(s[start])) start++;
    
    /* Find end (skip trailing whitespace) */
    size_t end = len;
    while (end > start && is_whitespace(s[end - 1])) end--;
    
    size_t new_len = end - start;
    char* result = malloc(new_len + 1);
    assert(result != NULL);
    memcpy(result, s + start, new_len);
    result[new_len] = '\0';
    return result;
}

/**
 * Trim whitespace from start.
 * @param s The string.
 * @return New string with leading whitespace trimmed.
 */
char* fern_str_trim_start(const char* s) {
    assert(s != NULL);
    size_t len = strlen(s);
    
    size_t start = 0;
    while (start < len && is_whitespace(s[start])) start++;
    
    size_t new_len = len - start;
    char* result = malloc(new_len + 1);
    assert(result != NULL);
    memcpy(result, s + start, new_len + 1);
    return result;
}

/**
 * Trim whitespace from end.
 * @param s The string.
 * @return New string with trailing whitespace trimmed.
 */
char* fern_str_trim_end(const char* s) {
    assert(s != NULL);
    size_t len = strlen(s);
    
    while (len > 0 && is_whitespace(s[len - 1])) len--;
    
    char* result = malloc(len + 1);
    assert(result != NULL);
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

/**
 * Convert string to uppercase.
 * @param s The string.
 * @return New string in uppercase.
 */
char* fern_str_to_upper(const char* s) {
    assert(s != NULL);
    size_t len = strlen(s);
    char* result = malloc(len + 1);
    assert(result != NULL);
    
    for (size_t i = 0; i <= len; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') {
            result[i] = c - 'a' + 'A';
        } else {
            result[i] = c;
        }
    }
    return result;
}

/**
 * Convert string to lowercase.
 * @param s The string.
 * @return New string in lowercase.
 */
char* fern_str_to_lower(const char* s) {
    assert(s != NULL);
    size_t len = strlen(s);
    char* result = malloc(len + 1);
    assert(result != NULL);
    
    for (size_t i = 0; i <= len; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            result[i] = c - 'A' + 'a';
        } else {
            result[i] = c;
        }
    }
    return result;
}

/**
 * Replace all occurrences of old with new.
 * @param s The string.
 * @param old_str Substring to replace.
 * @param new_str Replacement string.
 * @return New string with replacements.
 */
char* fern_str_replace(const char* s, const char* old_str, const char* new_str) {
    assert(s != NULL);
    assert(old_str != NULL);
    assert(new_str != NULL);
    
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);
    
    /* Empty old_str: return copy of s */
    if (old_len == 0) {
        char* result = malloc(strlen(s) + 1);
        assert(result != NULL);
        strcpy(result, s);
        return result;
    }
    
    /* Count occurrences */
    size_t count = 0;
    const char* p = s;
    while ((p = strstr(p, old_str)) != NULL) {
        count++;
        p += old_len;
    }
    
    /* Calculate new length */
    size_t s_len = strlen(s);
    size_t result_len = s_len + count * (new_len - old_len);
    char* result = malloc(result_len + 1);
    assert(result != NULL);
    
    /* Build result */
    char* dst = result;
    p = s;
    const char* found;
    while ((found = strstr(p, old_str)) != NULL) {
        size_t prefix_len = (size_t)(found - p);
        memcpy(dst, p, prefix_len);
        dst += prefix_len;
        memcpy(dst, new_str, new_len);
        dst += new_len;
        p = found + old_len;
    }
    strcpy(dst, p);
    
    return result;
}

/**
 * Split string by delimiter.
 * @param s The string.
 * @param delim The delimiter.
 * @return List of strings (as FernStringList).
 */
FernStringList* fern_str_split(const char* s, const char* delim) {
    assert(s != NULL);
    assert(delim != NULL);
    
    FernStringList* list = malloc(sizeof(FernStringList));
    assert(list != NULL);
    list->cap = 8;
    list->len = 0;
    list->data = malloc((size_t)list->cap * sizeof(char*));
    assert(list->data != NULL);
    
    size_t delim_len = strlen(delim);
    
    /* Empty delimiter: split into characters */
    if (delim_len == 0) {
        size_t s_len = strlen(s);
        for (size_t i = 0; i < s_len; i++) {
            if (list->len >= list->cap) {
                list->cap *= 2;
                list->data = realloc(list->data, (size_t)list->cap * sizeof(char*));
                assert(list->data != NULL);
            }
            char* ch = malloc(2);
            assert(ch != NULL);
            ch[0] = s[i];
            ch[1] = '\0';
            list->data[list->len++] = ch;
        }
        return list;
    }
    
    /* Split by delimiter */
    const char* p = s;
    const char* found;
    while ((found = strstr(p, delim)) != NULL) {
        if (list->len >= list->cap) {
            list->cap *= 2;
            list->data = realloc(list->data, (size_t)list->cap * sizeof(char*));
            assert(list->data != NULL);
        }
        size_t part_len = (size_t)(found - p);
        char* part = malloc(part_len + 1);
        assert(part != NULL);
        memcpy(part, p, part_len);
        part[part_len] = '\0';
        list->data[list->len++] = part;
        p = found + delim_len;
    }
    
    /* Add remaining part */
    if (list->len >= list->cap) {
        list->cap *= 2;
        list->data = realloc(list->data, (size_t)list->cap * sizeof(char*));
        assert(list->data != NULL);
    }
    char* last = malloc(strlen(p) + 1);
    assert(last != NULL);
    strcpy(last, p);
    list->data[list->len++] = last;
    
    return list;
}

/**
 * Join list of strings with separator.
 * @param list The string list.
 * @param sep The separator.
 * @return New joined string.
 */
char* fern_str_join(FernStringList* list, const char* sep) {
    assert(list != NULL);
    assert(sep != NULL);
    
    if (list->len == 0) {
        char* result = malloc(1);
        assert(result != NULL);
        result[0] = '\0';
        return result;
    }
    
    /* Calculate total length */
    size_t sep_len = strlen(sep);
    size_t total = 0;
    for (int64_t i = 0; i < list->len; i++) {
        total += strlen(list->data[i]);
        if (i < list->len - 1) total += sep_len;
    }
    
    char* result = malloc(total + 1);
    assert(result != NULL);
    
    char* dst = result;
    for (int64_t i = 0; i < list->len; i++) {
        size_t part_len = strlen(list->data[i]);
        memcpy(dst, list->data[i], part_len);
        dst += part_len;
        if (i < list->len - 1) {
            memcpy(dst, sep, sep_len);
            dst += sep_len;
        }
    }
    *dst = '\0';
    
    return result;
}

/**
 * Repeat string n times.
 * @param s The string.
 * @param n Number of repetitions.
 * @return New string with s repeated n times.
 */
char* fern_str_repeat(const char* s, int64_t n) {
    assert(s != NULL);
    
    if (n <= 0) {
        char* result = malloc(1);
        assert(result != NULL);
        result[0] = '\0';
        return result;
    }
    
    size_t s_len = strlen(s);
    size_t total = s_len * (size_t)n;
    char* result = malloc(total + 1);
    assert(result != NULL);
    
    char* dst = result;
    for (int64_t i = 0; i < n; i++) {
        memcpy(dst, s, s_len);
        dst += s_len;
    }
    *dst = '\0';
    
    return result;
}

/**
 * Get character at index.
 * @param s The string.
 * @param index The index.
 * @return Option: Some(char as int) if valid index, None otherwise.
 */
int64_t fern_str_char_at(const char* s, int64_t index) {
    assert(s != NULL);
    
    if (index < 0) return fern_option_none();
    
    size_t len = strlen(s);
    if ((size_t)index >= len) return fern_option_none();
    
    return fern_option_some((int64_t)(unsigned char)s[index]);
}

/**
 * Check if string is empty.
 * @param s The string.
 * @return 1 if empty, 0 otherwise.
 */
int64_t fern_str_is_empty(const char* s) {
    assert(s != NULL);
    return s[0] == '\0' ? 1 : 0;
}

/**
 * Free a string list.
 * @param list The list to free.
 */
void fern_str_list_free(FernStringList* list) {
    if (list != NULL) {
        for (int64_t i = 0; i < list->len; i++) {
            free(list->data[i]);
        }
        free(list->data);
        free(list);
    }
}

/* ========== List Functions ========== */

/**
 * Create a new empty list.
 * @return Pointer to new list.
 */
FernList* fern_list_new(void) {
    return fern_list_with_capacity(8);
}

/**
 * Create a list with given capacity.
 * @param cap Initial capacity.
 * @return Pointer to new list.
 */
FernList* fern_list_with_capacity(int64_t cap) {
    assert(cap > 0);
    FernList* list = malloc(sizeof(FernList));
    assert(list != NULL);
    list->data = malloc((size_t)cap * sizeof(int64_t));
    assert(list->data != NULL);
    list->len = 0;
    list->cap = cap;
    return list;
}

/**
 * Get the length of a list.
 * @param list The list.
 * @return Number of elements.
 */
int64_t fern_list_len(FernList* list) {
    assert(list != NULL);
    return list->len;
}

/**
 * Get element at index.
 * @param list The list.
 * @param index The index (0-based).
 * @return The element value.
 */
int64_t fern_list_get(FernList* list, int64_t index) {
    assert(list != NULL);
    assert(index >= 0 && index < list->len);
    return list->data[index];
}

/**
 * Append an element to a list in place (mutates the list).
 * Used for list literal construction.
 * @param list The list to modify.
 * @param value The value to append.
 */
void fern_list_push_mut(FernList* list, int64_t value) {
    assert(list != NULL);

    /* Grow if needed */
    if (list->len >= list->cap) {
        int64_t new_cap = list->cap * 2;
        if (new_cap < 8) new_cap = 8;
        list->data = realloc(list->data, (size_t)new_cap * sizeof(int64_t));
        assert(list->data != NULL);
        list->cap = new_cap;
    }
    
    list->data[list->len++] = value;
}

/**
 * Append an element to a list (returns new list).
 * @param list The original list.
 * @param value The value to append.
 * @return New list with element appended.
 */
FernList* fern_list_push(FernList* list, int64_t value) {
    assert(list != NULL);

    /* Create new list with copied data */
    int64_t new_cap = list->len + 1;
    if (new_cap < list->cap) {
        new_cap = list->cap;
    }
    FernList* new_list = fern_list_with_capacity(new_cap);
    assert(new_list != NULL);

    /* Copy existing elements */
    for (int64_t i = 0; i < list->len; i++) {
        new_list->data[i] = list->data[i];
    }
    new_list->data[list->len] = value;
    new_list->len = list->len + 1;

    return new_list;
}

/**
 * Map a function over a list.
 * @param list The list.
 * @param fn Function pointer (int64_t -> int64_t).
 * @return New list with function applied to each element.
 */
FernList* fern_list_map(FernList* list, int64_t (*fn)(int64_t)) {
    assert(list != NULL);
    assert(fn != NULL);

    FernList* new_list = fern_list_with_capacity(list->len > 0 ? list->len : 1);
    assert(new_list != NULL);

    for (int64_t i = 0; i < list->len; i++) {
        new_list->data[i] = fn(list->data[i]);
    }
    new_list->len = list->len;

    return new_list;
}

/**
 * Fold a list from left.
 * @param list The list.
 * @param init Initial accumulator value.
 * @param fn Function (acc, elem) -> acc.
 * @return Final accumulated value.
 */
int64_t fern_list_fold(FernList* list, int64_t init, int64_t (*fn)(int64_t, int64_t)) {
    assert(list != NULL);
    assert(fn != NULL);

    int64_t acc = init;
    for (int64_t i = 0; i < list->len; i++) {
        acc = fn(acc, list->data[i]);
    }
    return acc;
}

/**
 * Free a list.
 * @param list The list to free.
 */
void fern_list_free(FernList* list) {
    if (list != NULL) {
        free(list->data);
        free(list);
    }
}

/**
 * Filter a list with a predicate.
 * @param list The list.
 * @param pred Predicate function (int64_t -> bool as int64_t).
 * @return New list with elements where pred returns non-zero.
 */
FernList* fern_list_filter(FernList* list, int64_t (*pred)(int64_t)) {
    assert(list != NULL);
    assert(pred != NULL);

    FernList* new_list = fern_list_with_capacity(list->len > 0 ? list->len : 1);
    assert(new_list != NULL);

    for (int64_t i = 0; i < list->len; i++) {
        if (pred(list->data[i])) {
            /* Grow if needed */
            if (new_list->len >= new_list->cap) {
                new_list->cap *= 2;
                new_list->data = realloc(new_list->data, 
                    (size_t)new_list->cap * sizeof(int64_t));
                assert(new_list->data != NULL);
            }
            new_list->data[new_list->len++] = list->data[i];
        }
    }

    return new_list;
}

/**
 * Find first element matching predicate.
 * @param list The list.
 * @param pred Predicate function.
 * @return Option: Some(element) if found, None otherwise.
 */
int64_t fern_list_find(FernList* list, int64_t (*pred)(int64_t)) {
    assert(list != NULL);
    assert(pred != NULL);

    for (int64_t i = 0; i < list->len; i++) {
        if (pred(list->data[i])) {
            return fern_option_some(list->data[i]);
        }
    }

    return fern_option_none();
}

/**
 * Reverse a list.
 * @param list The list.
 * @return New list with elements in reverse order.
 */
FernList* fern_list_reverse(FernList* list) {
    assert(list != NULL);

    FernList* new_list = fern_list_with_capacity(list->len > 0 ? list->len : 1);
    assert(new_list != NULL);

    for (int64_t i = list->len - 1; i >= 0; i--) {
        new_list->data[list->len - 1 - i] = list->data[i];
    }
    new_list->len = list->len;

    return new_list;
}

/**
 * Concatenate two lists.
 * @param a First list.
 * @param b Second list.
 * @return New list with all elements from a followed by b.
 */
FernList* fern_list_concat(FernList* a, FernList* b) {
    assert(a != NULL);
    assert(b != NULL);

    int64_t total = a->len + b->len;
    FernList* new_list = fern_list_with_capacity(total > 0 ? total : 1);
    assert(new_list != NULL);

    for (int64_t i = 0; i < a->len; i++) {
        new_list->data[i] = a->data[i];
    }
    for (int64_t i = 0; i < b->len; i++) {
        new_list->data[a->len + i] = b->data[i];
    }
    new_list->len = total;

    return new_list;
}

/**
 * Get first element of a list.
 * @param list The list.
 * @return Option: Some(first) if non-empty, None otherwise.
 */
int64_t fern_list_head(FernList* list) {
    assert(list != NULL);

    if (list->len == 0) {
        return fern_option_none();
    }
    return fern_option_some(list->data[0]);
}

/**
 * Get list without first element.
 * @param list The list.
 * @return New list without first element (empty if list was empty/single).
 */
FernList* fern_list_tail(FernList* list) {
    assert(list != NULL);

    if (list->len <= 1) {
        return fern_list_new();
    }

    FernList* new_list = fern_list_with_capacity(list->len - 1);
    assert(new_list != NULL);

    for (int64_t i = 1; i < list->len; i++) {
        new_list->data[i - 1] = list->data[i];
    }
    new_list->len = list->len - 1;

    return new_list;
}

/**
 * Check if list is empty.
 * @param list The list.
 * @return 1 if empty, 0 otherwise.
 */
int64_t fern_list_is_empty(FernList* list) {
    assert(list != NULL);
    return list->len == 0 ? 1 : 0;
}

/**
 * Check if any element matches predicate.
 * @param list The list.
 * @param pred Predicate function.
 * @return 1 if any element matches, 0 otherwise.
 */
int64_t fern_list_any(FernList* list, int64_t (*pred)(int64_t)) {
    assert(list != NULL);
    assert(pred != NULL);

    for (int64_t i = 0; i < list->len; i++) {
        if (pred(list->data[i])) {
            return 1;
        }
    }
    return 0;
}

/**
 * Check if all elements match predicate.
 * @param list The list.
 * @param pred Predicate function.
 * @return 1 if all elements match, 0 otherwise.
 */
int64_t fern_list_all(FernList* list, int64_t (*pred)(int64_t)) {
    assert(list != NULL);
    assert(pred != NULL);

    for (int64_t i = 0; i < list->len; i++) {
        if (!pred(list->data[i])) {
            return 0;
        }
    }
    return 1;
}

/* ========== Memory Functions ========== */

/**
 * Allocate memory.
 * @param size Number of bytes.
 * @return Pointer to allocated memory.
 */
void* fern_alloc(size_t size) {
    void* ptr = malloc(size);
    assert(ptr != NULL);
    return ptr;
}

/**
 * Free memory.
 * @param ptr Pointer to free.
 */
void fern_free(void* ptr) {
    free(ptr);
}

/* ========== Result Type ========== */

/*
 * Result encoding (packed 64-bit):
 * - Bits 0-31: tag (0 = Ok, 1 = Err)
 * - Bits 32-63: value
 */

#define RESULT_TAG_OK  0
#define RESULT_TAG_ERR 1

/**
 * Create an Ok result.
 * @param value The success value.
 * @return Packed Result value.
 */
int64_t fern_result_ok(int64_t value) {
    /* Pack: value in upper 32 bits, tag=0 in lower 32 bits */
    return ((value & 0xFFFFFFFF) << 32) | RESULT_TAG_OK;
}

/**
 * Create an Err result.
 * @param value The error value.
 * @return Packed Result value.
 */
int64_t fern_result_err(int64_t value) {
    /* Pack: value in upper 32 bits, tag=1 in lower 32 bits */
    return ((value & 0xFFFFFFFF) << 32) | RESULT_TAG_ERR;
}

/**
 * Check if a Result is Ok.
 * @param result The packed Result value.
 * @return 1 if Ok, 0 if Err.
 */
int64_t fern_result_is_ok(int64_t result) {
    return (result & 0xFFFFFFFF) == RESULT_TAG_OK ? 1 : 0;
}

/**
 * Unwrap the value from a Result.
 * @param result The packed Result value.
 * @return The contained value (ok or err).
 */
int64_t fern_result_unwrap(int64_t result) {
    /* Extract value from upper 32 bits, sign-extend */
    return (int64_t)((int32_t)(result >> 32));
}

/**
 * Map a function over an Ok value.
 * @param result The Result.
 * @param fn Function to apply if Ok.
 * @return New Result with mapped value, or original Err.
 */
int64_t fern_result_map(int64_t result, int64_t (*fn)(int64_t)) {
    assert(fn != NULL);
    if (fern_result_is_ok(result)) {
        int64_t value = fern_result_unwrap(result);
        return fern_result_ok(fn(value));
    }
    return result;
}

/**
 * Chain a function that returns Result over an Ok value.
 * @param result The Result.
 * @param fn Function to apply if Ok (returns Result).
 * @return Result from fn if Ok, or original Err.
 */
int64_t fern_result_and_then(int64_t result, int64_t (*fn)(int64_t)) {
    assert(fn != NULL);
    if (fern_result_is_ok(result)) {
        int64_t value = fern_result_unwrap(result);
        return fn(value);
    }
    return result;
}

/**
 * Get the Ok value or a default.
 * @param result The Result.
 * @param default_val Value to return if Err.
 * @return Ok value or default.
 */
int64_t fern_result_unwrap_or(int64_t result, int64_t default_val) {
    if (fern_result_is_ok(result)) {
        return fern_result_unwrap(result);
    }
    return default_val;
}

/**
 * Get the Ok value or compute a default from the error.
 * @param result The Result.
 * @param fn Function to compute default from error.
 * @return Ok value or fn(err_value).
 */
int64_t fern_result_unwrap_or_else(int64_t result, int64_t (*fn)(int64_t)) {
    assert(fn != NULL);
    if (fern_result_is_ok(result)) {
        return fern_result_unwrap(result);
    }
    return fn(fern_result_unwrap(result));
}

/* ========== Option Type ========== */

/*
 * Option encoding (packed 64-bit):
 * - Bits 0-31: tag (0 = None, 1 = Some)
 * - Bits 32-63: value (only meaningful if Some)
 */

#define OPTION_TAG_NONE 0
#define OPTION_TAG_SOME 1

/**
 * Create a Some option.
 * @param value The contained value.
 * @return Packed Option value.
 */
int64_t fern_option_some(int64_t value) {
    return ((value & 0xFFFFFFFF) << 32) | OPTION_TAG_SOME;
}

/**
 * Create a None option.
 * @return Packed Option value representing None.
 */
int64_t fern_option_none(void) {
    return OPTION_TAG_NONE;
}

/**
 * Check if an Option is Some.
 * @param option The packed Option value.
 * @return 1 if Some, 0 if None.
 */
int64_t fern_option_is_some(int64_t option) {
    return (option & 0xFFFFFFFF) == OPTION_TAG_SOME ? 1 : 0;
}

/**
 * Unwrap the value from an Option.
 * @param option The packed Option value.
 * @return The contained value (undefined if None).
 */
int64_t fern_option_unwrap(int64_t option) {
    return (int64_t)((int32_t)(option >> 32));
}

/**
 * Map a function over a Some value.
 * @param option The Option.
 * @param fn Function to apply if Some.
 * @return New Option with mapped value, or None.
 */
int64_t fern_option_map(int64_t option, int64_t (*fn)(int64_t)) {
    assert(fn != NULL);
    if (fern_option_is_some(option)) {
        int64_t value = fern_option_unwrap(option);
        return fern_option_some(fn(value));
    }
    return option;
}

/**
 * Get the Some value or a default.
 * @param option The Option.
 * @param default_val Value to return if None.
 * @return Some value or default.
 */
int64_t fern_option_unwrap_or(int64_t option, int64_t default_val) {
    if (fern_option_is_some(option)) {
        return fern_option_unwrap(option);
    }
    return default_val;
}

/* ========== File I/O Functions ========== */

/**
 * Read entire file contents as a string.
 * @param path The file path.
 * @return Result: Ok(string pointer) or Err(error code).
 */
int64_t fern_read_file(const char* path) {
    assert(path != NULL);
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        return fern_result_err(FERN_ERR_FILE_NOT_FOUND);
    }
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return fern_result_err(FERN_ERR_IO);
    }
    
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return fern_result_err(FERN_ERR_IO);
    }
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return fern_result_err(FERN_ERR_IO);
    }
    
    /* Allocate buffer for contents + null terminator */
    char* contents = malloc((size_t)size + 1);
    if (!contents) {
        fclose(f);
        return fern_result_err(FERN_ERR_OUT_OF_MEMORY);
    }
    
    /* Read file contents */
    size_t read_size = fread(contents, 1, (size_t)size, f);
    fclose(f);
    
    if ((long)read_size != size) {
        free(contents);
        return fern_result_err(FERN_ERR_IO);
    }
    
    contents[size] = '\0';
    
    /* Return Ok with pointer cast to int64_t */
    return fern_result_ok((int64_t)(intptr_t)contents);
}

/**
 * Write string to file (overwrites if exists).
 * @param path The file path.
 * @param contents The string to write.
 * @return Result: Ok(bytes written) or Err(error code).
 */
int64_t fern_write_file(const char* path, const char* contents) {
    assert(path != NULL);
    assert(contents != NULL);
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        return fern_result_err(FERN_ERR_PERMISSION);
    }
    
    size_t len = strlen(contents);
    size_t written = fwrite(contents, 1, len, f);
    fclose(f);
    
    if (written != len) {
        return fern_result_err(FERN_ERR_IO);
    }
    
    return fern_result_ok((int64_t)written);
}

/**
 * Append string to file (creates if not exists).
 * @param path The file path.
 * @param contents The string to append.
 * @return Result: Ok(bytes written) or Err(error code).
 */
int64_t fern_append_file(const char* path, const char* contents) {
    assert(path != NULL);
    assert(contents != NULL);
    
    FILE* f = fopen(path, "ab");
    if (!f) {
        return fern_result_err(FERN_ERR_PERMISSION);
    }
    
    size_t len = strlen(contents);
    size_t written = fwrite(contents, 1, len, f);
    fclose(f);
    
    if (written != len) {
        return fern_result_err(FERN_ERR_IO);
    }
    
    return fern_result_ok((int64_t)written);
}

/**
 * Check if file exists.
 * @param path The file path.
 * @return 1 if exists, 0 otherwise.
 */
int64_t fern_file_exists(const char* path) {
    assert(path != NULL);
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

/**
 * Delete a file.
 * @param path The file path.
 * @return Result: Ok(0) if deleted, Err(error code) otherwise.
 */
int64_t fern_delete_file(const char* path) {
    assert(path != NULL);
    if (remove(path) == 0) {
        return fern_result_ok(0);
    }
    return fern_result_err(FERN_ERR_FILE_NOT_FOUND);
}

/**
 * Get file size in bytes.
 * @param path The file path.
 * @return Result: Ok(size) or Err(error code).
 */
int64_t fern_file_size(const char* path) {
    assert(path != NULL);
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        return fern_result_err(FERN_ERR_FILE_NOT_FOUND);
    }
    
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return fern_result_err(FERN_ERR_IO);
    }
    
    long size = ftell(f);
    fclose(f);
    
    if (size < 0) {
        return fern_result_err(FERN_ERR_IO);
    }
    
    return fern_result_ok((int64_t)size);
}
