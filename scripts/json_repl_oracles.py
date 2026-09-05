#!/usr/bin/env python3
"""Regenerate independent numeric checksums for the std-only JSON REPL tests.

No Fern or native code is invoked: Decimal supplies exact integer/error results,
Python binary64 supplies rounding bits, and .17g supplies builder spellings.
"""
import decimal
import math
import struct

MASK = (1 << 64) - 1
START = 14695981039346656037


class Random:
    def __init__(self, seed):
        self.seed = seed

    def next(self):
        self.seed = (self.seed * 6364136223846793005 + 1442695040888963407) & MASK
        return self.seed


def fnv(digest, text):
    for byte in text.encode():
        digest = ((digest ^ byte) * 1099511628211) & MASK
    return digest


def decimal_oracles():
    random = Random(77)
    digest = START
    for _ in range(6000):
        sign = "-" if random.next() >> 63 else ""
        whole, fraction, exponent = random.next(), random.next(), random.next() % 801 - 400
        text = f"{sign}{whole}.{fraction:020d}e{exponent}"
        value = decimal.Decimal(text)
        integral = value.to_integral_value()
        integer = ("err:9" if value != integral else "err:8"
                   if not -(1 << 63) <= integral < (1 << 63) else f"ok:{int(integral)}")
        converted = float(text)
        floating = ("err:8" if not math.isfinite(converted) or (converted == 0 and value != 0)
                    else "ok:" + struct.pack(">d", converted).hex())
        digest = fnv(digest, integer + "|" + floating + "\n")
    return digest


def format_oracles():
    random = Random(75)
    digest = START
    for _ in range(6000):
        value = struct.unpack(">d", random.next().to_bytes(8, "big"))[0]
        text = "err:11" if not math.isfinite(value) else format(value, ".17g")
        digest = fnv(digest, text + "\n")
    return digest


if __name__ == "__main__":
    decimal_digest, format_digest = decimal_oracles(), format_oracles()
    assert decimal_digest == 0x324E7A4B814E6448
    assert format_digest == 0x94597FF07428AB24
    print(f"6000 decimal oracles: {decimal_digest:016x}")
    print(f"6000 formatter oracles: {format_digest:016x}")
