/**
 * Fern Runtime Library
 *
 * This library provides core functions for compiled Fern programs.
 * It is linked into every Fern executable.
 */

#ifndef FERN_RUNTIME_H
#define FERN_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

/* ========== I/O Functions ========== */

/**
 * Print an integer to stdout (no newline).
 * @param n The integer to print.
 */
void fern_print_int(int64_t n);

/**
 * Print an integer to stdout with newline.
 * @param n The integer to print.
 */
void fern_println_int(int64_t n);

/**
 * Print a string to stdout (no newline).
 * @param s The null-terminated string to print.
 */
void fern_print_str(const char* s);

/**
 * Print a string to stdout with newline.
 * @param s The null-terminated string to print.
 */
void fern_println_str(const char* s);

/**
 * Print a boolean to stdout (no newline).
 * @param b The boolean to print (0 = false, non-zero = true).
 */
void fern_print_bool(int64_t b);

/**
 * Print a boolean to stdout with newline.
 * @param b The boolean to print.
 */
void fern_println_bool(int64_t b);

/* ========== String Functions ========== */

/**
 * Get the length of a string.
 * @param s The null-terminated string.
 * @return The length in bytes.
 */
int64_t fern_str_len(const char* s);

/**
 * Concatenate two strings.
 * @param a First string.
 * @param b Second string.
 * @return Newly allocated concatenated string (caller must free).
 */
char* fern_str_concat(const char* a, const char* b);

/**
 * Compare two strings for equality.
 * @param a First string.
 * @param b Second string.
 * @return 1 if equal, 0 otherwise.
 */
int64_t fern_str_eq(const char* a, const char* b);

/**
 * Check if string starts with prefix.
 * @param s The string.
 * @param prefix The prefix to check.
 * @return 1 if s starts with prefix, 0 otherwise.
 */
int64_t fern_str_starts_with(const char* s, const char* prefix);

/**
 * Check if string ends with suffix.
 * @param s The string.
 * @param suffix The suffix to check.
 * @return 1 if s ends with suffix, 0 otherwise.
 */
int64_t fern_str_ends_with(const char* s, const char* suffix);

/**
 * Check if string contains substring.
 * @param s The string.
 * @param substr The substring to find.
 * @return 1 if s contains substr, 0 otherwise.
 */
int64_t fern_str_contains(const char* s, const char* substr);

/**
 * Find index of substring.
 * @param s The string.
 * @param substr The substring to find.
 * @return Option: Some(index) if found, None otherwise.
 */
int64_t fern_str_index_of(const char* s, const char* substr);

/**
 * Get substring from start to end (exclusive).
 * @param s The string.
 * @param start Start index.
 * @param end End index (exclusive).
 * @return New string containing the substring.
 */
char* fern_str_slice(const char* s, int64_t start, int64_t end);

/**
 * Trim whitespace from both ends.
 * @param s The string.
 * @return New string with whitespace trimmed.
 */
char* fern_str_trim(const char* s);

/**
 * Trim whitespace from start.
 * @param s The string.
 * @return New string with leading whitespace trimmed.
 */
char* fern_str_trim_start(const char* s);

/**
 * Trim whitespace from end.
 * @param s The string.
 * @return New string with trailing whitespace trimmed.
 */
char* fern_str_trim_end(const char* s);

/**
 * Convert string to uppercase.
 * @param s The string.
 * @return New string in uppercase.
 */
char* fern_str_to_upper(const char* s);

/**
 * Convert string to lowercase.
 * @param s The string.
 * @return New string in lowercase.
 */
char* fern_str_to_lower(const char* s);

/**
 * Replace all occurrences of old with new.
 * @param s The string.
 * @param old_str Substring to replace.
 * @param new_str Replacement string.
 * @return New string with replacements.
 */
char* fern_str_replace(const char* s, const char* old_str, const char* new_str);

/**
 * Split string by delimiter.
 * @param s The string.
 * @param delim The delimiter.
 * @return List of strings (as FernStringList).
 */
struct FernStringList* fern_str_split(const char* s, const char* delim);

/**
 * Join list of strings with separator.
 * @param list The string list.
 * @param sep The separator.
 * @return New joined string.
 */
char* fern_str_join(struct FernStringList* list, const char* sep);

/**
 * Repeat string n times.
 * @param s The string.
 * @param n Number of repetitions.
 * @return New string with s repeated n times.
 */
char* fern_str_repeat(const char* s, int64_t n);

/**
 * Get character at index.
 * @param s The string.
 * @param index The index.
 * @return Option: Some(char as int) if valid index, None otherwise.
 */
int64_t fern_str_char_at(const char* s, int64_t index);

/**
 * Check if string is empty.
 * @param s The string.
 * @return 1 if empty, 0 otherwise.
 */
