"""Complete independent normalization coefficients/counts, never timed."""
import argparse
import math
import random
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("--timing", action="store_true")
args = parser.parse_args()
rng = random.Random(271019)
cases = []
for n in ([10000, 100000, 200000, 230000] if args.timing else [100, 2000, 10000, 100000, 200000, 230000]):
    value = math.factorial(n)
    count, p = 0, n // 5
    while p:
        count += p
        p //= 5
    cases.append((f"factorial-{n}", value, value // 10**count, count))
for bits in ([4096, 65536] if args.timing else [0, 1, 64, 127, 1024, 8192, 65536]):
    base = (rng.getrandbits(bits) | 1) if bits else 0
    if base and base % 5 == 0:
        base += 2
    for zeros in ([0, 1, 27, 1000, 10000] if args.timing else [0, 1, 18, 19, 20, 26, 27, 28, 53, 54, 55, 1000]):
        cases.append((f"bits-{bits}-zeros-{zeros}", base * 10**zeros, base, zeros if base else 0))
if not args.timing:
    # Surplus powers of 2/5, carry-heavy and just-nondivisible trial quotients.
    for i in range(100):
        two, five = rng.randrange(0, 500), rng.randrange(0, 500)
        base = rng.getrandbits(rng.randrange(1, 4096)) | 1
        while base % 5 == 0:
            base += 2
        count = min(two, five)
        value = base * 2**two * 5**five
        cases.append((f"valuation-{i}", value, value // 10**count, count))
    for bits in [64, 128, 1024, 8192, 262144]:
        cases.append((f"only-two-{bits}", 1 << bits, 1 << bits, 0))
        for count in [19, 27, 54]:
            base = (1 << bits) - 1
            while base % 5 == 0:
                base -= 2
            cases.append((f"carry-{bits}-{count}", base * 10**count, base, count))

def encode(n):
    return "hex:" + ("-" if n < 0 else "") + format(abs(n), "x")

operations = ["strip_classic", "strip19", "strip27"] if args.timing else ["strip_classic", "strip19", "strip27", "strip"]
with args.output.open("w", encoding="utf-8", newline="\n") as out:
    for name, value, expected, count in cases:
        for sign in ([1] if args.timing else [1, -1]):
            for mode in (["reuse"] if args.timing else ["reuse", "alias_a"]):
                for op in operations:
                    out.write(f"{name}-{sign}-{mode}-{op}\t{op}\t{mode}\t{encode(sign*value)}\t0\t{encode(sign*expected)}\t{count}\n")
