/* Dynamic Array (Vector) for Fern Compiler
 * 
 * Generic dynamic array using macros.
 * All elements are stored contiguously for cache efficiency.
 * 
 * Usage:
 *   VEC_TYPE(IntVec, int);
 *   
 *   Arena* arena = arena_create(4096);
 *   IntVec* vec = IntVec_new(arena);
 *   IntVec_push(arena, vec, 42);
 *   IntVec_push(arena, vec, 99);
 *   
 *   for (size_t i = 0; i < vec->len; i++) {
 *       printf("%d\n", vec->data[i]);
 *   }
 */

#ifndef FERN_VEC_H
#define FERN_VEC_H

#include "arena.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/* Define a vector type for a specific element type.
 * 
 * Example:
 *   VEC_TYPE(TokenVec, Token*);
 * 
 * This creates:
 *   - Type: TokenVec
 *   - Constructor: TokenVec_new(arena)
 *   - Method: TokenVec_push(arena, vec, item)
 *   - Method: TokenVec_get(vec, index)
 *   - Field: .len (current length)
 *   - Field: .cap (current capacity)
 *   - Field: .data (array of elements)
 */
#define VEC_TYPE(Name, ElemType) \
    typedef struct Name { \
        size_t len; \
        size_t cap; \
        ElemType* data; \
    } Name; \
    \
    static inline Name* Name##_new(Arena* arena) { \
        Name* vec = arena_alloc(arena, sizeof(Name)); \
        if (!vec) return NULL; \
        vec->len = 0; \
        vec->cap = 8; \
        vec->data = arena_alloc(arena, sizeof(ElemType) * vec->cap); \
        return vec; \
    } \
    \
    static inline bool Name##_push(Arena* arena, Name* vec, ElemType item) { \
        assert(vec != NULL); \
        if (vec->len >= vec->cap) { \
            size_t new_cap = vec->cap * 2; \
            ElemType* new_data = arena_alloc(arena, sizeof(ElemType) * new_cap); \
            if (!new_data) return false; \
            memcpy(new_data, vec->data, sizeof(ElemType) * vec->len); \
            vec->data = new_data; \
            vec->cap = new_cap; \
        } \
        vec->data[vec->len++] = item; \
        return true; \
    } \
    \
    static inline ElemType Name##_get(const Name* vec, size_t index) { \
        assert(vec != NULL); \
        assert(index < vec->len); \
        return vec->data[index]; \
    } \
    \
    static inline ElemType* Name##_get_ptr(Name* vec, size_t index) { \
        assert(vec != NULL); \
        assert(index < vec->len); \
        return &vec->data[index]; \
    } \
    \
    static inline bool Name##_is_empty(const Name* vec) { \
        assert(vec != NULL); \
        return vec->len == 0; \
    } \
    \
    static inline ElemType Name##_last(const Name* vec) { \
        assert(vec != NULL); \
        assert(vec->len > 0); \
        return vec->data[vec->len - 1]; \
    }

#endif /* FERN_VEC_H */
