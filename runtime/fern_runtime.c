/**
 * Fern Runtime Library Implementation
 *
 * Core functions for compiled Fern programs.
 */

/* Enable POSIX/BSD functions in strict C11 mode */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _BSD_SOURCE

#include "fern_runtime.h"
#include "fern_gc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <unistd.h>
#include <regex.h>

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
 * Convert an integer to a string.
 * @param n The integer to convert.
 * @return Newly allocated string (caller must free).
 */
char* fern_int_to_str(int64_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)n);
    char* result = FERN_ALLOC(strlen(buf) + 1);
    assert(result != NULL);
    strcpy(result, buf);
    return result;
}

/**
 * Convert a boolean to a string.
 * @param b The boolean (0 = false, non-zero = true).
 * @return Newly allocated string (caller must free).
 */
char* fern_bool_to_str(int64_t b) {
    const char* str = b ? "true" : "false";
    char* result = FERN_ALLOC(strlen(str) + 1);
    assert(result != NULL);
    strcpy(result, str);
    return result;
}

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
    char* result = FERN_ALLOC(len_a + len_b + 1);
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
    char* result = FERN_ALLOC(slice_len + 1);
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
    char* result = FERN_ALLOC(new_len + 1);
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
    char* result = FERN_ALLOC(new_len + 1);
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
    
    char* result = FERN_ALLOC(len + 1);
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
    char* result = FERN_ALLOC(len + 1);
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
    char* result = FERN_ALLOC(len + 1);
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
        char* result = FERN_ALLOC(strlen(s) + 1);
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
    char* result = FERN_ALLOC(result_len + 1);
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
    
    FernStringList* list = FERN_ALLOC(sizeof(FernStringList));
    assert(list != NULL);
    list->cap = 8;
    list->len = 0;
    list->data = FERN_ALLOC((size_t)list->cap * sizeof(char*));
    assert(list->data != NULL);
    
    size_t delim_len = strlen(delim);
    
    /* Empty delimiter: split into characters */
    if (delim_len == 0) {
        size_t s_len = strlen(s);
        for (size_t i = 0; i < s_len; i++) {
            if (list->len >= list->cap) {
                list->cap *= 2;
                list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
                assert(list->data != NULL);
            }
            char* ch = FERN_ALLOC(2);
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
            list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
            assert(list->data != NULL);
        }
        size_t part_len = (size_t)(found - p);
        char* part = FERN_ALLOC(part_len + 1);
        assert(part != NULL);
        memcpy(part, p, part_len);
        part[part_len] = '\0';
        list->data[list->len++] = part;
        p = found + delim_len;
    }
    
    /* Add remaining part */
    if (list->len >= list->cap) {
        list->cap *= 2;
        list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
        assert(list->data != NULL);
    }
    char* last = FERN_ALLOC(strlen(p) + 1);
    assert(last != NULL);
    strcpy(last, p);
    list->data[list->len++] = last;
    
    return list;
}

/**
 * Split string into lines.
 * Handles \n, \r\n, and \r line endings.
 * @param s The string.
 * @return List of lines (as FernStringList).
 */
