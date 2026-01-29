/* Dynamic String Library for Fern Compiler
 * 
 * Simple, efficient string type with automatic memory management.
 * Strings are immutable by default for safety.
 * 
 * Usage:
 *   Arena* arena = arena_create(4096);
 *   String* s = string_new(arena, "Hello");
 *   String* s2 = string_concat(arena, s, " World");
 *   printf("%s\n", string_cstr(s2));  // "Hello World"
 *   arena_destroy(arena);  // Frees all strings
 */

#ifndef FERN_STRING_H
#define FERN_STRING_H

#include "arena.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct String String;

/* Create a new string from a C string.
 * The string data is copied into the arena.
 * Returns NULL on allocation failure.
 */
String* string_new(Arena* arena, const char* cstr);

/* Create a new string from a buffer with explicit length.
 * Useful for non-null-terminated data.
 * Returns NULL on allocation failure.
 */
String* string_new_len(Arena* arena, const char* data, size_t len);

/* Create an empty string.
 * Returns NULL on allocation failure.
 */
String* string_empty(Arena* arena);

/* Get the C string representation.
 * The returned pointer is valid as long as the arena exists.
 */
const char* string_cstr(const String* s);

/* Get the length of the string (excluding null terminator). */
size_t string_len(const String* s);

/* Check if string is empty. */
bool string_is_empty(const String* s);

/* Concatenate two strings, returning a new string.
 * Returns NULL on allocation failure.
 */
String* string_concat(Arena* arena, const String* a, const String* b);

/* Concatenate a string with a C string, returning a new string.
 * Returns NULL on allocation failure.
 */
String* string_append_cstr(Arena* arena, const String* s, const char* cstr);

/* Compare two strings for equality. */
bool string_equal(const String* a, const String* b);

/* Compare two strings lexicographically.
 * Returns: < 0 if a < b, 0 if a == b, > 0 if a > b
 */
int string_compare(const String* a, const String* b);

/* Format a string using printf-style formatting.
 * Returns NULL on allocation failure.
 */
String* string_format(Arena* arena, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* FERN_STRING_H */
