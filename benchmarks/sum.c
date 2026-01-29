// Sum benchmark - recursive (to match Fern)
#include <stdio.h>
#include <stdint.h>

int64_t sum_to(int64_t n, int64_t acc) {
    if (n <= 0) return acc;
    return sum_to(n - 1, acc + n);
}

int main(void) {
    int64_t result = sum_to(10000, 0);
    printf("%lld\n", result);
    return 0;
}