FernStringList* fern_str_lines(const char* s) {
    assert(s != NULL);
    
    FernStringList* list = FERN_ALLOC(sizeof(FernStringList));
    assert(list != NULL);
    list->cap = 8;
    list->len = 0;
    list->data = FERN_ALLOC((size_t)list->cap * sizeof(char*));
    assert(list->data != NULL);
    
    const char* p = s;
    const char* line_start = s;
    
    while (*p != '\0') {
        if (*p == '\n' || *p == '\r') {
            /* End of line found - add the line */
            if (list->len >= list->cap) {
                list->cap *= 2;
                list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
                assert(list->data != NULL);
            }
            size_t line_len = (size_t)(p - line_start);
            char* line = FERN_ALLOC(line_len + 1);
            assert(line != NULL);
            memcpy(line, line_start, line_len);
            line[line_len] = '\0';
            list->data[list->len++] = line;
            
            /* Skip line ending - handle \r\n as single newline */
            if (*p == '\r' && *(p + 1) == '\n') {
                p += 2;
            } else {
                p++;
            }
            line_start = p;
        } else {
            p++;
        }
    }
    
    /* Add remaining content as last line (even if empty after final newline) */
    if (p > line_start || list->len == 0) {
        if (list->len >= list->cap) {
            list->cap *= 2;
            list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
            assert(list->data != NULL);
        }
        size_t line_len = (size_t)(p - line_start);
        char* line = FERN_ALLOC(line_len + 1);
        assert(line != NULL);
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';
        list->data[list->len++] = line;
    }
    
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
        char* result = FERN_ALLOC(1);
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
    
    char* result = FERN_ALLOC(total + 1);
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
        char* result = FERN_ALLOC(1);
        assert(result != NULL);
        result[0] = '\0';
        return result;
    }
    
    size_t s_len = strlen(s);
    size_t total = s_len * (size_t)n;
    char* result = FERN_ALLOC(total + 1);
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
void fern_str_list_FERN_FREE(FernStringList* list) {
    if (list != NULL) {
        for (int64_t i = 0; i < list->len; i++) {
            FERN_FREE(list->data[i]);
        }
        FERN_FREE(list->data);
        FERN_FREE(list);
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
    FernList* list = FERN_ALLOC(sizeof(FernList));
    assert(list != NULL);
    list->data = FERN_ALLOC((size_t)cap * sizeof(int64_t));
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
        list->data = FERN_REALLOC(list->data, (size_t)new_cap * sizeof(int64_t));
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
void fern_list_FERN_FREE(FernList* list) {
    if (list != NULL) {
        FERN_FREE(list->data);
        FERN_FREE(list);
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
                new_list->data = FERN_REALLOC(new_list->data, 
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
 * @return The first element (panics if list is empty).
 */
int64_t fern_list_head(FernList* list) {
    assert(list != NULL);
    assert(list->len > 0 && "List.head called on empty list");

    return list->data[0];
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
 * Check if list contains element (by value equality for Int).
 * @param list The list.
 * @param elem The element to find.
 * @return 1 if found, 0 otherwise.
 */
int64_t fern_list_contains(FernList* list, int64_t elem) {
    assert(list != NULL);

    for (int64_t i = 0; i < list->len; i++) {
        if (list->data[i] == elem) {
            return 1;
        }
    }
    return 0;
}

/**
 * Check if string list contains string (by string equality).
 * @param list The list of strings.
 * @param s The string to find.
 * @return 1 if found, 0 otherwise.
 */
int64_t fern_list_contains_str(FernList* list, const char* s) {
    assert(list != NULL);
    assert(s != NULL);

    for (int64_t i = 0; i < list->len; i++) {
        const char* elem = (const char*)list->data[i];
        if (elem != NULL && strcmp(elem, s) == 0) {
            return 1;
        }
    }
    return 0;
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

/* ========== System Functions (argc/argv access) ========== */

/* Global storage for command-line arguments */
static int g_argc = 0;
static char** g_argv = NULL;

/**
 * Set command-line arguments (called from C main wrapper).
 * @param argc Argument count.
 * @param argv Argument vector.
 */
void fern_set_args(int argc, char** argv) {
    assert(argc >= 0);
    assert(argv != NULL || argc == 0);
    g_argc = argc;
    g_argv = argv;
}

/**
 * Get command-line argument count.
 * @return Number of arguments (including program name).
 */
int64_t fern_args_count(void) {
    return (int64_t)g_argc;
}

/**
 * Get command-line argument at index.
 * @param index The index (0 = program name).
 * @return The argument string, or empty string if out of bounds.
 */
char* fern_arg(int64_t index) {
    assert(g_argv != NULL || g_argc == 0);
    if (index < 0 || index >= (int64_t)g_argc) {
        /* Return empty string for out-of-bounds access */
        char* empty = FERN_ALLOC(1);
        assert(empty != NULL);
        empty[0] = '\0';
        return empty;
    }
    /* Return a copy of the argument string */
    size_t len = strlen(g_argv[index]);
    char* result = FERN_ALLOC(len + 1);
    assert(result != NULL);
    memcpy(result, g_argv[index], len + 1);
    return result;
}

/**
 * Get all command-line arguments as a string list.
 * @return FernStringList containing all arguments.
 */
FernStringList* fern_args(void) {
    assert(g_argv != NULL || g_argc == 0);
    
    FernStringList* list = FERN_ALLOC(sizeof(FernStringList));
    assert(list != NULL);
    list->len = g_argc;
    list->cap = g_argc > 0 ? g_argc : 1;
    list->data = FERN_ALLOC((size_t)list->cap * sizeof(char*));
    assert(list->data != NULL);
    
    for (int i = 0; i < g_argc; i++) {
        size_t len = strlen(g_argv[i]);
        list->data[i] = FERN_ALLOC(len + 1);
        assert(list->data[i] != NULL);
        memcpy(list->data[i], g_argv[i], len + 1);
    }
    
    return list;
}

/**
 * Exit program immediately with given code.
 * @param code Exit code.
 */
void fern_exit(int64_t code) {
    exit((int)code);
}

/* C entry point - calls user's fern_main */
extern int fern_main(void);

int main(int argc, char** argv) {
    /* Initialize the garbage collector before any allocations */
    fern_gc_init();

    fern_set_args(argc, argv);
    return fern_main();
}

/**
 * Execute a shell command and capture output.
 * @param cmd The command to execute.
 * @return FernExecResult with exit code, stdout, and stderr.
 */
FernExecResult* fern_exec(const char* cmd) {
    assert(cmd != NULL);
    
    FernExecResult* result = FERN_ALLOC(sizeof(FernExecResult));
    assert(result != NULL);
    result->exit_code = -1;
    result->stdout_str = NULL;
    result->stderr_str = NULL;
    
    /* Create temp file for stderr */
    char stderr_template[] = "/tmp/fern_stderr_XXXXXX";
    int stderr_fd = mkstemp(stderr_template);
    if (stderr_fd < 0) {
        result->stdout_str = FERN_STRDUP("");
        result->stderr_str = FERN_STRDUP("Failed to create temp file for stderr");
        return result;
    }
    close(stderr_fd);
    
    /* Build command that redirects stderr to temp file */
    size_t cmd_len = strlen(cmd) + strlen(stderr_template) + 32;
    char* full_cmd = FERN_ALLOC(cmd_len);
    assert(full_cmd != NULL);
    snprintf(full_cmd, cmd_len, "{ %s; } 2>%s", cmd, stderr_template);
    
    /* Execute command and capture stdout */
    FILE* pipe = popen(full_cmd, "r");
    FERN_FREE(full_cmd);
    
    if (!pipe) {
        result->stdout_str = FERN_STRDUP("");
        result->stderr_str = FERN_STRDUP("Failed to execute command");
        unlink(stderr_template);
        return result;
    }
    
    /* Read stdout */
    size_t stdout_cap = 4096;
    size_t stdout_len = 0;
    char* stdout_buf = FERN_ALLOC(stdout_cap);
    assert(stdout_buf != NULL);
    
    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe) != NULL) {
        size_t buf_len = strlen(buf);
        if (stdout_len + buf_len >= stdout_cap) {
            stdout_cap *= 2;
            stdout_buf = FERN_REALLOC(stdout_buf, stdout_cap);
            assert(stdout_buf != NULL);
        }
        memcpy(stdout_buf + stdout_len, buf, buf_len);
        stdout_len += buf_len;
    }
    stdout_buf[stdout_len] = '\0';
    
    int status = pclose(pipe);
    result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result->stdout_str = stdout_buf;
    
    /* Read stderr from temp file */
    FILE* stderr_file = fopen(stderr_template, "r");
    if (stderr_file) {
        fseek(stderr_file, 0, SEEK_END);
        long stderr_size = ftell(stderr_file);
        fseek(stderr_file, 0, SEEK_SET);
        
        char* stderr_buf = FERN_ALLOC((size_t)stderr_size + 1);
        assert(stderr_buf != NULL);
        size_t read_size = fread(stderr_buf, 1, (size_t)stderr_size, stderr_file);
        stderr_buf[read_size] = '\0';
        fclose(stderr_file);
        result->stderr_str = stderr_buf;
    } else {
        result->stderr_str = FERN_STRDUP("");
    }
    
    unlink(stderr_template);
    return result;
}

/**
 * Execute a command with arguments (no shell).
 * @param args FernStringList of command and arguments.
 * @return FernExecResult with exit code, stdout, and stderr.
 */
FernExecResult* fern_exec_args(FernStringList* args) {
    assert(args != NULL);
    
    /* For simplicity, join args and use shell execution */
    /* A proper implementation would use fork/exec directly */
    if (args->len == 0) {
        FernExecResult* result = FERN_ALLOC(sizeof(FernExecResult));
        assert(result != NULL);
        result->exit_code = -1;
        result->stdout_str = FERN_STRDUP("");
        result->stderr_str = FERN_STRDUP("No command specified");
        return result;
    }
    
    /* Calculate total length needed */
    size_t total_len = 0;
    for (int64_t i = 0; i < args->len; i++) {
        total_len += strlen(args->data[i]) + 3; /* quotes + space */
    }
    
    /* Build quoted command string */
    char* cmd = FERN_ALLOC(total_len + 1);
    assert(cmd != NULL);
    char* p = cmd;
    for (int64_t i = 0; i < args->len; i++) {
        if (i > 0) *p++ = ' ';
        /* Simple quoting - wrap in single quotes */
        *p++ = '\'';
        const char* arg = args->data[i];
        while (*arg) {
            if (*arg == '\'') {
                /* Escape single quote: ' -> '\'' */
                memcpy(p, "'\\''", 4);
                p += 4;
            } else {
                *p++ = *arg;
            }
            arg++;
        }
        *p++ = '\'';
    }
    *p = '\0';
    
    FernExecResult* result = fern_exec(cmd);
    FERN_FREE(cmd);
    return result;
}

/**
 * Get environment variable.
 * @param name The variable name.
 * @return The value, or empty string if not set.
 */
char* fern_getenv(const char* name) {
    assert(name != NULL);
    const char* value = getenv(name);
    if (value == NULL) {
        char* empty = FERN_ALLOC(1);
        assert(empty != NULL);
        empty[0] = '\0';
        return empty;
    }
    return FERN_STRDUP(value);
}

/**
 * Set environment variable.
 * @param name The variable name.
 * @param value The value to set.
 * @return 0 on success, non-zero on failure.
 */
int64_t fern_setenv(const char* name, const char* value) {
    assert(name != NULL);
    assert(value != NULL);
    return setenv(name, value, 1);
}

/**
 * Get current working directory.
 * @return The current working directory path.
 */
char* fern_cwd(void) {
    char* buf = FERN_ALLOC(4096);
    assert(buf != NULL);
    if (getcwd(buf, 4096) == NULL) {
        /* Return empty string on error */
        buf[0] = '\0';
    }
    return buf;
}

/**
 * Change current working directory.
 * @param path The path to change to.
 * @return 0 on success, -1 on failure.
 */
int64_t fern_chdir(const char* path) {
    assert(path != NULL);
    return chdir(path);
}

/**
 * Get system hostname.
 * @return The hostname string.
 */
char* fern_hostname(void) {
    char* buf = FERN_ALLOC(256);
    assert(buf != NULL);
    if (gethostname(buf, 256) != 0) {
        /* Return empty string on error */
        buf[0] = '\0';
    }
    return buf;
}

/**
 * Get current username.
 * @return The username string.
 */
char* fern_user(void) {
    const char* user = getenv("USER");
    if (user == NULL) {
        user = getenv("LOGNAME");
    }
    if (user == NULL) {
        char* empty = FERN_ALLOC(1);
        assert(empty != NULL);
        empty[0] = '\0';
        return empty;
    }
    return FERN_STRDUP(user);
}

/**
 * Get home directory.
 * @return The home directory path.
 */
char* fern_home(void) {
    const char* home = getenv("HOME");
    if (home == NULL) {
        char* empty = FERN_ALLOC(1);
        assert(empty != NULL);
        empty[0] = '\0';
        return empty;
    }
    return FERN_STRDUP(home);
}

/* ========== Memory Functions ========== */

/**
 * Allocate memory.
 * @param size Number of bytes.
 * @return Pointer to allocated memory.
 */
void* fern_alloc(size_t size) {
    void* ptr = FERN_ALLOC(size);
    assert(ptr != NULL);
    return ptr;
}

/**
 * Free memory.
 * @param ptr Pointer to free.
 */
void fern_free(void* ptr) {
    FERN_FREE(ptr);
}

/* ========== Result Type ========== */

/*
 * Result encoding: heap-allocated struct to support full 64-bit values.
 * This allows Result to contain pointers (strings, lists, etc.).
 *
 * FernResult {
 *   tag: int32_t (0 = Ok, 1 = Err)
 *   value: int64_t (the ok/err value, can be a pointer)
 * }
 *
 * The Result is passed around as a pointer (int64_t).
 */

typedef struct {
    int32_t tag;
    int64_t value;
} FernResult;

#define RESULT_TAG_OK  0
#define RESULT_TAG_ERR 1

/**
 * Create an Ok result.
 * @param value The success value (can be a pointer).
 * @return Pointer to heap-allocated Result.
 */
int64_t fern_result_ok(int64_t value) {
    FernResult* result = FERN_ALLOC(sizeof(FernResult));
    result->tag = RESULT_TAG_OK;
    result->value = value;
    return (int64_t)(intptr_t)result;
}

/**
 * Create an Err result.
 * @param value The error value.
 * @return Pointer to heap-allocated Result.
 */
int64_t fern_result_err(int64_t value) {
    FernResult* result = FERN_ALLOC(sizeof(FernResult));
    result->tag = RESULT_TAG_ERR;
    result->value = value;
    return (int64_t)(intptr_t)result;
}

/**
 * Check if a Result is Ok.
 * @param result Pointer to Result.
 * @return 1 if Ok, 0 if Err.
 */
int64_t fern_result_is_ok(int64_t result) {
    FernResult* r = (FernResult*)(intptr_t)result;
    return r->tag == RESULT_TAG_OK ? 1 : 0;
}

/**
 * Unwrap the value from a Result.
 * @param result Pointer to Result.
 * @return The contained value (ok or err).
 */
int64_t fern_result_unwrap(int64_t result) {
    FernResult* r = (FernResult*)(intptr_t)result;
    return r->value;
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
 * Option encoding: heap-allocated struct to support full 64-bit values.
 * This allows Option to contain pointers (strings, lists, etc.).
 *
 * FernOption {
 *   tag: int32_t (0 = None, 1 = Some)
 *   value: int64_t (the some value, can be a pointer)
 * }
 *
 * The Option is passed around as a pointer (int64_t).
 * None is represented as NULL (0).
 */

typedef struct {
    int32_t tag;
    int64_t value;
} FernOption;

#define OPTION_TAG_NONE 0
#define OPTION_TAG_SOME 1

/**
 * Create a Some option.
 * @param value The contained value (must fit in 32 bits).
 * @return Packed 64-bit value: tag (1) in lower 32 bits, value in upper 32 bits.
 *
 * Format: [value:32][tag:32] where tag=1 for Some
 * This matches the codegen's expected Option representation.
 */
int64_t fern_option_some(int64_t value) {
    /* Pack: value in upper 32 bits, tag=1 in lower 32 bits */
    return ((value & 0xFFFFFFFF) << 32) | 1;
}

/**
 * Create a None option.
 * @return Packed value with tag=0.
 *
 * Format: [0:32][tag:32] where tag=0 for None
 */
int64_t fern_option_none(void) {
    /* Tag = 0 for None */
    return 0;
}

/**
 * Check if an Option is Some.
 * @param option Packed Option value.
 * @return 1 if Some (tag=1), 0 if None (tag=0).
 */
int64_t fern_option_is_some(int64_t option) {
    /* Check if lower 32 bits (tag) == 1 */
    return (option & 0xFFFFFFFF) == OPTION_TAG_SOME ? 1 : 0;
}

/**
 * Unwrap the value from an Option.
 * @param option Packed Option value.
 * @return The contained value (upper 32 bits).
 */
int64_t fern_option_unwrap(int64_t option) {
    assert((option & 0xFFFFFFFF) == OPTION_TAG_SOME);  /* Should not unwrap None */
    /* Extract upper 32 bits as the value */
    return (option >> 32) & 0xFFFFFFFF;
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
    char* contents = FERN_ALLOC((size_t)size + 1);
    if (!contents) {
        fclose(f);
        return fern_result_err(FERN_ERR_OUT_OF_MEMORY);
    }
    
    /* Read file contents */
    size_t read_size = fread(contents, 1, (size_t)size, f);
    fclose(f);
    
    if ((long)read_size != size) {
        FERN_FREE(contents);
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

/**
 * Check if path is a directory.
 * @param path The path to check.
 * @return 1 if directory, 0 otherwise.
 */
int64_t fern_is_dir(const char* path) {
    assert(path != NULL);
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

/**
 * List directory contents.
 * @param path The directory path.
 * @return FernStringList containing entry names, or NULL on error.
 */
FernStringList* fern_list_dir(const char* path) {
    assert(path != NULL);
    
    DIR* dir = opendir(path);
    if (!dir) {
        return NULL;
    }
    
    FernStringList* list = FERN_ALLOC(sizeof(FernStringList));
    assert(list != NULL);
    list->cap = 16;
    list->len = 0;
    list->data = FERN_ALLOC((size_t)list->cap * sizeof(char*));
    assert(list->data != NULL);
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        /* Grow if needed */
        if (list->len >= list->cap) {
            list->cap *= 2;
            list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
            assert(list->data != NULL);
        }
        
        /* Copy entry name */
        size_t name_len = strlen(entry->d_name);
        char* name = FERN_ALLOC(name_len + 1);
        assert(name != NULL);
        memcpy(name, entry->d_name, name_len + 1);
        list->data[list->len++] = name;
    }
    
    closedir(dir);
    return list;
}

/* ========== Gate C Stdlib Runtime Surface ========== */

/**
 * Parse JSON text.
 * @param text UTF-8 JSON text.
 * @return Result: Ok(copy of text) or Err(error code).
 */
int64_t fern_json_parse(const char* text) {
    if (text == NULL || text[0] == '\0') {
        return fern_result_err(FERN_ERR_IO);
    }
    char* copy = FERN_STRDUP(text);
    if (copy == NULL) {
        return fern_result_err(FERN_ERR_OUT_OF_MEMORY);
    }
    return fern_result_ok((int64_t)(intptr_t)copy);
}

/**
 * Encode value as JSON text.
 * @param text Input text payload.
 * @return Result: Ok(copy of text) or Err(error code).
 */
int64_t fern_json_stringify(const char* text) {
    if (text == NULL) {
        return fern_result_err(FERN_ERR_IO);
    }
    char* copy = FERN_STRDUP(text);
    if (copy == NULL) {
        return fern_result_err(FERN_ERR_OUT_OF_MEMORY);
    }
    return fern_result_ok((int64_t)(intptr_t)copy);
}

/**
 * Perform an HTTP GET request (placeholder).
 * @param url Request URL.
 * @return Result: Err(error code).
 */
int64_t fern_http_get(const char* url) {
    if (url == NULL || url[0] == '\0') {
        return fern_result_err(FERN_ERR_IO);
    }
    return fern_result_err(FERN_ERR_IO);
}

/**
 * Perform an HTTP POST request (placeholder).
 * @param url Request URL.
 * @param body Request body payload.
 * @return Result: Err(error code).
 */
int64_t fern_http_post(const char* url, const char* body) {
    if (url == NULL || url[0] == '\0' || body == NULL) {
        return fern_result_err(FERN_ERR_IO);
    }
    return fern_result_err(FERN_ERR_IO);
}

/**
 * Open a SQL connection (placeholder).
 * @param path Database path/URL.
 * @return Result: Err(error code).
 */
int64_t fern_sql_open(const char* path) {
    if (path == NULL || path[0] == '\0') {
        return fern_result_err(FERN_ERR_IO);
    }
    return fern_result_err(FERN_ERR_IO);
}

/**
 * Execute SQL against a connection (placeholder).
 * @param handle SQL handle.
 * @param query SQL statement.
 * @return Result: Err(error code).
 */
int64_t fern_sql_execute(int64_t handle, const char* query) {
    if (query == NULL || handle < 0) {
        return fern_result_err(FERN_ERR_IO);
    }
    return fern_result_err(FERN_ERR_IO);
}

/**
 * Start an actor and return its id.
 * @param name Actor name.
 * @return Actor id.
 */
int64_t fern_actor_start(const char* name) {
    static int64_t next_actor_id = 1;
    assert(name != NULL);
    assert(next_actor_id > 0);
    return next_actor_id++;
}

/**
 * Post a message to an actor.
 * @param actor_id Destination actor id.
 * @param msg Message payload.
 * @return Result: Ok(0) or Err(error code).
 */
int64_t fern_actor_post(int64_t actor_id, const char* msg) {
    assert(actor_id > 0);
    assert(msg != NULL);
    (void)msg;
    return fern_result_ok(0);
}

/**
 * Receive the next actor message (placeholder).
 * @param actor_id Actor id.
 * @return Result: Err(error code).
 */
int64_t fern_actor_next(int64_t actor_id) {
    assert(actor_id > 0);
    assert(actor_id < INT64_MAX);
    return fern_result_err(FERN_ERR_IO);
}

/* ========== Regex Functions ========== */

/**
 * Check if string matches regex pattern.
 * @param s The string to match.
 * @param pattern The regex pattern.
 * @return 1 if matches, 0 otherwise.
 */
int64_t fern_regex_is_match(const char* s, const char* pattern) {
    assert(s != NULL);
    assert(pattern != NULL);
    
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
    if (ret != 0) {
        return 0; /* Invalid pattern */
    }
    
    ret = regexec(&regex, s, 0, NULL, 0);
    regfree(&regex);
    
    return (ret == 0) ? 1 : 0;
}

/**
 * Find first match of regex in string.
 * @param s The string to search.
 * @param pattern The regex pattern.
 * @return FernRegexMatch with match info (start=-1 if no match).
 */
FernRegexMatch* fern_regex_find(const char* s, const char* pattern) {
    assert(s != NULL);
    assert(pattern != NULL);
    
    FernRegexMatch* result = FERN_ALLOC(sizeof(FernRegexMatch));
    assert(result != NULL);
    result->start = -1;
    result->end = -1;
    result->matched = NULL;
    
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        return result; /* Invalid pattern */
    }
    
    regmatch_t match;
    ret = regexec(&regex, s, 1, &match, 0);
    regfree(&regex);
    
    if (ret == 0) {
        result->start = match.rm_so;
        result->end = match.rm_eo;
        size_t len = (size_t)(match.rm_eo - match.rm_so);
        result->matched = FERN_ALLOC(len + 1);
        assert(result->matched != NULL);
        memcpy(result->matched, s + match.rm_so, len);
        result->matched[len] = '\0';
    }
    
    return result;
}

/**
 * Find all matches of regex in string.
 * @param s The string to search.
 * @param pattern The regex pattern.
 * @return FernStringList of all matched substrings.
 */
FernStringList* fern_regex_find_all(const char* s, const char* pattern) {
    assert(s != NULL);
    assert(pattern != NULL);
    
    FernStringList* list = FERN_ALLOC(sizeof(FernStringList));
    assert(list != NULL);
    list->cap = 8;
    list->len = 0;
    list->data = FERN_ALLOC((size_t)list->cap * sizeof(char*));
    assert(list->data != NULL);
    
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        return list; /* Invalid pattern, return empty list */
    }
    
    const char* p = s;
    regmatch_t match;
    
    while (regexec(&regex, p, 1, &match, 0) == 0) {
        /* Grow if needed */
        if (list->len >= list->cap) {
            list->cap *= 2;
            list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
            assert(list->data != NULL);
        }
        
        /* Copy matched substring */
        size_t len = (size_t)(match.rm_eo - match.rm_so);
        char* matched = FERN_ALLOC(len + 1);
        assert(matched != NULL);
        memcpy(matched, p + match.rm_so, len);
        matched[len] = '\0';
        list->data[list->len++] = matched;
        
        /* Move past this match */
        p += match.rm_eo;
        if (*p == '\0') break;
        
        /* Avoid infinite loop on zero-length matches */
        if (match.rm_eo == match.rm_so) {
            p++;
            if (*p == '\0') break;
        }
    }
    
    regfree(&regex);
    return list;
}

/**
 * Replace first match of regex with replacement.
 * @param s The string to modify.
 * @param pattern The regex pattern.
 * @param replacement The replacement string.
 * @return New string with replacement applied.
 */
char* fern_regex_replace(const char* s, const char* pattern, const char* replacement) {
    assert(s != NULL);
    assert(pattern != NULL);
    assert(replacement != NULL);
    
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        /* Invalid pattern, return copy of original */
        return FERN_STRDUP(s);
    }
    
    regmatch_t match;
    ret = regexec(&regex, s, 1, &match, 0);
    regfree(&regex);
    
    if (ret != 0) {
        /* No match, return copy of original */
        return FERN_STRDUP(s);
    }
    
    /* Build result: prefix + replacement + suffix */
    size_t prefix_len = (size_t)match.rm_so;
    size_t repl_len = strlen(replacement);
    size_t suffix_len = strlen(s + match.rm_eo);
    size_t total_len = prefix_len + repl_len + suffix_len;
    
    char* result = FERN_ALLOC(total_len + 1);
    assert(result != NULL);
    
    memcpy(result, s, prefix_len);
    memcpy(result + prefix_len, replacement, repl_len);
    memcpy(result + prefix_len + repl_len, s + match.rm_eo, suffix_len + 1);
    
    return result;
}

/**
 * Replace all matches of regex with replacement.
 * @param s The string to modify.
 * @param pattern The regex pattern.
 * @param replacement The replacement string.
 * @return New string with all replacements applied.
 */
char* fern_regex_replace_all(const char* s, const char* pattern, const char* replacement) {
    assert(s != NULL);
    assert(pattern != NULL);
    assert(replacement != NULL);
    
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        return FERN_STRDUP(s);
    }
    
    /* Build result incrementally */
    size_t result_cap = strlen(s) * 2 + 1;
    size_t result_len = 0;
    char* result = FERN_ALLOC(result_cap);
    assert(result != NULL);
    
    const char* p = s;
    regmatch_t match;
    size_t repl_len = strlen(replacement);
    
    while (regexec(&regex, p, 1, &match, 0) == 0) {
        /* Append prefix (before match) */
        size_t prefix_len = (size_t)match.rm_so;
        if (result_len + prefix_len + repl_len >= result_cap) {
            result_cap = (result_len + prefix_len + repl_len) * 2;
            result = FERN_REALLOC(result, result_cap);
            assert(result != NULL);
        }
        memcpy(result + result_len, p, prefix_len);
        result_len += prefix_len;
        
        /* Append replacement */
        memcpy(result + result_len, replacement, repl_len);
        result_len += repl_len;
        
        /* Move past this match */
        p += match.rm_eo;
        
        /* Avoid infinite loop on zero-length matches */
        if (match.rm_eo == match.rm_so) {
            if (*p == '\0') break;
            /* Copy one char and move on */
            if (result_len + 1 >= result_cap) {
                result_cap *= 2;
                result = FERN_REALLOC(result, result_cap);
                assert(result != NULL);
            }
            result[result_len++] = *p++;
        }
    }
    
    /* Append remaining suffix */
    size_t suffix_len = strlen(p);
    if (result_len + suffix_len >= result_cap) {
        result_cap = result_len + suffix_len + 1;
        result = FERN_REALLOC(result, result_cap);
        assert(result != NULL);
    }
    memcpy(result + result_len, p, suffix_len + 1);
    
    regfree(&regex);
    return result;
}

/**
 * Split string by regex pattern.
 * @param s The string to split.
 * @param pattern The regex pattern to split on.
 * @return FernStringList of parts.
 */
FernStringList* fern_regex_split(const char* s, const char* pattern) {
    assert(s != NULL);
    assert(pattern != NULL);
    
    FernStringList* list = FERN_ALLOC(sizeof(FernStringList));
    assert(list != NULL);
    list->cap = 8;
    list->len = 0;
    list->data = FERN_ALLOC((size_t)list->cap * sizeof(char*));
    assert(list->data != NULL);
    
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        /* Invalid pattern, return original as single element */
        list->data[0] = FERN_STRDUP(s);
        list->len = 1;
        return list;
    }
    
    const char* p = s;
    regmatch_t match;
    
    while (regexec(&regex, p, 1, &match, 0) == 0) {
        /* Grow if needed */
        if (list->len >= list->cap) {
            list->cap *= 2;
            list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
            assert(list->data != NULL);
        }
        
        /* Copy part before match */
        size_t len = (size_t)match.rm_so;
        char* part = FERN_ALLOC(len + 1);
        assert(part != NULL);
        memcpy(part, p, len);
        part[len] = '\0';
        list->data[list->len++] = part;
        
        /* Move past this match */
        p += match.rm_eo;
        
        /* Avoid infinite loop on zero-length matches */
        if (match.rm_eo == match.rm_so) {
            if (*p == '\0') break;
            p++;
        }
    }
    
    /* Add remaining part */
    if (list->len >= list->cap) {
        list->cap *= 2;
        list->data = FERN_REALLOC(list->data, (size_t)list->cap * sizeof(char*));
        assert(list->data != NULL);
    }
    list->data[list->len++] = FERN_STRDUP(p);
    
    regfree(&regex);
    return list;
}

/**
 * Find match with capture groups.
 * @param s The string to search.
 * @param pattern The regex pattern with groups.
 * @return FernRegexCaptures with all capture groups.
 */
FernRegexCaptures* fern_regex_captures(const char* s, const char* pattern) {
    assert(s != NULL);
    assert(pattern != NULL);
    
    FernRegexCaptures* result = FERN_ALLOC(sizeof(FernRegexCaptures));
    assert(result != NULL);
    result->count = 0;
    result->captures = NULL;
    
    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        return result;
    }
    
    /* POSIX regex supports up to 9 subexpressions + full match */
    size_t max_groups = 10;
    regmatch_t* matches = FERN_ALLOC(max_groups * sizeof(regmatch_t));
    assert(matches != NULL);
    
    ret = regexec(&regex, s, max_groups, matches, 0);
    if (ret != 0) {
        regfree(&regex);
        FERN_FREE(matches);
        return result;
    }
    
    /* Count valid matches */
    size_t count = 0;
    for (size_t i = 0; i < max_groups; i++) {
        if (matches[i].rm_so == -1) break;
        count++;
    }
    
    result->count = (int64_t)count;
    result->captures = FERN_ALLOC(count * sizeof(FernRegexMatch));
    assert(result->captures != NULL);
    
    for (size_t i = 0; i < count; i++) {
        result->captures[i].start = matches[i].rm_so;
        result->captures[i].end = matches[i].rm_eo;
        
        size_t len = (size_t)(matches[i].rm_eo - matches[i].rm_so);
        result->captures[i].matched = FERN_ALLOC(len + 1);
        assert(result->captures[i].matched != NULL);
        memcpy(result->captures[i].matched, s + matches[i].rm_so, len);
        result->captures[i].matched[len] = '\0';
    }
    
    regfree(&regex);
    FERN_FREE(matches);
    return result;
}

/**
 * Free a regex match result.
 * @param match The match to free.
 */
void fern_regex_match_FERN_FREE(FernRegexMatch* match) {
    if (match) {
        FERN_FREE(match->matched);
        FERN_FREE(match);
    }
}

/**
 * Free regex captures.
 * @param captures The captures to free.
 */
void fern_regex_captures_FERN_FREE(FernRegexCaptures* captures) {
    if (captures) {
        for (int64_t i = 0; i < captures->count; i++) {
            FERN_FREE(captures->captures[i].matched);
        }
        FERN_FREE(captures->captures);
        FERN_FREE(captures);
    }
}

/* ========== TUI: Term Module ========== */

#include <sys/ioctl.h>
#include <termios.h>

/**
 * Get terminal dimensions.
 * @return FernTermSize with columns and rows.
 */
FernTermSize* fern_term_size(void) {
    FernTermSize* size = FERN_ALLOC(sizeof(FernTermSize));
    if (!size) return NULL;
    
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        size->cols = ws.ws_col;
        size->rows = ws.ws_row;
    } else {
        /* Default fallback */
        size->cols = 80;
        size->rows = 24;
    }
    return size;
}

/**
 * Check if stdout is a TTY.
 * @return 1 if TTY, 0 otherwise.
 */
int64_t fern_term_is_tty(void) {
    return isatty(STDOUT_FILENO) ? 1 : 0;
}

/**
 * Get color support level.
 * Checks COLORTERM, TERM, and NO_COLOR environment variables.
 * @return 0 (no color), 16 (basic), 256 (extended), 16777216 (truecolor).
 */
int64_t fern_term_color_support(void) {
    /* NO_COLOR takes precedence */
    const char* no_color = getenv("NO_COLOR");
    if (no_color && no_color[0] != '\0') {
        return 0;
    }
    
    /* Not a TTY means no color */
    if (!isatty(STDOUT_FILENO)) {
        return 0;
    }
    
    /* Check for truecolor support */
    const char* colorterm = getenv("COLORTERM");
    if (colorterm) {
        if (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0) {
            return 16777216;
        }
    }
    
    /* Check TERM for 256-color support */
    const char* term = getenv("TERM");
    if (term) {
        if (strstr(term, "256color") || strstr(term, "256")) {
            return 256;
        }
        if (strstr(term, "color") || strstr(term, "xterm") || 
            strstr(term, "screen") || strstr(term, "vt100")) {
            return 16;
        }
    }
    
    /* Default to basic 16 colors if TTY */
    return 16;
}

/* ========== TUI: Style Module ========== */

/* ANSI escape code constants */
#define ANSI_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_DIM "\x1b[2m"
#define ANSI_ITALIC "\x1b[3m"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_BLINK "\x1b[5m"
#define ANSI_REVERSE "\x1b[7m"
#define ANSI_STRIKETHROUGH "\x1b[9m"

/* Foreground colors */
#define ANSI_FG_BLACK "\x1b[30m"
#define ANSI_FG_RED "\x1b[31m"
#define ANSI_FG_GREEN "\x1b[32m"
#define ANSI_FG_YELLOW "\x1b[33m"
#define ANSI_FG_BLUE "\x1b[34m"
#define ANSI_FG_MAGENTA "\x1b[35m"
#define ANSI_FG_CYAN "\x1b[36m"
#define ANSI_FG_WHITE "\x1b[37m"

/* Bright foreground colors */
#define ANSI_FG_BRIGHT_BLACK "\x1b[90m"
#define ANSI_FG_BRIGHT_RED "\x1b[91m"
#define ANSI_FG_BRIGHT_GREEN "\x1b[92m"
#define ANSI_FG_BRIGHT_YELLOW "\x1b[93m"
#define ANSI_FG_BRIGHT_BLUE "\x1b[94m"
#define ANSI_FG_BRIGHT_MAGENTA "\x1b[95m"
#define ANSI_FG_BRIGHT_CYAN "\x1b[96m"
#define ANSI_FG_BRIGHT_WHITE "\x1b[97m"

/* Background colors */
#define ANSI_BG_BLACK "\x1b[40m"
#define ANSI_BG_RED "\x1b[41m"
#define ANSI_BG_GREEN "\x1b[42m"
#define ANSI_BG_YELLOW "\x1b[43m"
#define ANSI_BG_BLUE "\x1b[44m"
#define ANSI_BG_MAGENTA "\x1b[45m"
#define ANSI_BG_CYAN "\x1b[46m"
#define ANSI_BG_WHITE "\x1b[47m"

/* ========== Fern Color Palette ========== */
/* Centralized color definitions for TUI components.
 * These can be customized to change the look of Status badges, etc.
 * 256-color codes use format: \x1b[38;5;Nm (fg) or \x1b[48;5;Nm (bg)
 * RGB colors use format: \x1b[38;2;R;G;Bm (fg) or \x1b[48;2;R;G;Bm (bg)
 */

/* Status badge colors - background */
#define FERN_COLOR_WARN_BG   "\x1b[48;5;214m"  /* Orange (256-color) */
#define FERN_COLOR_OKAY_BG   "\x1b[42m"        /* Green */
#define FERN_COLOR_INFO_BG   "\x1b[44m"        /* Blue */
#define FERN_COLOR_FAIL_BG   "\x1b[41m"        /* Red */
#define FERN_COLOR_DBUG_BG   "\x1b[45m"        /* Magenta */

/* Status badge colors - foreground (text) */
#define FERN_COLOR_WARN_FG   "\x1b[30m"        /* Black */
#define FERN_COLOR_OKAY_FG   "\x1b[30m"        /* Black */
#define FERN_COLOR_INFO_FG   "\x1b[37m"        /* White */
#define FERN_COLOR_FAIL_FG   "\x1b[37m"        /* White */
#define FERN_COLOR_DBUG_FG   "\x1b[37m"        /* White */

/* Panel border colors (defaults, can be overridden per-panel) */
#define FERN_COLOR_PANEL_BORDER "\x1b[36m"    /* Cyan */

/**
 * Helper to wrap text with ANSI escape codes.
 * @param prefix The ANSI escape sequence to apply.
 * @param text The text to wrap.
 * @return New string with escape codes.
 */
static char* style_wrap(const char* prefix, const char* text) {
    if (!text) return NULL;
    size_t prefix_len = strlen(prefix);
    size_t text_len = strlen(text);
    size_t reset_len = strlen(ANSI_RESET);
    
    char* result = FERN_ALLOC(prefix_len + text_len + reset_len + 1);
    if (!result) return NULL;
    
    memcpy(result, prefix, prefix_len);
    memcpy(result + prefix_len, text, text_len);
    memcpy(result + prefix_len + text_len, ANSI_RESET, reset_len);
    result[prefix_len + text_len + reset_len] = '\0';
    
    return result;
}

/* Basic colors (foreground) */
char* fern_style_black(const char* text) { return style_wrap(ANSI_FG_BLACK, text); }
char* fern_style_red(const char* text) { return style_wrap(ANSI_FG_RED, text); }
char* fern_style_green(const char* text) { return style_wrap(ANSI_FG_GREEN, text); }
char* fern_style_yellow(const char* text) { return style_wrap(ANSI_FG_YELLOW, text); }
char* fern_style_blue(const char* text) { return style_wrap(ANSI_FG_BLUE, text); }
char* fern_style_magenta(const char* text) { return style_wrap(ANSI_FG_MAGENTA, text); }
char* fern_style_cyan(const char* text) { return style_wrap(ANSI_FG_CYAN, text); }
char* fern_style_white(const char* text) { return style_wrap(ANSI_FG_WHITE, text); }

/* Bright colors (foreground) */
char* fern_style_bright_black(const char* text) { return style_wrap(ANSI_FG_BRIGHT_BLACK, text); }
char* fern_style_bright_red(const char* text) { return style_wrap(ANSI_FG_BRIGHT_RED, text); }
char* fern_style_bright_green(const char* text) { return style_wrap(ANSI_FG_BRIGHT_GREEN, text); }
char* fern_style_bright_yellow(const char* text) { return style_wrap(ANSI_FG_BRIGHT_YELLOW, text); }
char* fern_style_bright_blue(const char* text) { return style_wrap(ANSI_FG_BRIGHT_BLUE, text); }
char* fern_style_bright_magenta(const char* text) { return style_wrap(ANSI_FG_BRIGHT_MAGENTA, text); }
char* fern_style_bright_cyan(const char* text) { return style_wrap(ANSI_FG_BRIGHT_CYAN, text); }
char* fern_style_bright_white(const char* text) { return style_wrap(ANSI_FG_BRIGHT_WHITE, text); }

/* Background colors */
char* fern_style_on_black(const char* text) { return style_wrap(ANSI_BG_BLACK, text); }
char* fern_style_on_red(const char* text) { return style_wrap(ANSI_BG_RED, text); }
char* fern_style_on_green(const char* text) { return style_wrap(ANSI_BG_GREEN, text); }
char* fern_style_on_yellow(const char* text) { return style_wrap(ANSI_BG_YELLOW, text); }
char* fern_style_on_blue(const char* text) { return style_wrap(ANSI_BG_BLUE, text); }
char* fern_style_on_magenta(const char* text) { return style_wrap(ANSI_BG_MAGENTA, text); }
char* fern_style_on_cyan(const char* text) { return style_wrap(ANSI_BG_CYAN, text); }
char* fern_style_on_white(const char* text) { return style_wrap(ANSI_BG_WHITE, text); }

/* Text attributes */
char* fern_style_bold(const char* text) { return style_wrap(ANSI_BOLD, text); }
char* fern_style_dim(const char* text) { return style_wrap(ANSI_DIM, text); }
char* fern_style_italic(const char* text) { return style_wrap(ANSI_ITALIC, text); }
char* fern_style_underline(const char* text) { return style_wrap(ANSI_UNDERLINE, text); }
char* fern_style_blink(const char* text) { return style_wrap(ANSI_BLINK, text); }
char* fern_style_reverse(const char* text) { return style_wrap(ANSI_REVERSE, text); }
char* fern_style_strikethrough(const char* text) { return style_wrap(ANSI_STRIKETHROUGH, text); }

/**
 * Apply 256-color foreground.
 * @param text The text to style.
 * @param color_code Color index 0-255.
 * @return Styled string.
 */
char* fern_style_color(const char* text, int64_t color_code) {
    if (!text) return NULL;
    if (color_code < 0) color_code = 0;
    if (color_code > 255) color_code = 255;
    
    char prefix[20];
    snprintf(prefix, sizeof(prefix), "\x1b[38;5;%ldm", (long)color_code);
    return style_wrap(prefix, text);
}

/**
 * Apply 256-color background.
 * @param text The text to style.
 * @param color_code Color index 0-255.
 * @return Styled string.
 */
char* fern_style_on_color(const char* text, int64_t color_code) {
    if (!text) return NULL;
    if (color_code < 0) color_code = 0;
    if (color_code > 255) color_code = 255;
    
    char prefix[20];
    snprintf(prefix, sizeof(prefix), "\x1b[48;5;%ldm", (long)color_code);
    return style_wrap(prefix, text);
}

/**
 * Apply RGB foreground color.
 * @param text The text to style.
 * @param r Red component 0-255.
 * @param g Green component 0-255.
 * @param b Blue component 0-255.
 * @return Styled string.
 */
char* fern_style_rgb(const char* text, int64_t r, int64_t g, int64_t b) {
    if (!text) return NULL;
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    char prefix[30];
    snprintf(prefix, sizeof(prefix), "\x1b[38;2;%ld;%ld;%ldm", (long)r, (long)g, (long)b);
    return style_wrap(prefix, text);
}

/**
 * Apply RGB background color.
 * @param text The text to style.
 * @param r Red component 0-255.
 * @param g Green component 0-255.
 * @param b Blue component 0-255.
 * @return Styled string.
 */
char* fern_style_on_rgb(const char* text, int64_t r, int64_t g, int64_t b) {
    if (!text) return NULL;
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    
    char prefix[30];
    snprintf(prefix, sizeof(prefix), "\x1b[48;2;%ld;%ld;%ldm", (long)r, (long)g, (long)b);
    return style_wrap(prefix, text);
}

/**
 * Parse hex color string to RGB.
 * @param hex Hex color string like "#ff8800" or "ff8800".
 * @param r Output red component.
 * @param g Output green component.
 * @param b Output blue component.
 * @return 1 on success, 0 on failure.
 */
static int parse_hex_color(const char* hex, int* r, int* g, int* b) {
    if (!hex) return 0;
    
    /* Skip leading # if present */
    if (hex[0] == '#') hex++;
    
    if (strlen(hex) != 6) return 0;
    
    unsigned int rv, gv, bv;
    if (sscanf(hex, "%2x%2x%2x", &rv, &gv, &bv) != 3) return 0;
    
    *r = (int)rv;
    *g = (int)gv;
    *b = (int)bv;
    return 1;
}

/**
 * Apply hex foreground color.
 * @param text The text to style.
 * @param hex_color Hex color string like "#ff8800".
 * @return Styled string.
 */
char* fern_style_hex(const char* text, const char* hex_color) {
    int r, g, b;
    if (!parse_hex_color(hex_color, &r, &g, &b)) {
        /* Invalid hex, return unstyled copy */
        return FERN_STRDUP(text ? text : "");
    }
    return fern_style_rgb(text, r, g, b);
}

/**
 * Apply hex background color.
 * @param text The text to style.
 * @param hex_color Hex color string like "#ff8800".
 * @return Styled string.
 */
char* fern_style_on_hex(const char* text, const char* hex_color) {
    int r, g, b;
    if (!parse_hex_color(hex_color, &r, &g, &b)) {
        /* Invalid hex, return unstyled copy */
        return FERN_STRDUP(text ? text : "");
    }
    return fern_style_on_rgb(text, r, g, b);
}

/**
 * Strip ANSI escape codes from text.
 * @param text The text to strip.
 * @return New string with escape codes removed.
 */
char* fern_style_reset(const char* text) {
    if (!text) return NULL;
    
    size_t len = strlen(text);
    char* result = FERN_ALLOC(len + 1);
    if (!result) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == '\x1b') {
            /* Skip escape sequence: \x1b[...m */
            while (i < len && text[i] != 'm') i++;
        } else {
            result[j++] = text[i];
        }
    }
    result[j] = '\0';
    
    return result;
}

