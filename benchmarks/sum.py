#!/usr/bin/env python3
# Sum benchmark - recursive (to match Fern)
import sys
sys.setrecursionlimit(200000)

def sum_to(n, acc=0):
    if n <= 0:
        return acc
    return sum_to(n - 1, acc + n)

if __name__ == "__main__":
    print(sum_to(10000))
