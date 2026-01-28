/* Test Framework Implementation */

#include "test.h"
#include <assert.h>

TestStats g_test_stats = {0, 0, 0};

/**
 * Initialize the test framework, resetting all counters.
 */
void test_init(void) {
    assert(g_test_stats.total >= 0);
    assert(g_test_stats.failed >= 0);
    g_test_stats.total = 0;
    g_test_stats.passed = 0;
    g_test_stats.failed = 0;
}

/**
 * Finish testing and print results.
 * @return 0 on success, 1 on failure.
 */
int test_finish(void) {
    assert(g_test_stats.total >= 0);
    assert(g_test_stats.passed + g_test_stats.failed == g_test_stats.total);
    printf("\n");
    printf("================================================================================\n");
    printf("Test Results:\n");
    printf("  Total:  %d\n", g_test_stats.total);
    
    if (g_test_stats.passed > 0) {
        TEST_PRINT_COLOR(COLOR_GREEN, "  Passed: %d\n", g_test_stats.passed);
    }
    
    if (g_test_stats.failed > 0) {
        TEST_PRINT_COLOR(COLOR_RED, "  Failed: %d\n", g_test_stats.failed);
    }
    
    printf("================================================================================\n");
    
    if (g_test_stats.failed == 0) {
        TEST_PRINT_COLOR(COLOR_GREEN, "\n✓ All tests passed!\n\n");
        return 0;
    } else {
        TEST_PRINT_COLOR(COLOR_RED, "\n✗ Some tests failed.\n\n");
        return 1;
    }
}