/* ========== TUI: Status Badges ========== */

/**
 * Create a status badge with background color and text color.
 * All badges are 8 visible chars: space + 6-char centered label + space.
 * @param label The badge text (e.g., "WARN", "OK").
 * @param bg_color Background color code.
 * @param fg_color Foreground (text) color code.
 * @return Formatted badge string with colors.
 */
static char* make_badge(const char* label, const char* bg_color, const char* fg_color) {
    if (!label) return FERN_STRDUP("");
    
    /* All labels are 4 chars: WARN, OKAY, INFO, FAIL, DBUG */
    const size_t inner_width = 4;
    size_t label_len = strlen(label);
    size_t total_pad = inner_width - label_len;
    size_t pad_left = total_pad / 2;  /* Round down - extra space goes right */
    size_t pad_right = total_pad - pad_left;
    
    size_t bg_len = bg_color ? strlen(bg_color) : 0;
    size_t fg_len = fg_color ? strlen(fg_color) : 0;
    size_t bold_len = strlen(ANSI_BOLD);
    size_t reset_len = strlen(ANSI_RESET);
    
    /* Total: colors + bold + outer_space + pad_left + label + pad_right + outer_space + reset + null */
    size_t total = bg_len + fg_len + bold_len + 1 + pad_left + label_len + pad_right + 1 + reset_len + 1;
    char* result = FERN_ALLOC(total);
    if (!result) return FERN_STRDUP("");
    
    char* p = result;
    if (bg_color) { memcpy(p, bg_color, bg_len); p += bg_len; }
    if (fg_color) { memcpy(p, fg_color, fg_len); p += fg_len; }
    memcpy(p, ANSI_BOLD, bold_len); p += bold_len;
    *p++ = ' ';
    for (size_t i = 0; i < pad_left; i++) *p++ = ' ';
    memcpy(p, label, label_len); p += label_len;
    for (size_t i = 0; i < pad_right; i++) *p++ = ' ';
    *p++ = ' ';
    memcpy(p, ANSI_RESET, reset_len); p += reset_len;
    *p = '\0';
    
    return result;
}

