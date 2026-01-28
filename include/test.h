/* Simple Test Framework for Fern Compiler
 * 
 * Minimal testing library with clear output.
 * 
 * Usage:
 *   void test_addition() {
 *       ASSERT_EQ(2 + 2, 4);
 *       ASSERT_TRUE(5 > 3);
 *   }
 *   
 *   int main() {
 *       TEST_RUN(test_addition);
 *       return TEST_FINISH();
 *   }
 */

#ifndef FERN_TEST_H
#define FERN_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Test statistics */
typedef struct {
    int total;
    int passed;
    int failed;
} TestStats;

extern TestStats g_test_stats;

/* Color codes for output */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"

/* Print with color */
#define TEST_PRINT_COLOR(color, ...) \
    do { \
        printf(color); \
        printf(__VA_ARGS__); \
        printf(COLOR_RESET); \
    } while (0)

/* Assert macros */
#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            TEST_PRINT_COLOR(COLOR_RED, "  ✗ Assertion failed: %s\n", #cond); \
            TEST_PRINT_COLOR(COLOR_YELLOW, "    at %s:%d\n", __FILE__, __LINE__); \
            g_test_stats.failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            TEST_PRINT_COLOR(COLOR_RED, "  ✗ Assertion failed: %s == %s\n", #a, #b); \
            TEST_PRINT_COLOR(COLOR_YELLOW, "    at %s:%d\n", __FILE__, __LINE__); \
            g_test_stats.failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            TEST_PRINT_COLOR(COLOR_RED, "  ✗ Assertion failed: %s != %s\n", #a, #b); \
            TEST_PRINT_COLOR(COLOR_YELLOW, "    at %s:%d\n", __FILE__, __LINE__); \
            g_test_stats.failed++; \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) ASSERT_EQ(ptr, NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT_NE(ptr, NULL)

#define ASSERT_STR_EQ(a, b) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            TEST_PRINT_COLOR(COLOR_RED, "  ✗ Assertion failed: %s == %s\n", #a, #b); \
            TEST_PRINT_COLOR(COLOR_YELLOW, "    Expected: \"%s\"\n", (b)); \
            TEST_PRINT_COLOR(COLOR_YELLOW, "    Got:      \"%s\"\n", (a)); \
            TEST_PRINT_COLOR(COLOR_YELLOW, "    at %s:%d\n", __FILE__, __LINE__); \
            g_test_stats.failed++; \
            return; \
        } \
    } while (0)

/* Run a test */
#define TEST_RUN(test_func) \
    do { \
        int before_failed = g_test_stats.failed; \
        printf("Running " #test_func "... "); \
        fflush(stdout); \
        test_func(); \
        g_test_stats.total++; \
        if (g_test_stats.failed == before_failed) { \
            TEST_PRINT_COLOR(COLOR_GREEN, "✓ PASS\n"); \
            g_test_stats.passed++; \
        } else { \
            TEST_PRINT_COLOR(COLOR_RED, "✗ FAIL\n"); \
        } \
    } while (0)

/* Finish testing and return exit code */
#define TEST_FINISH() test_finish()

/* Initialize test stats */
void test_init(void);

/* Print summary and return exit code */
int test_finish(void);

#endif /* FERN_TEST_H */
