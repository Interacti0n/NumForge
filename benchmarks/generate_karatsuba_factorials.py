"""Independent complete factorial references, outside NumForge timing."""
import argparse
import math
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("--timing", action="store_true")
parser.add_argument("--workspace", action="store_true")
parser.add_argument("--stress", action="store_true",
                    help="Workspace references around the application admission boundary")
parser.add_argument("--admission", action="store_true",
                    help="Complete references around the tighter tree admission boundary")
parser.add_argument("--split", action="store_true", help="Bounded split-workspace experiment")
args = parser.parse_args()
operations = ["factorial", "tree_grouped", "tree_karatsuba16", "tree_karatsuba32", "tree_karatsuba64", "tree_karatsuba128"]
if args.workspace:
    operations = ["factorial_grouped", "tree_workspace"] if args.timing else ["factorial", "factorial_grouped", "tree_workspace"]
if args.split:
    if not args.workspace or args.stress or args.admission:
        parser.error("--split requires --workspace and no stress/admission flag")
    operations = ["tree_workspace", "tree_split"]
if args.stress or args.admission:
    if not args.workspace or args.timing:
        parser.error("--stress/--admission requires --workspace without --timing")
    if args.stress and args.admission:
        parser.error("choose only one of --stress and --admission")
    operations = ["factorial", "tree_split"]
with args.output.open("w", encoding="utf-8", newline="\n") as stream:
    for n in ([100000, 230000, 235000, 245000, 250000, 255000] if args.split and args.timing else
              [0, 1, 31, 32, 33, 8191, 8192, 100000, 230000, 235000, 245000, 247572, 247573, 250000, 255000] if args.split else
              [8192, 233000, 234000, 234536, 234537, 235000, 250000] if args.admission else
              [200000, 225000, 230000, 233000, 234000, 250000, 300000] if args.stress else
              [2000, 8192, 10000, 50000, 100000, 150000, 200000] if args.timing and args.workspace else
              [10000, 50000, 100000, 150000, 200000] if args.timing else
              [0, 1, 31, 32, 33, 2000, 8191, 8192, 8193, 10000, 50000, 100000, 150000, 200000] if args.workspace else
              [0, 1, 20, 100, 2000, 10000, 50000, 100000, 150000, 200000]):
        reference = "hex:" + format(math.factorial(n), "x")
        for mode in (["reuse"] if args.timing or (n > 2000 and not args.workspace) else ["reuse", "alias_a"]):
            for operation in operations:
                stream.write(f"{operation}-{n}-{mode}\t{operation}\t{mode}\t{n}\t0\t{reference}\t0\n")
