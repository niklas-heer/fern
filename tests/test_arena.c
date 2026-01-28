/* Arena Allocator Tests */

#include "test.h"
#include "arena.h"
#include <string.h>

void test_arena_create(void) {
    Arena* arena = arena_create(4096);
    ASSERT_NOT_NULL(arena);
    arena_destroy(arena);
}

void test_arena_alloc(void) {
    Arena* arena = arena_create(4096);
    ASSERT_NOT_NULL(arena);
    
    int* num = arena_alloc(arena, sizeof(int));
    ASSERT_NOT_NULL(num);
    
    *num = 42;
    ASSERT_EQ(*num, 42);
    
    arena_destroy(arena);
}

void test_arena_alloc_multiple(void) {
    Arena* arena = arena_create(4096);
    ASSERT_NOT_NULL(arena);
    
    int* nums[100];
    for (int i = 0; i < 100; i++) {
        nums[i] = arena_alloc(arena, sizeof(int));
        ASSERT_NOT_NULL(nums[i]);
        *nums[i] = i;
    }
    
    // Verify all values
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(*nums[i], i);
    }
    
    arena_destroy(arena);
}

void test_arena_alloc_large(void) {
    Arena* arena = arena_create(4096);
    ASSERT_NOT_NULL(arena);
    
    // Allocate something larger than default block size
    char* big = arena_alloc(arena, 8192);
    ASSERT_NOT_NULL(big);
    
    memset(big, 'A', 8192);
    ASSERT_EQ(big[0], 'A');
    ASSERT_EQ(big[8191], 'A');
    
    arena_destroy(arena);
}

void test_arena_reset(void) {
    Arena* arena = arena_create(4096);
    ASSERT_NOT_NULL(arena);
    
    int* num1 = arena_alloc(arena, sizeof(int));
    *num1 = 42;
    
    size_t allocated_before = arena_total_allocated(arena);
    ASSERT_TRUE(allocated_before > 0);
    
    arena_reset(arena);
    ASSERT_EQ(arena_total_allocated(arena), 0);
    
    // Can allocate after reset
    int* num2 = arena_alloc(arena, sizeof(int));
    ASSERT_NOT_NULL(num2);
    *num2 = 99;
    ASSERT_EQ(*num2, 99);
    
    arena_destroy(arena);
}

void test_arena_alloc_aligned(void) {
    Arena* arena = arena_create(4096);
    ASSERT_NOT_NULL(arena);
    
    // Allocate with 64-byte alignment
    void* ptr = arena_alloc_aligned(arena, 100, 64);
    ASSERT_NOT_NULL(ptr);
    
    // Check alignment
    uintptr_t addr = (uintptr_t)ptr;
    ASSERT_EQ(addr % 64, 0);
    
    arena_destroy(arena);
}

void run_arena_tests(void) {
    printf("\n=== Arena Allocator Tests ===\n");
    TEST_RUN(test_arena_create);
    TEST_RUN(test_arena_alloc);
    TEST_RUN(test_arena_alloc_multiple);
    TEST_RUN(test_arena_alloc_large);
    TEST_RUN(test_arena_reset);
    TEST_RUN(test_arena_alloc_aligned);
}