int64_t fern_str_is_empty(const char* s);

/**
 * String list type for split/join operations.
 */
typedef struct FernStringList {
    char** data;
    int64_t len;
    int64_t cap;
} FernStringList;

/**
 * Free a string list.
 * @param list The list to free.
 */
void fern_str_list_free(FernStringList* list);

/* ========== List Functions ========== */

/**
 * Fern list structure.
 * Lists are immutable; operations return new lists.
 */
typedef struct FernList {
    int64_t* data;      /* Array of elements (int64_t for now) */
    int64_t len;        /* Number of elements */
    int64_t cap;        /* Allocated capacity */
} FernList;

/**
 * Create a new empty list.
 * @return Pointer to new list.
 */
FernList* fern_list_new(void);

/**
 * Create a list with given capacity.
 * @param cap Initial capacity.
 * @return Pointer to new list.
 */
FernList* fern_list_with_capacity(int64_t cap);

/**
 * Get the length of a list.
 * @param list The list.
 * @return Number of elements.
 */
int64_t fern_list_len(FernList* list);

/**
 * Get element at index.
 * @param list The list.
 * @param index The index (0-based).
 * @return The element value.
 */
int64_t fern_list_get(FernList* list, int64_t index);

/**
 * Append an element to a list in place (mutates the list).
 * Used for list literal construction.
 * @param list The list to modify.
 * @param value The value to append.
 */
void fern_list_push_mut(FernList* list, int64_t value);

/**
 * Append an element to a list (returns new list).
 * @param list The original list.
 * @param value The value to append.
 * @return New list with element appended.
 */
FernList* fern_list_push(FernList* list, int64_t value);

/**
 * Map a function over a list.
 * @param list The list.
 * @param fn Function pointer (int64_t -> int64_t).
 * @return New list with function applied to each element.
 */
FernList* fern_list_map(FernList* list, int64_t (*fn)(int64_t));

/**
 * Fold a list from left.
 * @param list The list.
 * @param init Initial accumulator value.
 * @param fn Function (acc, elem) -> acc.
 * @return Final accumulated value.
 */
int64_t fern_list_fold(FernList* list, int64_t init, int64_t (*fn)(int64_t, int64_t));

/**
 * Free a list.
 * @param list The list to free.
 */
void fern_list_free(FernList* list);

/**
 * Filter a list with a predicate.
 * @param list The list.
 * @param pred Predicate function (int64_t -> bool as int64_t).
 * @return New list with elements where pred returns non-zero.
 */
FernList* fern_list_filter(FernList* list, int64_t (*pred)(int64_t));

/**
 * Find first element matching predicate.
 * @param list The list.
 * @param pred Predicate function.
 * @return Option: Some(element) if found, None otherwise.
 */
int64_t fern_list_find(FernList* list, int64_t (*pred)(int64_t));

/**
 * Reverse a list.
 * @param list The list.
 * @return New list with elements in reverse order.
 */
FernList* fern_list_reverse(FernList* list);

/**
 * Concatenate two lists.
 * @param a First list.
 * @param b Second list.
 * @return New list with all elements from a followed by b.
 */
FernList* fern_list_concat(FernList* a, FernList* b);

/**
 * Get first element of a list.
 * @param list The list.
 * @return Option: Some(first) if non-empty, None otherwise.
 */
int64_t fern_list_head(FernList* list);

/**
 * Get list without first element.
 * @param list The list.
 * @return New list without first element (empty if list was empty/single).
 */
FernList* fern_list_tail(FernList* list);

/**
 * Check if list is empty.
 * @param list The list.
 * @return 1 if empty, 0 otherwise.
 */
int64_t fern_list_is_empty(FernList* list);

/**
 * Check if any element matches predicate.
 * @param list The list.
 * @param pred Predicate function.
 * @return 1 if any element matches, 0 otherwise.
 */
int64_t fern_list_any(FernList* list, int64_t (*pred)(int64_t));

/**
 * Check if all elements match predicate.
 * @param list The list.
 * @param pred Predicate function.
 * @return 1 if all elements match, 0 otherwise.
 */
int64_t fern_list_all(FernList* list, int64_t (*pred)(int64_t));

/* ========== Result Type ========== */

/**
 * Fern Result type.
 * Tag: 0 = Ok, 1 = Err
 * Value: The ok or err value (int64_t for simplicity).
 *
 * Represented as a packed 64-bit value:
 * - Upper 32 bits: value
 * - Lower 32 bits: tag (0 = Ok, 1 = Err)
 */

/**
 * Create an Ok result.
 * @param value The success value.
 * @return Packed Result value.
 */
int64_t fern_result_ok(int64_t value);

/**
 * Create an Err result.
 * @param value The error value.
 * @return Packed Result value.
 */
