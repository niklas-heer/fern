/* Test Framework Implementation */

#include "test.h"

TestStats g_test_stats = {0, 0, 0};

void test_init(void) {
    g_test_stats.total = 0;
    g_test_stats.passed = 0;
    g_test_stats.failed = 0;
}

int test_finish(void) {
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
