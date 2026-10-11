"""Independent complete factorial references, outside NumForge timing."""
import argparse
import math
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("--timing", action="store_true")
args = parser.parse_args()
operations = ["factorial", "tree_grouped", "tree_karatsuba16", "tree_karatsuba32", "tree_karatsuba64", "tree_karatsuba128"]
with args.output.open("w", encoding="utf-8", newline="\n") as stream:
    for n in ([10000, 50000, 100000, 200000] if args.timing else [0, 1, 20, 100, 2000, 10000, 50000, 100000, 150000, 200000]):
        reference = "hex:" + format(math.factorial(n), "x")
        for mode in (["reuse"] if args.timing or n > 2000 else ["reuse", "alias_a"]):
            for operation in operations:
                stream.write(f"{operation}-{n}-{mode}\t{operation}\t{mode}\t{n}\t0\t{reference}\t0\n")