/**
 * Create a WARN badge (yellow/orange background, black text).
 * @param message Optional message to append after badge.
 * @return Formatted string like "[WARN] message".
 */
char* fern_status_warn(const char* message) {
    char* badge = make_badge("WARN", FERN_COLOR_WARN_BG, FERN_COLOR_WARN_FG);
    if (!message || message[0] == '\0') return badge;
    
    size_t badge_len = strlen(badge);
    size_t msg_len = strlen(message);
    char* result = FERN_ALLOC(badge_len + 1 + msg_len + 1);
    if (!result) { FERN_FREE(badge); return FERN_STRDUP(""); }
    
    memcpy(result, badge, badge_len);
    result[badge_len] = ' ';
    memcpy(result + badge_len + 1, message, msg_len);
    result[badge_len + 1 + msg_len] = '\0';
    
    FERN_FREE(badge);
    return result;
}

/**
 * Create an OK badge (green background, black text).
 * @param message Optional message to append after badge.
 * @return Formatted string like "[OK] message".
 */
char* fern_status_ok(const char* message) {
    char* badge = make_badge("OKAY", FERN_COLOR_OKAY_BG, FERN_COLOR_OKAY_FG);
    if (!message || message[0] == '\0') return badge;
    
    size_t badge_len = strlen(badge);
    size_t msg_len = strlen(message);
    char* result = FERN_ALLOC(badge_len + 1 + msg_len + 1);
    if (!result) { FERN_FREE(badge); return FERN_STRDUP(""); }
    
    memcpy(result, badge, badge_len);
    result[badge_len] = ' ';
    memcpy(result + badge_len + 1, message, msg_len);
    result[badge_len + 1 + msg_len] = '\0';
    
    FERN_FREE(badge);
    return result;
}

