"""Developer-only frozen benchmark references; runtime/CI do not need Python.

Use mpmath==1.3.0 in an isolated project environment. Independently evaluate at
two working precisions with enough extra digits for tiny inputs/cancellation
and large-angle reduction, then require identical HALF_EVEN rounded decimals.
"""
from decimal import Decimal, ROUND_HALF_EVEN, localcontext
from pathlib import Path
import mpmath as mp

PRECISIONS = (10, 100, 500, 1000, 2000)
CASES = [
    ("exp", "ordinary", "1", "0"), ("exp", "tiny", "1e-40", "0"),
    ("ln", "ordinary", "2", "0"), ("ln", "near_one", "1.0000000000000000000000000000000000000001", "0"),
    ("ln", "small", "1e-100", "0"), ("log10", "ordinary", "2", "0"),
    ("log", "ordinary", "2", "3"), ("log", "near_base_one", "2", "1.00000000000000000001"),
    ("sqrt", "ordinary", "2", "0"), ("cbrt", "negative", "-2", "0"),
    ("root", "degree7", "2", "7"),
    ("sin", "ordinary", "0.5", "0"), ("sin", "tiny", "1e-40", "0"), ("sin", "large_angle", "1e100", "0"),
    ("cos", "ordinary", "0.5", "0"), ("cos", "large_angle", "1e100", "0"),
    ("tan", "ordinary", "0.5", "0"), ("tan", "near_pole", "1.5707963267948966192313216916397514420985", "0"),
    ("tan", "resolved_pole", "1.570796326794", "0"),
    ("asin", "ordinary", "0.5", "0"), ("asin", "near_boundary", "0.99999999999999999999", "0"),
    ("acos", "near_boundary", "0.99999999999999999999", "0"), ("atan", "large", "1e100", "0"),
    ("sinh", "ordinary", "1", "0"), ("sinh", "tiny", "1e-40", "0"),
    ("cosh", "large", "100", "0"), ("tanh", "ordinary", "1", "0"),
    ("asinh", "tiny", "1e-40", "0"), ("acosh", "near_boundary", "1.00000000000000000001", "0"),
    ("atanh", "near_boundary", "0.99999999999999999999", "0"),
]

def evaluate(op, a, b):
    x = mp.mpf(a)
    if op == "pi": return mp.pi
    if op == "e": return mp.e
    if op == "phi": return (1 + mp.sqrt(5)) / 2
    if op == "log": return mp.log(x) / mp.log(mp.mpf(b))
    if op == "cbrt": return mp.sign(x) * mp.root(abs(x), 3)
    if op == "root": return mp.root(x, int(b))
    return getattr(mp, op)(x)

def rounded(op, a, b, digits, dps):
    with mp.workdps(dps), localcontext() as context:
        context.prec = dps + 20
        value = Decimal(mp.nstr(evaluate(op, a, b), dps - 20, strip_zeros=False))
        if value.is_zero(): return "0"
        quantum = Decimal(1).scaleb(value.adjusted() - digits + 1)
        return format(value.quantize(quantum, rounding=ROUND_HALF_EVEN).normalize(), "f")

def main():
    if mp.__version__ != "1.3.0": raise RuntimeError("Use mpmath==1.3.0")
    rows = []
    def add(op, name, a, b, digits):
        # +240/+400 cover the 100-digit angle and the cancellation cases.
        first = rounded(op, a, b, digits, digits + 240)
        if first != rounded(op, a, b, digits, digits + 400):
            raise RuntimeError(f"Unstable reference: {op}/{name}/{digits}")
        rows.append((f"{op}_{name}_p{digits}", op, "reuse", a, b, digits, 5, 0, 0, first))
    for op, name, a, b in CASES:
        for digits in PRECISIONS: add(op, name, a, b, digits)
    for op in ("pi", "e", "phi"):
        for digits in sorted(set(PRECISIONS + (499, 501))): add(op, "factory", "0", "0", digits)
    for op, a, b in [("ln", "0", "0"), ("log", "2", "1"), ("sqrt", "-2", "0"),
                     ("asin", "1.01", "0"), ("acosh", "0.9", "0"), ("atanh", "1", "0")]:
        rows.append((f"domain_{op}", op, "alias_a", a, b, 10, 5, 3, 0, 0))
    output = Path(__file__).with_name("decimal_math_references.tsv")
    with output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write("# mpmath 1.3.0; HALF_EVEN; working precision=requested+240 / requested+400.\n")
        stream.write("# name op mode input operand parameter rounding status tolerance expected\n")
        for row in rows: stream.write("\t".join(map(str, row)) + "\n")
    print(f"Wrote {len(rows)} independently checked cases to {output}")

if __name__ == "__main__": main()
