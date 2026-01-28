/* Arena Allocator Implementation
 *
 * NOTE: This file intentionally uses malloc/free because it implements
 * the arena allocator itself. All other code should use arena_alloc().
 */

#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define ARENA_MIN_BLOCK_SIZE 4096
#define ARENA_ALIGNMENT 16
#define ARENA_MAX_BLOCKS 10000  /* Limit to prevent infinite loops */

typedef struct ArenaBlock {
    struct ArenaBlock* next;
    size_t size;
    size_t used;
    char data[];
} ArenaBlock;

struct Arena {
    ArenaBlock* current;
    ArenaBlock* first;
    size_t block_size;
    size_t total_allocated;
};

/**
 * Align n up to the nearest multiple of alignment.
 * @param n The value to align.
 * @param alignment The alignment (must be power of 2).
 * @return The aligned value.
 */
static size_t align_up(size_t n, size_t alignment) {
    assert(alignment > 0);
    assert((alignment & (alignment - 1)) == 0);  /* Must be power of 2. */
    return (n + alignment - 1) & ~(alignment - 1);
}

/**
 * Create a new arena block with the given size.
 * @param size The size of the block data area.
 * @return The new block, or NULL on allocation failure.
 */
static ArenaBlock* arena_block_create(size_t size) {
    // FERN_STYLE: allow(no-malloc) this IS the arena allocator implementation
    assert(size > 0);
    assert(size <= SIZE_MAX - sizeof(ArenaBlock));  /* Prevent overflow. */
    
    size_t block_size = sizeof(ArenaBlock) + size;
    ArenaBlock* block = malloc(block_size);
    if (!block) {
        return NULL;
    }
    
    block->next = NULL;
    block->size = size;
    block->used = 0;
    
    return block;
}

/**
 * Create a new arena with the given initial block size.
 * @param block_size The initial block size (minimum ARENA_MIN_BLOCK_SIZE).
 * @return The new arena, or NULL on allocation failure.
 */
Arena* arena_create(size_t block_size) {
    // FERN_STYLE: allow(no-malloc, no-free) this IS the arena allocator implementation
    assert(block_size > 0);
    assert(block_size <= SIZE_MAX / 2);  /* Reasonable upper bound. */
    
    if (block_size < ARENA_MIN_BLOCK_SIZE) {
        block_size = ARENA_MIN_BLOCK_SIZE;
    }
    
    Arena* arena = malloc(sizeof(Arena));
    if (!arena) {
        return NULL;
    }
    
    arena->block_size = block_size;
    arena->total_allocated = 0;
    arena->first = arena_block_create(block_size);
    
    if (!arena->first) {
        free(arena);
        return NULL;
    }
    
    arena->current = arena->first;
    
    return arena;
}

/**
 * Allocate size bytes from arena with the given alignment.
 * @param arena The arena to allocate from.
 * @param size The number of bytes to allocate.
 * @param alignment The alignment (must be power of 2).
 * @return Pointer to the allocated memory, or NULL on failure.
 */
void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment) {
    assert(arena != NULL);
    assert(size > 0);
    assert(alignment > 0 && (alignment & (alignment - 1)) == 0); // Power of 2
    
    ArenaBlock* block = arena->current;
    size_t aligned_size = align_up(size, ARENA_ALIGNMENT);
    
    // Calculate the aligned pointer from current position
    uintptr_t current_addr = (uintptr_t)(block->data + block->used);
    uintptr_t aligned_addr = (current_addr + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned_addr - current_addr;
    size_t aligned_used = block->used + padding;
    
    // Check if current block has enough space
    if (aligned_used + aligned_size > block->size) {
        // Need a new block - ensure it has space for alignment padding + data
        size_t new_block_size = arena->block_size;
        // Add extra space for worst-case alignment padding
        size_t required_size = aligned_size + alignment;
        if (required_size > new_block_size) {
            new_block_size = required_size;
        }
        
        ArenaBlock* new_block = arena_block_create(new_block_size);
        if (!new_block) {
            return NULL;
        }
        
        block->next = new_block;
        arena->current = new_block;
        block = new_block;
        
        // Recalculate alignment for new block
        current_addr = (uintptr_t)(block->data);
        aligned_addr = (current_addr + alignment - 1) & ~(alignment - 1);
        padding = aligned_addr - current_addr;
        aligned_used = padding;
    }
    
    void* ptr = block->data + aligned_used;
    block->used = aligned_used + aligned_size;
    arena->total_allocated += aligned_size;
    
    // Zero the memory
    memset(ptr, 0, size);
    
    return ptr;
}

/**
 * Allocate size bytes from arena with default alignment.
 * @param arena The arena to allocate from.
 * @param size The number of bytes to allocate.
 * @return Pointer to the allocated memory, or NULL on failure.
 */
void* arena_alloc(Arena* arena, size_t size) {
    assert(arena != NULL);
    assert(size > 0);
    return arena_alloc_aligned(arena, size, ARENA_ALIGNMENT);
}

/**
 * Reset arena to empty state, keeping allocated blocks for reuse.
 * @param arena The arena to reset.
 */
void arena_reset(Arena* arena) {
    assert(arena != NULL);
    assert(arena->first != NULL);
    
    /* Reset all blocks to unused. */
    size_t block_count = 0;
    for (ArenaBlock* block = arena->first; block != NULL && block_count < ARENA_MAX_BLOCKS; block = block->next) {
        block->used = 0;
        block_count++;
    }
    
    arena->current = arena->first;
    arena->total_allocated = 0;
}

/**
 * Free all memory associated with the arena.
 * @param arena The arena to destroy (may be NULL).
 */
void arena_destroy(Arena* arena) {
    // FERN_STYLE: allow(no-free) this IS the arena allocator implementation
    if (!arena) {
        return;
    }
    assert(arena->first != NULL || arena->current == NULL);
    
    /* Free all blocks with explicit limit. */
    ArenaBlock* block = arena->first;
    size_t block_count = 0;
    while (block && block_count < ARENA_MAX_BLOCKS) {
        ArenaBlock* next = block->next;
        free(block);
        block = next;
        block_count++;
    }
    assert(block == NULL);  /* Should have freed all blocks. */
    
    free(arena);
}

/**
 * Return total bytes allocated from this arena.
 * @param arena The arena to query.
 * @return The total bytes allocated.
 */
size_t arena_total_allocated(Arena* arena) {
    assert(arena != NULL);
    assert(arena->total_allocated <= SIZE_MAX);  /* Sanity check. */
    return arena->total_allocated;
}