/**
 * Create an INFO badge (blue background, white text).
 * @param message Optional message to append after badge.
 * @return Formatted string like "[INFO] message".
 */
char* fern_status_info(const char* message) {
    char* badge = make_badge("INFO", FERN_COLOR_INFO_BG, FERN_COLOR_INFO_FG);
    if (!message || message[0] == '\0') return badge;
    
    size_t badge_len = strlen(badge);
    size_t msg_len = strlen(message);
    char* result = FERN_ALLOC(badge_len + 1 + msg_len + 1);
    if (!result) { FERN_FREE(badge); return FERN_STRDUP(""); }
    
    memcpy(result, badge, badge_len);
    result[badge_len] = ' ';
    memcpy(result + badge_len + 1, message, msg_len);
    result[badge_len + 1 + msg_len] = '\0';
    
    FERN_FREE(badge);
    return result;
}

/**
 * Create an ERROR badge (red background, white text).
 * @param message Optional message to append after badge.
 * @return Formatted string like "[ERROR] message".
 */
char* fern_status_error(const char* message) {
    char* badge = make_badge("FAIL", FERN_COLOR_FAIL_BG, FERN_COLOR_FAIL_FG);
    if (!message || message[0] == '\0') return badge;
    
    size_t badge_len = strlen(badge);
    size_t msg_len = strlen(message);
    char* result = FERN_ALLOC(badge_len + 1 + msg_len + 1);
    if (!result) { FERN_FREE(badge); return FERN_STRDUP(""); }
    
    memcpy(result, badge, badge_len);
    result[badge_len] = ' ';
    memcpy(result + badge_len + 1, message, msg_len);
    result[badge_len + 1 + msg_len] = '\0';
    
    FERN_FREE(badge);
    return result;
}

/**
 * Create a DEBUG badge (magenta background, white text).
 * @param message Optional message to append after badge.
 * @return Formatted string like "[DEBUG] message".
 */
char* fern_status_debug(const char* message) {
    char* badge = make_badge("DBUG", FERN_COLOR_DBUG_BG, FERN_COLOR_DBUG_FG);
    if (!message || message[0] == '\0') return badge;
    
    size_t badge_len = strlen(badge);
    size_t msg_len = strlen(message);
    char* result = FERN_ALLOC(badge_len + 1 + msg_len + 1);
    if (!result) { FERN_FREE(badge); return FERN_STRDUP(""); }
    
    memcpy(result, badge, badge_len);
    result[badge_len] = ' ';
    memcpy(result + badge_len + 1, message, msg_len);
    result[badge_len + 1 + msg_len] = '\0';
    
    FERN_FREE(badge);
    return result;
}

/* ========== TUI: Live/Spinner Module ========== */

/**
 * Print text without a newline (for same-line updates).
 * @param text The text to print.
 */
void fern_live_print(const char* text) {
    if (text) {
        printf("%s", text);
        fflush(stdout);
    }
}

/**
 * Clear the current line and move cursor to beginning.
 */
