/**
 * Fern Garbage Collection Header
 *
 * This header provides memory allocation macros for the Fern runtime.
 * Currently uses Boehm GC for automatic memory management.
 *
 * Future: BEAM-style per-process heaps for actor isolation.
 */

#ifndef FERN_GC_H
#define FERN_GC_H

#include <gc.h>
#include <string.h>

/* ========== GC Initialization ========== */

/**
 * Initialize the garbage collector.
 * Must be called once at program startup (before any allocations).
 */
#define fern_gc_init() GC_INIT()

/* ========== Allocation Macros ========== */

/**
 * Allocate memory that will be automatically garbage collected.
 * Replacement for malloc().
 */
#define FERN_ALLOC(size) GC_MALLOC(size)

/**
 * Allocate zeroed memory (like calloc).
 * GC_MALLOC already zeros memory, so this is the same as FERN_ALLOC.
 */
#define FERN_CALLOC(count, size) GC_MALLOC((count) * (size))

/**
 * Reallocate memory to a new size.
 * Replacement for realloc().
 */
#define FERN_REALLOC(ptr, size) GC_REALLOC(ptr, size)

/**
 * Duplicate a string with GC-managed memory.
 * Replacement for strdup().
 */
static inline char* fern_gc_strdup(const char* s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char* dup = (char*)GC_MALLOC_ATOMIC(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

#define FERN_STRDUP(s) fern_gc_strdup(s)

/* ========== Free (No-ops with GC) ========== */

/**
 * Free memory. With Boehm GC, this is a no-op.
 * The GC will automatically reclaim unreachable memory.
 *
 * We keep these as explicit no-ops rather than removing the calls
 * to maintain code clarity about ownership semantics.
 */
#define FERN_FREE(ptr) ((void)(ptr))

/* ========== GC Statistics (Optional) ========== */

/**
 * Get the total heap size managed by the GC.
 */
#define fern_gc_heap_size() GC_get_heap_size()

/**
 * Get the number of bytes allocated since the last collection.
 */
#define fern_gc_bytes_since_gc() GC_get_bytes_since_gc()

/**
 * Force a garbage collection cycle.
 * Normally not needed; the GC runs automatically.
 */
#define fern_gc_collect() GC_gcollect()

#endif /* FERN_GC_H */
