"""Developer-only independent fixtures; requires mpmath==1.3.0.

Each component must agree at 600 and 800 decimal digits. References retain
digits+20 significant digits; tests allow 2 units at the requested precision.
Exact cuts follow NumForge's documented unsigned-zero convention.
"""
from pathlib import Path
import mpmath as mp

if mp.__version__ != "1.3.0":
    raise RuntimeError("Requires mpmath==1.3.0")

OPS = "sqrt exp ln sin cos tan sinh cosh tanh asin acos atan asinh acosh atanh".split()

def reference(op, re, im, digits, precision):
    with mp.workdps(precision):
        z = mp.mpc(re, im)
        value = mp.log(z) if op == "ln" else getattr(mp, op)(z)
        # mpmath chooses the opposite lip for the negative imaginary asinh cut.
        if op == "asinh" and re == "0" and mp.mpf(im) < -1:
            value = -mp.asinh(mp.mpc(0, -mp.mpf(im)))
        return [mp.nstr(x, digits + 20) for x in (value.real, value.imag)]

rows = []
for digits in (12, 34, 100, 250):
    for op in OPS:
        points = [("1", "1"), ("-0.3", "0.7"), ("0.1", "1e-100")]
        if op in ("sqrt", "ln", "asin", "acos", "atan", "asinh", "acosh", "atanh"):
            points += [("2", "0"), ("-2", "0"), ("0", "2"), ("0", "-2"),
                       ("-2", "1e-30"), ("-2", "-1e-30")]
        if op in ("sqrt", "ln", "asinh", "acosh", "atan"):
            points += [("1e100", "1e-100"), ("3e-100", "4e-100")]
        if op in ("tan", "tanh", "atan", "atanh"):
            points += [("0.00000000000000000001", "1.00000000000000000001"),
                       ("1.00000000000000000001", "0.00000000000000000001")]
        for re, im in points:
            a = reference(op, re, im, digits, 600)
            if a != reference(op, re, im, digits, 800):
                raise RuntimeError(f"Unstable reference: {op} {re} {im}")
            for rounding in (range(6) if (re, im) == ("1", "1") else (5,)):
                rows.append("\t".join([op, re, im, str(digits), str(rounding), *a]))
path = Path(__file__).with_name("complex_references.tsv")
path.write_text("# mpmath 1.3.0; op re im digits rounding expected_re expected_im\n" + "\n".join(rows) + "\n", encoding="utf-8")
print(f"Generated {len(rows)} independently verified cases")

BINARY = {
    "div": [("1", "1", "3", "-2"), ("1e100", "1e-100", "1e100", "-1e-100"),
            ("1", "1", "1e-100", "1e-100"), ("1", "-1", "1", "1"),
            ("0.00000000000000000001", "1", "1", "0.00000000000000000001")],
    "log": [("1", "1", "2", "-0.5"), ("-2", "0", "-3", "0"),
            ("-2", "1e-30", "-3", "-1e-30"), ("-2", "-1e-30", "-3", "1e-30"),
            ("1.00000000000000000001", "1e-30", "1.00000000000000000002", "-1e-30"),
            ("1", "1", "1", "1." + "0" * 59 + "1"),
            ("1e100", "1e-100", "1e-100", "2e-100")],
    "pow": [("1", "1", "0.3", "-0.2"), ("-2", "0", "0.5", "0"),
            ("-2", "1e-30", "0.3", "0.2"), ("-2", "-1e-30", "0.3", "0.2"),
            ("1", "1e-100", "1", "1"), ("1e100", "1e-100", "0.3", "0.2"),
            ("0.99999999999999999999", "1e-30", "1000", "0.3")],
}

def binary_reference(op, re, im, bre, bim, digits, precision):
    with mp.workdps(precision):
        z, b = mp.mpc(re, im), mp.mpc(bre, bim)
        value = z / b if op == "div" else mp.log(z) / mp.log(b) if op == "log" else mp.power(z, b)
        return [mp.nstr(x, digits + 20) for x in (value.real, value.imag)]

rows = []
for digits in (12, 34, 100, 250):
    for op, points in BINARY.items():
        for index, (re, im, bre, bim) in enumerate(points):
            a = binary_reference(op, re, im, bre, bim, digits, 600)
            if a != binary_reference(op, re, im, bre, bim, digits, 800):
                raise RuntimeError(f"Unstable binary reference: {op} {re} {im}")
            for rounding in (range(6) if index == 0 or (op == "log" and bim.startswith("1.000")) else (5,)):
                rows.append("\t".join([op, re, im, str(digits), str(rounding), *a, bre, bim]))
path.with_name("complex_binary_references.tsv").write_text(
    "# mpmath 1.3.0; op re im digits rounding expected_re expected_im b_re b_im\n" + "\n".join(rows) + "\n", encoding="utf-8")
print(f"Generated {len(rows)} independently verified binary cases")