void fern_live_clear_line(void) {
    printf("\r\x1b[K");
    fflush(stdout);
}

/**
 * Update the current line with new text (clears line first).
 * @param text The new text to display.
 */
void fern_live_update(const char* text) {
    fern_live_clear_line();
    fern_live_print(text);
}

/**
 * Finish live update by printing newline.
 */
void fern_live_done(void) {
    printf("\n");
    fflush(stdout);
}

/**
 * Sleep for specified milliseconds.
 * @param ms Milliseconds to sleep.
 */
void fern_sleep_ms(int64_t ms) {
    if (ms > 0) {
        usleep((useconds_t)(ms * 1000));
    }
}

/* ========== TUI: Panel Module ========== */

/* Box drawing characters for each style */
typedef struct {
    const char* top_left;
    const char* top;
    const char* top_right;
    const char* left;
    const char* right;
    const char* bottom_left;
    const char* bottom;
    const char* bottom_right;
} BoxChars;

static const BoxChars BOX_CHARS[] = {
    /* BOX_ROUNDED */  { "╭", "─", "╮", "│", "│", "╰", "─", "╯" },
    /* BOX_SQUARE */   { "┌", "─", "┐", "│", "│", "└", "─", "┘" },
    /* BOX_DOUBLE */   { "╔", "═", "╗", "║", "║", "╚", "═", "╝" },
    /* BOX_HEAVY */    { "┏", "━", "┓", "┃", "┃", "┗", "━", "┛" },
    /* BOX_ASCII */    { "+", "-", "+", "|", "|", "+", "-", "+" },
    /* BOX_NONE */     { " ", " ", " ", " ", " ", " ", " ", " " },
};

/**
 * Get display width of a string (ignoring ANSI codes).
 * @param s The string.
 * @return Display width in characters.
 */
static size_t display_width(const char* s) {
    if (!s) return 0;
    size_t width = 0;
    size_t len = strlen(s);
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\n' || s[i] == '\r') {
            /* Don't count newlines as width */
            continue;
        } else if (s[i] == '\x1b') {
            /* Skip ANSI escape sequence */
            while (i < len && s[i] != 'm') i++;
        } else if ((unsigned char)s[i] >= 0x80) {
            /* UTF-8 multi-byte: skip continuation bytes */
            /* This is a simplification - assumes 1 display width per codepoint */
            if ((unsigned char)s[i] >= 0xC0) width++;
            /* Skip continuation bytes (0x80-0xBF) */
        } else {
            width++;
        }
    }
    return width;
}

/**
 * Get the maximum line width from a multi-line string.
 * @param s The string (may contain newlines).
 * @return Maximum display width of any line.
 */
static size_t max_line_width(const char* s) {
    if (!s) return 0;
    size_t max_width = 0;
    size_t current_width = 0;
    size_t len = strlen(s);
    
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\n') {
            if (current_width > max_width) max_width = current_width;
            current_width = 0;
        } else if (s[i] == '\r') {
            /* Skip carriage return */
            continue;
        } else if (s[i] == '\x1b') {
            /* Skip ANSI escape sequence */
            while (i < len && s[i] != 'm') i++;
        } else if ((unsigned char)s[i] >= 0x80) {
            if ((unsigned char)s[i] >= 0xC0) current_width++;
        } else {
            current_width++;
        }
    }
    /* Check last line (if no trailing newline) */
    if (current_width > max_width) max_width = current_width;
    
    return max_width;
}

/**
 * Repeat a string n times.
 * @param s The string to repeat.
 * @param n Number of repetitions.
 * @return New string.
 */
static char* str_repeat(const char* s, size_t n) {
    size_t slen = strlen(s);
    char* result = FERN_ALLOC(slen * n + 1);
    if (!result) return NULL;
    result[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        strcat(result, s);
    }
    return result;
}

/**
 * Pad string to width (left-aligned).
 * @param s The string.
 * @param width Target width.
 * @return Padded string.
 */
static char* pad_right(const char* s, size_t width) {
    size_t dw = display_width(s);
    if (dw >= width) return FERN_STRDUP(s);
    
    size_t padding = width - dw;
    size_t slen = strlen(s);
    char* result = FERN_ALLOC(slen + padding + 1);
    if (!result) return NULL;
    
    strcpy(result, s);
    for (size_t i = 0; i < padding; i++) {
        result[slen + i] = ' ';
    }
    result[slen + padding] = '\0';
    return result;
}

/**
 * Center string within width.
 * @param s The string.
 * @param width Target width.
 * @return Centered string.
 */
static char* center_text(const char* s, size_t width) {
    size_t dw = display_width(s);
    if (dw >= width) return FERN_STRDUP(s);
    
    size_t total_padding = width - dw;
    size_t left_pad = total_padding / 2;
    size_t right_pad = total_padding - left_pad;
    size_t slen = strlen(s);
    
    char* result = FERN_ALLOC(left_pad + slen + right_pad + 1);
    if (!result) return NULL;
    
    for (size_t i = 0; i < left_pad; i++) result[i] = ' ';
    strcpy(result + left_pad, s);
    for (size_t i = 0; i < right_pad; i++) result[left_pad + slen + i] = ' ';
    result[left_pad + slen + right_pad] = '\0';
    return result;
}

FernPanel* fern_panel_new(const char* content) {
    FernPanel* panel = FERN_ALLOC(sizeof(FernPanel));
    if (!panel) return NULL;
    
    panel->content = content ? FERN_STRDUP(content) : FERN_STRDUP("");
    panel->title = NULL;
    panel->subtitle = NULL;
    panel->border_color = NULL;
    panel->box_style = BOX_ROUNDED;
    panel->width = 0;  /* Auto */
    panel->padding_h = 1;
    panel->padding_v = 0;
    
    return panel;
}

FernPanel* fern_panel_title(FernPanel* panel, const char* title) {
    if (!panel) return NULL;
    if (panel->title) FERN_FREE(panel->title);
    panel->title = title ? FERN_STRDUP(title) : NULL;
    return panel;
}

FernPanel* fern_panel_subtitle(FernPanel* panel, const char* subtitle) {
    if (!panel) return NULL;
    if (panel->subtitle) FERN_FREE(panel->subtitle);
    panel->subtitle = subtitle ? FERN_STRDUP(subtitle) : NULL;
    return panel;
}

FernPanel* fern_panel_border(FernPanel* panel, int64_t style) {
    if (!panel) return NULL;
    if (style >= 0 && style <= 5) {
        panel->box_style = (FernBoxStyle)style;
    }
    return panel;
}

FernPanel* fern_panel_border_str(FernPanel* panel, const char* style) {
    if (!panel || !style) return panel;
    /* Convert style name to enum value */
    if (strcmp(style, "rounded") == 0) panel->box_style = BOX_ROUNDED;
    else if (strcmp(style, "square") == 0) panel->box_style = BOX_SQUARE;
    else if (strcmp(style, "double") == 0) panel->box_style = BOX_DOUBLE;
    else if (strcmp(style, "heavy") == 0) panel->box_style = BOX_HEAVY;
    else if (strcmp(style, "ascii") == 0) panel->box_style = BOX_ASCII;
    else if (strcmp(style, "none") == 0) panel->box_style = BOX_NONE;
    return panel;
}

FernPanel* fern_panel_width(FernPanel* panel, int64_t width) {
    if (!panel) return NULL;
    panel->width = width;
    return panel;
}

FernPanel* fern_panel_padding(FernPanel* panel, int64_t vertical, int64_t horizontal) {
    if (!panel) return NULL;
    panel->padding_v = vertical;
    panel->padding_h = horizontal;
    return panel;
}

FernPanel* fern_panel_border_color(FernPanel* panel, const char* color) {
    if (!panel) return NULL;
    FERN_FREE(panel->border_color);
    panel->border_color = color ? FERN_STRDUP(color) : NULL;
    return panel;
}

/**
 * Get ANSI color code for a color name or hex value.
 * @param color Color name or hex (e.g., "red", "cyan", "#00ff00").
 * @return ANSI escape sequence string (caller must free).
 */
