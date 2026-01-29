// Fibonacci benchmark - recursive (CPU-bound)
#include <stdio.h>

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    int result = fib(35);
    printf("%d\n", result);
    return 0;
}
