"""Independent standard-library Decimal reevaluation of emitted full coordinates.

Run against a complete recording or --quick CSV. No production parser or numeric
library is reused. This verifies sampled residuals, not continuity/existence.
"""
import csv
from decimal import Decimal as D, localcontext
from fractions import Fraction
import sys


def sine(x):
    term = total = x
    for n in range(1, 1000):
        term *= -x * x / D((2 * n) * (2 * n + 1))
        previous, total = total, total + term
        if previous == total:
            return total
    raise AssertionError("sine reference did not converge")


def cosine(x):
    term = total = D(1)
    for n in range(1, 1000):
        term *= -x * x / D((2 * n - 1) * (2 * n))
        previous, total = total, total + term
        if previous == total:
            return total
    raise AssertionError("cosine reference did not converge")


functions = {
    "cos_fixed": lambda x: cosine(x) - x,
    "log_two": lambda x: x.exp() - 2,
    "log_inverse": lambda x: x.ln() - 1,
    "cubic": lambda x: x**3 - 2,
    "flat_cubic": lambda x: (x - D(".1234"))**3,
    "tiny_linear": lambda x: D("1E-100") * (x - D(".1234")),
    "large_offset": lambda x: x - D("1E100"),
    # Cancel the common offset analytically in the independent reference.
    "large_cancellation": lambda x: x - D(1) / 3,
    "narrow_linear": lambda x: x - D(".1234"),
    "sqrt_boundary": lambda x: x.sqrt() - D(".1"),
    "rapid_sine": lambda x: sine(1000 * x),
    "steep_exp": lambda x: (100 * x).exp() - 2,
    "multiple_roots": lambda x: (x - 1) * (x - 2) * (x - 3),
    "absolute_corner": lambda x: abs(x - D(".1234")) - x,
    "steep_linear": lambda x: D("1E20") * (x - D(".1234")),
    "wide_linear": lambda x: x - D(1) / 3,
    "huge_exp_target": lambda x: x.exp() - D("1E100"),
    "log_near_boundary": lambda x: x.ln() + 100,
}


def main(path):
    count = found = 0
    with open(path, newline="", encoding="utf-8-sig") as source, localcontext() as context:
        context.prec = 180
        for row in csv.DictReader(source):
            count += 1
            assert row["verified"] == "1" and row["status"] == "0", row
            assert D(row["ms"]) >= 0 and int(row["evaluations"]) <= 192, row
            assert int(row["iterations"]) <= 96, row
            if "interpolations" in row:
                bisect, interpolate = int(row["bisections"]), int(row["interpolations"])
                fallback = int(row.get("fallback_steps", 0))
                switches = int(row.get("switch_offs", 0))
                assert bisect + interpolate == int(row["iterations"]), row
                assert int(row["secants"]) + int(row["quadratics"]) == interpolate, row
                assert switches <= 1 and (not fallback or switches == 1), row
                if row["algorithm"] != "bisection":
                    assert int(row["rejections"]) + fallback == bisect, row
            if row["outcome"] != "found":
                assert not row["x"] and not row["residual"], row
                continue
            found += 1
            coordinate = Fraction(row["x"])
            x = D(coordinate.numerator) / D(coordinate.denominator)
            residual = abs(functions[row["case"]](x))
            assert residual <= D("1E-12"), (row["case"], residual)
            # Work precision is at least 32 digits; permit only its numerical
            # rounding noise, independently of the much looser root tolerance.
            assert abs(residual - D(row["residual"])) <= D("1E-25"), (row, residual)
    assert count and found, "empty recording or no found roots"
    print(f"Checked {count} rows; independently reevaluated {found} full coordinates at 180 digits.")


if __name__ == "__main__":
    main(sys.argv[1])