static char* get_ansi_color(const char* color) {
    if (!color || color[0] == '\0') return FERN_STRDUP("");
    
    /* Handle hex colors */
    if (color[0] == '#' && strlen(color) == 7) {
        int r, g, b;
        if (sscanf(color + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            char* result = FERN_ALLOC(32);
            if (result) {
                snprintf(result, 32, "\x1b[38;2;%d;%d;%dm", r, g, b);
            }
            return result;
        }
    }
    
    /* Named colors */
    if (strcmp(color, "black") == 0) return FERN_STRDUP("\x1b[30m");
    if (strcmp(color, "red") == 0) return FERN_STRDUP("\x1b[31m");
    if (strcmp(color, "green") == 0) return FERN_STRDUP("\x1b[32m");
    if (strcmp(color, "yellow") == 0) return FERN_STRDUP("\x1b[33m");
    if (strcmp(color, "blue") == 0) return FERN_STRDUP("\x1b[34m");
    if (strcmp(color, "magenta") == 0) return FERN_STRDUP("\x1b[35m");
    if (strcmp(color, "cyan") == 0) return FERN_STRDUP("\x1b[36m");
    if (strcmp(color, "white") == 0) return FERN_STRDUP("\x1b[37m");
    /* Bright colors */
    if (strcmp(color, "bright_black") == 0) return FERN_STRDUP("\x1b[90m");
    if (strcmp(color, "bright_red") == 0) return FERN_STRDUP("\x1b[91m");
    if (strcmp(color, "bright_green") == 0) return FERN_STRDUP("\x1b[92m");
    if (strcmp(color, "bright_yellow") == 0) return FERN_STRDUP("\x1b[93m");
    if (strcmp(color, "bright_blue") == 0) return FERN_STRDUP("\x1b[94m");
    if (strcmp(color, "bright_magenta") == 0) return FERN_STRDUP("\x1b[95m");
    if (strcmp(color, "bright_cyan") == 0) return FERN_STRDUP("\x1b[96m");
    if (strcmp(color, "bright_white") == 0) return FERN_STRDUP("\x1b[97m");
    
    return FERN_STRDUP("");
}

char* fern_panel_render(FernPanel* panel) {
    /* FERN_STYLE: allow(function-length) panel rendering with multi-line support */
    if (!panel) return FERN_STRDUP("");
    
    const BoxChars* box = &BOX_CHARS[panel->box_style];
    
    /* Get border color codes */
    char* color_start = get_ansi_color(panel->border_color);
    const char* color_end = (color_start && color_start[0] != '\0') ? "\x1b[0m" : "";
    
    /* Calculate content width - use max line width for multi-line content */
    size_t content_width = max_line_width(panel->content);
    size_t title_width = panel->title ? display_width(panel->title) + 2 : 0;  /* +2 for spaces */
    size_t subtitle_width = panel->subtitle ? display_width(panel->subtitle) + 2 : 0;
    
    /* Determine panel width */
    size_t inner_width = content_width;
    if (title_width > inner_width) inner_width = title_width;
    if (subtitle_width > inner_width) inner_width = subtitle_width;
    inner_width += panel->padding_h * 2;
    
    if (panel->width > 0) {
        inner_width = (size_t)panel->width - 2;  /* -2 for borders */
    } else if (panel->width == -1) {
        /* Expand to terminal width */
        FernTermSize* size = fern_term_size();
        if (size) {
            inner_width = (size_t)size->cols - 2;
            FERN_FREE(size);
        }
    }
    
    /* Build result string */
    size_t result_cap = 8192;
    char* result = FERN_ALLOC(result_cap);
    if (!result) return FERN_STRDUP("");
    result[0] = '\0';
    
    /* Top border with optional title */
    strcat(result, color_start);
    strcat(result, box->top_left);
    if (panel->title && panel->title[0] != '\0') {
        size_t title_len = display_width(panel->title);
        size_t side_len = (inner_width - title_len - 2) / 2;
        char* side = str_repeat(box->top, side_len);
        strcat(result, side);
        strcat(result, color_end);
        strcat(result, " ");
        strcat(result, panel->title);
        strcat(result, " ");
        strcat(result, color_start);
        FERN_FREE(side);
        side = str_repeat(box->top, inner_width - side_len - title_len - 2);
        strcat(result, side);
        FERN_FREE(side);
    } else {
        char* top_line = str_repeat(box->top, inner_width);
        strcat(result, top_line);
        FERN_FREE(top_line);
    }
    strcat(result, box->top_right);
    strcat(result, color_end);
    strcat(result, "\n");
    
    /* Vertical padding (top) */
    for (int64_t i = 0; i < panel->padding_v; i++) {
        strcat(result, color_start);
        strcat(result, box->left);
        strcat(result, color_end);
        char* spaces = str_repeat(" ", inner_width);
        strcat(result, spaces);
        FERN_FREE(spaces);
        strcat(result, color_start);
        strcat(result, box->right);
        strcat(result, color_end);
        strcat(result, "\n");
    }
    
    /* Content lines - split by newlines and render each line */
    char* h_pad = str_repeat(" ", panel->padding_h);
    size_t line_width = inner_width - panel->padding_h * 2;
    
    /* Split content into lines */
    const char* content = panel->content;
    const char* line_start = content;
    const char* p = content;
    
    while (*p != '\0') {
        if (*p == '\n') {
            /* Render this line */
            size_t line_len = (size_t)(p - line_start);
            char* line = FERN_ALLOC(line_len + 1);
            if (line) {
                memcpy(line, line_start, line_len);
                line[line_len] = '\0';
                
                strcat(result, color_start);
                strcat(result, box->left);
                strcat(result, color_end);
                strcat(result, h_pad);
                char* padded_line = pad_right(line, line_width);
                strcat(result, padded_line);
                FERN_FREE(padded_line);
                strcat(result, h_pad);
                strcat(result, color_start);
                strcat(result, box->right);
                strcat(result, color_end);
                strcat(result, "\n");
                FERN_FREE(line);
            }
            line_start = p + 1;
        }
        p++;
    }
    
    /* Render last line (or only line if no newlines) */
    if (line_start <= p) {
        size_t line_len = (size_t)(p - line_start);
        if (line_len > 0 || line_start == content) {
            char* line = FERN_ALLOC(line_len + 1);
            if (line) {
                memcpy(line, line_start, line_len);
                line[line_len] = '\0';
                
                strcat(result, color_start);
                strcat(result, box->left);
                strcat(result, color_end);
                strcat(result, h_pad);
                char* padded_line = pad_right(line, line_width);
                strcat(result, padded_line);
                FERN_FREE(padded_line);
                strcat(result, h_pad);
                strcat(result, color_start);
                strcat(result, box->right);
                strcat(result, color_end);
                strcat(result, "\n");
                FERN_FREE(line);
            }
        }
    }
    
    FERN_FREE(h_pad);
    
    /* Vertical padding (bottom) */
    for (int64_t i = 0; i < panel->padding_v; i++) {
        strcat(result, color_start);
        strcat(result, box->left);
        strcat(result, color_end);
        char* spaces = str_repeat(" ", inner_width);
        strcat(result, spaces);
        FERN_FREE(spaces);
        strcat(result, color_start);
        strcat(result, box->right);
        strcat(result, color_end);
        strcat(result, "\n");
    }
    
    /* Bottom border with optional subtitle */
    strcat(result, color_start);
    strcat(result, box->bottom_left);
    if (panel->subtitle && panel->subtitle[0] != '\0') {
        size_t subtitle_len = display_width(panel->subtitle);
        size_t side_len = (inner_width - subtitle_len - 2) / 2;
        char* side = str_repeat(box->bottom, side_len);
        strcat(result, side);
        strcat(result, color_end);
        strcat(result, " ");
        strcat(result, panel->subtitle);
        strcat(result, " ");
        strcat(result, color_start);
        FERN_FREE(side);
        side = str_repeat(box->bottom, inner_width - side_len - subtitle_len - 2);
        strcat(result, side);
        FERN_FREE(side);
    } else {
        char* bottom_line = str_repeat(box->bottom, inner_width);
        strcat(result, bottom_line);
        FERN_FREE(bottom_line);
    }
    strcat(result, box->bottom_right);
    strcat(result, color_end);
    
    FERN_FREE(color_start);
    
    return result;
}

void fern_panel_FERN_FREE(FernPanel* panel) {
    if (panel) {
        FERN_FREE(panel->content);
        FERN_FREE(panel->title);
        FERN_FREE(panel->subtitle);
        FERN_FREE(panel->border_color);
        FERN_FREE(panel);
    }
}

/* ========== TUI: Table Module ========== */

FernTable* fern_table_new(void) {
    FernTable* table = FERN_ALLOC(sizeof(FernTable));
    if (!table) return NULL;
    
    table->title = NULL;
    table->caption = NULL;
    table->columns = NULL;
    table->column_count = 0;
    table->rows = NULL;
    table->row_count = 0;
    table->row_capacity = 0;
    table->box_style = BOX_ROUNDED;
    table->show_header = 1;
    table->show_lines = 0;
    table->expand = 0;
    
    return table;
}

FernTable* fern_table_add_column(FernTable* table, const char* header) {
    if (!table) return NULL;
    
    table->column_count++;
    table->columns = FERN_REALLOC(table->columns, table->column_count * sizeof(FernTableColumn));
    if (!table->columns) return NULL;
    
    FernTableColumn* col = &table->columns[table->column_count - 1];
    col->header = header ? FERN_STRDUP(header) : FERN_STRDUP("");
    col->min_width = 0;
    col->max_width = 0;
    col->justify = 0;  /* Left */
    
    return table;
}

FernTable* fern_table_add_row(FernTable* table, FernStringList* cells) {
    if (!table || !cells) return NULL;
    
    /* Grow rows array if needed */
    if (table->row_count >= table->row_capacity) {
        table->row_capacity = table->row_capacity == 0 ? 8 : table->row_capacity * 2;
        table->rows = FERN_REALLOC(table->rows, table->row_capacity * sizeof(FernTableRow));
        if (!table->rows) return NULL;
    }
    
    FernTableRow* row = &table->rows[table->row_count++];
    row->cell_count = cells->len;
    row->cells = FERN_ALLOC(cells->len * sizeof(char*));
    if (!row->cells) return NULL;
    
    for (int64_t i = 0; i < cells->len; i++) {
        row->cells[i] = cells->data[i] ? FERN_STRDUP(cells->data[i]) : FERN_STRDUP("");
    }
    
    return table;
}

FernTable* fern_table_title(FernTable* table, const char* title) {
    if (!table) return NULL;
    if (table->title) FERN_FREE(table->title);
    table->title = title ? FERN_STRDUP(title) : NULL;
    return table;
}

FernTable* fern_table_border(FernTable* table, int64_t style) {
    if (!table) return NULL;
    if (style >= 0 && style <= 5) {
        table->box_style = (FernBoxStyle)style;
    }
    return table;
}

FernTable* fern_table_show_header(FernTable* table, int64_t show) {
    if (!table) return NULL;
    table->show_header = show ? 1 : 0;
    return table;
}

char* fern_table_render(FernTable* table) {
    if (!table || table->column_count == 0) return FERN_STRDUP("");
    
    const BoxChars* box = &BOX_CHARS[table->box_style];
    
    /* Calculate column widths */
    size_t* col_widths = FERN_ALLOC(table->column_count * sizeof(size_t));
    if (!col_widths) return FERN_STRDUP("");
    
    for (int64_t i = 0; i < table->column_count; i++) {
        col_widths[i] = display_width(table->columns[i].header);
    }
    
    /* Find max width from rows */
    for (int64_t r = 0; r < table->row_count; r++) {
        FernTableRow* row = &table->rows[r];
        for (int64_t c = 0; c < row->cell_count && c < table->column_count; c++) {
            size_t w = display_width(row->cells[c]);
            if (w > col_widths[c]) col_widths[c] = w;
        }
    }
    
    /* Add padding */
    for (int64_t i = 0; i < table->column_count; i++) {
        col_widths[i] += 2;  /* Padding on each side */
    }
    
    /* Build result */
    size_t result_cap = 8192;
    char* result = FERN_ALLOC(result_cap);
    if (!result) { FERN_FREE(col_widths); return FERN_STRDUP(""); }
    result[0] = '\0';
    
    /* Top border */
    strcat(result, box->top_left);
    for (int64_t i = 0; i < table->column_count; i++) {
        char* line = str_repeat(box->top, col_widths[i]);
        strcat(result, line);
        FERN_FREE(line);
        if (i < table->column_count - 1) {
            /* Use top for separator (simplified) */
            strcat(result, box->top);
        }
    }
    strcat(result, box->top_right);
    strcat(result, "\n");
    
    /* Header row */
    if (table->show_header) {
        strcat(result, box->left);
        for (int64_t i = 0; i < table->column_count; i++) {
            char* centered = center_text(table->columns[i].header, col_widths[i]);
            strcat(result, centered);
            FERN_FREE(centered);
            if (i < table->column_count - 1) {
                strcat(result, box->left);
            }
        }
        strcat(result, box->right);
        strcat(result, "\n");
        
        /* Header separator */
        strcat(result, box->left);
        for (int64_t i = 0; i < table->column_count; i++) {
            char* line = str_repeat(box->top, col_widths[i]);
            strcat(result, line);
            FERN_FREE(line);
            if (i < table->column_count - 1) {
                strcat(result, box->top);
            }
        }
        strcat(result, box->right);
        strcat(result, "\n");
    }
    
    /* Data rows */
    for (int64_t r = 0; r < table->row_count; r++) {
        FernTableRow* row = &table->rows[r];
        strcat(result, box->left);
        for (int64_t c = 0; c < table->column_count; c++) {
            const char* cell = (c < row->cell_count) ? row->cells[c] : "";
            char* padded = pad_right(cell, col_widths[c] - 1);
            strcat(result, " ");
            strcat(result, padded);
            FERN_FREE(padded);
            if (c < table->column_count - 1) {
                strcat(result, box->left);
            }
        }
        strcat(result, box->right);
        strcat(result, "\n");
    }
    
    /* Bottom border */
    strcat(result, box->bottom_left);
    for (int64_t i = 0; i < table->column_count; i++) {
        char* line = str_repeat(box->bottom, col_widths[i]);
        strcat(result, line);
        FERN_FREE(line);
        if (i < table->column_count - 1) {
            strcat(result, box->bottom);
        }
    }
    strcat(result, box->bottom_right);
    
    FERN_FREE(col_widths);
    return result;
}

void fern_table_FERN_FREE(FernTable* table) {
    if (table) {
        FERN_FREE(table->title);
        FERN_FREE(table->caption);
        for (int64_t i = 0; i < table->column_count; i++) {
            FERN_FREE(table->columns[i].header);
        }
        FERN_FREE(table->columns);
        for (int64_t r = 0; r < table->row_count; r++) {
            for (int64_t c = 0; c < table->rows[r].cell_count; c++) {
                FERN_FREE(table->rows[r].cells[c]);
            }
            FERN_FREE(table->rows[r].cells);
        }
        FERN_FREE(table->rows);
        FERN_FREE(table);
    }
}

/* ========== Progress Bar Module ========== */

FernProgress* fern_progress_new(int64_t total) {
    FernProgress* p = FERN_ALLOC(sizeof(FernProgress));
    if (!p) return NULL;
    
    p->total = total > 0 ? total : 100;
    p->completed = 0;
    p->width = 40;
    p->description = NULL;
    p->fill_char = FERN_STRDUP("█");
    p->empty_char = FERN_STRDUP("░");
    p->show_percentage = 1;
    p->show_count = 0;
    
    return p;
}

FernProgress* fern_progress_description(FernProgress* p, const char* desc) {
    if (!p) return NULL;
    if (p->description) FERN_FREE(p->description);
    p->description = desc ? FERN_STRDUP(desc) : NULL;
    return p;
}

FernProgress* fern_progress_width(FernProgress* p, int64_t width) {
    if (!p) return NULL;
    p->width = width > 0 ? width : 40;
    return p;
}

FernProgress* fern_progress_advance(FernProgress* p) {
    if (!p) return NULL;
    if (p->completed < p->total) {
        p->completed++;
    }
    return p;
}

FernProgress* fern_progress_set(FernProgress* p, int64_t value) {
    if (!p) return NULL;
    if (value < 0) value = 0;
    if (value > p->total) value = p->total;
    p->completed = value;
    return p;
}

char* fern_progress_render(FernProgress* p) {
    if (!p) return FERN_STRDUP("");
    
    double ratio = p->total > 0 ? (double)p->completed / p->total : 0;
    int filled = (int)(ratio * p->width);
    int empty = (int)p->width - filled;
    
    /* Calculate buffer size */
    size_t desc_len = p->description ? strlen(p->description) + 1 : 0;
    size_t fill_len = strlen(p->fill_char);
    size_t empty_len = strlen(p->empty_char);
    size_t buf_size = desc_len + filled * fill_len + empty * empty_len + 50;
    
    char* result = FERN_ALLOC(buf_size);
    if (!result) return FERN_STRDUP("");
    result[0] = '\0';
    
    /* Description */
    if (p->description) {
        strcat(result, p->description);
        strcat(result, " ");
    }
    
    /* Opening bracket */
    strcat(result, "[");
    
    /* Filled portion */
    for (int i = 0; i < filled; i++) {
        strcat(result, p->fill_char);
    }
    
    /* Empty portion */
    for (int i = 0; i < empty; i++) {
        strcat(result, p->empty_char);
    }
    
    /* Closing bracket */
    strcat(result, "]");
    
    /* Percentage */
    if (p->show_percentage) {
        char pct[20];
        snprintf(pct, sizeof(pct), " %3d%%", (int)(ratio * 100));
        strcat(result, pct);
    }
    
    /* Count */
    if (p->show_count) {
        char count[40];
        snprintf(count, sizeof(count), " (%lld/%lld)", 
            (long long)p->completed, (long long)p->total);
        strcat(result, count);
    }
    
    return result;
}

void fern_progress_FERN_FREE(FernProgress* p) {
    if (p) {
        FERN_FREE(p->description);
        FERN_FREE(p->fill_char);
        FERN_FREE(p->empty_char);
        FERN_FREE(p);
    }
}

/* ========== Spinner Module ========== */

/* Spinner frame sequences */
static const char* SPINNER_FRAMES_DOTS[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
static const int SPINNER_FRAMES_DOTS_COUNT = 10;

static const char* SPINNER_FRAMES_LINE[] = {
    "-", "\\", "|", "/"
};
static const int SPINNER_FRAMES_LINE_COUNT = 4;

static const char* SPINNER_FRAMES_CIRCLE[] = {
    "◐", "◓", "◑", "◒"
};
static const int SPINNER_FRAMES_CIRCLE_COUNT = 4;

static const char* SPINNER_FRAMES_SQUARE[] = {
    "◰", "◳", "◲", "◱"
};
static const int SPINNER_FRAMES_SQUARE_COUNT = 4;

static const char* SPINNER_FRAMES_ARROW[] = {
    "←", "↖", "↑", "↗", "→", "↘", "↓", "↙"
};
static const int SPINNER_FRAMES_ARROW_COUNT = 8;

static const char* SPINNER_FRAMES_BOUNCE[] = {
    "⠁", "⠂", "⠄", "⠂"
};
static const int SPINNER_FRAMES_BOUNCE_COUNT = 4;

static const char* SPINNER_FRAMES_CLOCK[] = {
    "🕐", "🕑", "🕒", "🕓", "🕔", "🕕", "🕖", "🕗", "🕘", "🕙", "🕚", "🕛"
};
static const int SPINNER_FRAMES_CLOCK_COUNT = 12;

FernSpinner* fern_spinner_new(void) {
    FernSpinner* s = FERN_ALLOC(sizeof(FernSpinner));
    if (!s) return NULL;
    
    s->style = SPINNER_DOTS;
    s->frame = 0;
    s->message = NULL;
    
    return s;
}

FernSpinner* fern_spinner_message(FernSpinner* s, const char* message) {
    if (!s) return NULL;
    if (s->message) FERN_FREE(s->message);
    s->message = message ? FERN_STRDUP(message) : NULL;
    return s;
}

FernSpinner* fern_spinner_style(FernSpinner* s, const char* style) {
    if (!s || !style) return s;
    
    if (strcmp(style, "dots") == 0) s->style = SPINNER_DOTS;
    else if (strcmp(style, "line") == 0) s->style = SPINNER_LINE;
    else if (strcmp(style, "circle") == 0) s->style = SPINNER_CIRCLE;
    else if (strcmp(style, "square") == 0) s->style = SPINNER_SQUARE;
    else if (strcmp(style, "arrow") == 0) s->style = SPINNER_ARROW;
    else if (strcmp(style, "bounce") == 0) s->style = SPINNER_BOUNCE;
    else if (strcmp(style, "clock") == 0) s->style = SPINNER_CLOCK;
    
    s->frame = 0;  /* Reset frame on style change */
    return s;
}

FernSpinner* fern_spinner_tick(FernSpinner* s) {
    if (!s) return NULL;
    
    int count = 0;
    switch (s->style) {
        case SPINNER_DOTS:   count = SPINNER_FRAMES_DOTS_COUNT; break;
        case SPINNER_LINE:   count = SPINNER_FRAMES_LINE_COUNT; break;
        case SPINNER_CIRCLE: count = SPINNER_FRAMES_CIRCLE_COUNT; break;
        case SPINNER_SQUARE: count = SPINNER_FRAMES_SQUARE_COUNT; break;
        case SPINNER_ARROW:  count = SPINNER_FRAMES_ARROW_COUNT; break;
        case SPINNER_BOUNCE: count = SPINNER_FRAMES_BOUNCE_COUNT; break;
        case SPINNER_CLOCK:  count = SPINNER_FRAMES_CLOCK_COUNT; break;
    }
    
    s->frame = (s->frame + 1) % count;
    return s;
}

char* fern_spinner_render(FernSpinner* s) {
    if (!s) return FERN_STRDUP("");
    
    const char* frame = NULL;
    switch (s->style) {
        case SPINNER_DOTS:   frame = SPINNER_FRAMES_DOTS[s->frame]; break;
        case SPINNER_LINE:   frame = SPINNER_FRAMES_LINE[s->frame]; break;
        case SPINNER_CIRCLE: frame = SPINNER_FRAMES_CIRCLE[s->frame]; break;
        case SPINNER_SQUARE: frame = SPINNER_FRAMES_SQUARE[s->frame]; break;
        case SPINNER_ARROW:  frame = SPINNER_FRAMES_ARROW[s->frame]; break;
        case SPINNER_BOUNCE: frame = SPINNER_FRAMES_BOUNCE[s->frame]; break;
        case SPINNER_CLOCK:  frame = SPINNER_FRAMES_CLOCK[s->frame]; break;
    }
    
    if (!frame) frame = "?";
    
    size_t msg_len = s->message ? strlen(s->message) : 0;
    size_t buf_size = strlen(frame) + msg_len + 10;
    
    char* result = FERN_ALLOC(buf_size);
    if (!result) return FERN_STRDUP("");
    
    if (s->message) {
        snprintf(result, buf_size, "%s %s", frame, s->message);
    } else {
        strcpy(result, frame);
    }
    
    return result;
}

void fern_spinner_FERN_FREE(FernSpinner* s) {
    if (s) {
        FERN_FREE(s->message);
        FERN_FREE(s);
    }
}

/* ========== Prompt Module ========== */

char* fern_prompt_input(const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    
    if (read <= 0) {
        FERN_FREE(line);
        return FERN_STRDUP("");
    }
    
    /* Remove trailing newline */
    if (read > 0 && line[read - 1] == '\n') {
        line[read - 1] = '\0';
    }
    
    return line;
}

int fern_prompt_confirm(const char* prompt) {
    if (prompt) {
        printf("%s [y/N] ", prompt);
        fflush(stdout);
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    
    if (read <= 0) {
        FERN_FREE(line);
        return 0;
    }
    
    /* Check for yes */
    int result = (line[0] == 'y' || line[0] == 'Y');
    FERN_FREE(line);
    return result;
}

int fern_prompt_select(const char* prompt, FernStringList* choices) {
    if (!choices || choices->len == 0) return -1;
    
    /* Display prompt */
    if (prompt) {
        printf("%s\n", prompt);
    }
    
    /* Display choices */
    for (int64_t i = 0; i < choices->len; i++) {
        printf("  %lld. %s\n", (long long)(i + 1), choices->data[i]);
    }
    
    printf("Enter choice (1-%lld): ", (long long)choices->len);
    fflush(stdout);
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    
    if (read <= 0) {
        FERN_FREE(line);
        return -1;
    }
    
    /* Parse selection */
    long selection = strtol(line, NULL, 10);
    FERN_FREE(line);
    
    if (selection < 1 || selection > choices->len) {
        return -1;
    }
    
    return (int)(selection - 1);
}

char* fern_prompt_password(const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    
    /* Disable echo for password input */
    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    
    /* Restore echo */
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    printf("\n");  /* Print newline since user's enter was hidden */
    
    if (read <= 0) {
        FERN_FREE(line);
        return FERN_STRDUP("");
    }
    
    /* Remove trailing newline */
    if (read > 0 && line[read - 1] == '\n') {
        line[read - 1] = '\0';
    }
    
    return line;
}

int64_t fern_prompt_int(const char* prompt, int64_t min, int64_t max) {
    if (prompt) {
        printf("%s (%lld-%lld): ", prompt, (long long)min, (long long)max);
        fflush(stdout);
    }
    
    char* line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, stdin);
    
    if (read <= 0) {
        FERN_FREE(line);
        return min;
    }
    
    /* Parse number */
    char* endptr;
    long long value = strtoll(line, &endptr, 10);
    FERN_FREE(line);
    
    /* Validate */
    if (value < min) return min;
    if (value > max) return max;
    
    return (int64_t)value;
}