int64_t fern_result_err(int64_t value);

/**
 * Check if a Result is Ok.
 * @param result The packed Result value.
 * @return 1 if Ok, 0 if Err.
 */
int64_t fern_result_is_ok(int64_t result);

/**
 * Unwrap the value from a Result.
 * @param result The packed Result value.
 * @return The contained value (ok or err).
 */
int64_t fern_result_unwrap(int64_t result);

/**
 * Map a function over an Ok value.
 * @param result The Result.
 * @param fn Function to apply if Ok.
 * @return New Result with mapped value, or original Err.
 */
int64_t fern_result_map(int64_t result, int64_t (*fn)(int64_t));

/**
 * Chain a function that returns Result over an Ok value.
 * @param result The Result.
 * @param fn Function to apply if Ok (returns Result).
 * @return Result from fn if Ok, or original Err.
 */
int64_t fern_result_and_then(int64_t result, int64_t (*fn)(int64_t));

/**
 * Get the Ok value or a default.
 * @param result The Result.
 * @param default_val Value to return if Err.
 * @return Ok value or default.
 */
int64_t fern_result_unwrap_or(int64_t result, int64_t default_val);

/**
 * Get the Ok value or compute a default from the error.
 * @param result The Result.
 * @param fn Function to compute default from error.
 * @return Ok value or fn(err_value).
 */
int64_t fern_result_unwrap_or_else(int64_t result, int64_t (*fn)(int64_t));

/* ========== Option Type ========== */

/**
 * Fern Option type.
 * Tag: 0 = None, 1 = Some
 * Represented as packed 64-bit value (same as Result).
 */

/**
 * Create a Some option.
 * @param value The contained value.
 * @return Packed Option value.
 */
int64_t fern_option_some(int64_t value);

/**
 * Create a None option.
 * @return Packed Option value representing None.
 */
int64_t fern_option_none(void);

/**
 * Check if an Option is Some.
 * @param option The packed Option value.
 * @return 1 if Some, 0 if None.
 */
int64_t fern_option_is_some(int64_t option);

/**
 * Unwrap the value from an Option.
 * @param option The packed Option value.
 * @return The contained value (undefined if None).
 */
int64_t fern_option_unwrap(int64_t option);

/**
 * Map a function over a Some value.
 * @param option The Option.
 * @param fn Function to apply if Some.
 * @return New Option with mapped value, or None.
 */
int64_t fern_option_map(int64_t option, int64_t (*fn)(int64_t));

/**
 * Get the Some value or a default.
 * @param option The Option.
 * @param default_val Value to return if None.
 * @return Some value or default.
 */
int64_t fern_option_unwrap_or(int64_t option, int64_t default_val);

/* ========== Memory Functions ========== */

/**
 * Allocate memory.
 * @param size Number of bytes.
 * @return Pointer to allocated memory.
 */
void* fern_alloc(size_t size);

/**
 * Free memory.
 * @param ptr Pointer to free.
 */
void fern_free(void* ptr);

/* ========== File I/O Functions ========== */

/**
 * Fern file handle.
 */
typedef struct FernFile {
    void* handle;   /* FILE* internally */
    int64_t error;  /* Last error code, 0 if none */
} FernFile;

/**
 * Read entire file contents as a string.
 * @param path The file path.
 * @return Result: Ok(string contents) or Err(error code).
 *         Returns packed Result where Ok contains pointer to string.
 */
int64_t fern_read_file(const char* path);

/**
 * Write string to file (overwrites if exists).
 * @param path The file path.
 * @param contents The string to write.
 * @return Result: Ok(bytes written) or Err(error code).
 */
int64_t fern_write_file(const char* path, const char* contents);

/**
 * Append string to file (creates if not exists).
 * @param path The file path.
 * @param contents The string to append.
 * @return Result: Ok(bytes written) or Err(error code).
 */
int64_t fern_append_file(const char* path, const char* contents);

/**
 * Check if file exists.
 * @param path The file path.
 * @return 1 if exists, 0 otherwise.
 */
int64_t fern_file_exists(const char* path);

/**
 * Delete a file.
 * @param path The file path.
 * @return Result: Ok(0) if deleted, Err(error code) otherwise.
 */
int64_t fern_delete_file(const char* path);

/**
 * Get file size in bytes.
 * @param path The file path.
 * @return Result: Ok(size) or Err(error code).
 */
int64_t fern_file_size(const char* path);

/* Error codes for file operations */
#define FERN_ERR_FILE_NOT_FOUND  1
#define FERN_ERR_PERMISSION      2
#define FERN_ERR_IO              3
#define FERN_ERR_OUT_OF_MEMORY   4

#endif /* FERN_RUNTIME_H */
