/* Arena Allocator for Fern Compiler
 * 
 * Fast, safe memory management for compiler phases.
 * All allocations from an arena are freed together when the arena is destroyed.
 * 
 * Usage:
 *   Arena* arena = arena_create(4096);
 *   MyStruct* obj = arena_alloc(arena, sizeof(MyStruct));
 *   // ... use obj ...
 *   arena_destroy(arena);  // Free all allocations at once
 */

#ifndef FERN_ARENA_H
#define FERN_ARENA_H

#include <stddef.h>
#include <stdbool.h>

typedef struct Arena Arena;

/* Create a new arena with the given block size.
 * Returns NULL on allocation failure.
 */
Arena* arena_create(size_t block_size);

/* Allocate memory from the arena.
 * Returns NULL if allocation fails.
 * Memory is zeroed.
 */
void* arena_alloc(Arena* arena, size_t size);

/* Allocate aligned memory from the arena.
 * Returns NULL if allocation fails.
 * Memory is zeroed.
 */
void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment);

/* Reset arena to initial state, keeping allocated blocks for reuse.
 * Much faster than destroy + create for repeated use.
 */
void arena_reset(Arena* arena);

/* Destroy arena and free all memory.
 * All pointers allocated from this arena become invalid.
 */
void arena_destroy(Arena* arena);

/* Get total bytes allocated from this arena.
 * Useful for profiling memory usage.
 */
size_t arena_total_allocated(Arena* arena);

#endif /* FERN_ARENA_H */
