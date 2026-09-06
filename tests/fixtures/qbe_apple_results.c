/** Independent integer/Float/call-pressure oracle, reporting mismatch without crashing. */
#include <stdint.h>
#include <stdio.h>
#if defined(QBE_APPLE_SYMBOLS) && !defined(__APPLE__)
#define SYMBOL(name) __asm__("_" name)
#else
#define SYMBOL(name)
#endif
int64_t rotate(int64_t, int64_t, int64_t) SYMBOL("rotate");
double rotate_float(int64_t, double, double) SYMBOL("rotate_float");
int64_t pressure(int64_t) SYMBOL("pressure");
int64_t constant_spills(int64_t, int64_t) SYMBOL("constant_spills");
int64_t opaque(int64_t value) SYMBOL("opaque");

/** Keep a genuine external call boundary with a known full-width result.
 * @param value Full-width integer. @return The same integer. */
int64_t opaque(int64_t value) {
    volatile int64_t retained = value;
#if defined(__aarch64__) || defined(__arm64__)
    /* IP1 is caller-clobbered even though Apple reserves it as QBE scratch. */
    __asm__ volatile("mov x17, #123" ::: "x17");
#endif
    return retained;
}

/** Check results without abort/core-dump behavior, including the deterministic clobber probe.
 * @return Zero on exact results, one on any mismatch. */
int main(void) {
    for (int64_t count = 0; count < 200; count++) {
        int64_t delta = INT64_C(0x123456789abc) - INT64_C(0x998877665544);
        int64_t expected = count % 2 ? -delta : delta;
        if (rotate(count, INT64_C(0x123456789abc), INT64_C(0x998877665544)) != expected ||
            rotate_float(count, 1.25, 4.5) != (count % 2 ? 3.25 : -3.25)) {
            return 1;
        }
        if (pressure(count) != 41 * count + 780 ||
            constant_spills(1, count) != 41 * count + 780 ||
            constant_spills(0, count) != count + 40 * INT64_C(4294967296) + 780) {
            return 2;
        }
    }
    puts("integer, Float and call-pressure results passed");
    return 0;
}
