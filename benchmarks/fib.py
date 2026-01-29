#!/usr/bin/env python3
# Fibonacci benchmark - recursive (CPU-bound)

def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

if __name__ == "__main__":
    print(fib(35))
