"""Independent Decimal/Fraction check of numeric_root_scan_benchmark CSV."""
import csv
from decimal import Decimal, localcontext
from fractions import Fraction
from pathlib import Path
import sys


def check(path):
    rows = list(csv.DictReader(Path(path).open(encoding="utf-8-sig", newline="")))
    if not rows:
        raise ValueError("empty benchmark")
    known = {"cubic", "sine", "cosine_fixed", "partial_log_domain", "sampled_pole",
             "vanishing_jump", "scaled_pole", "unsampled_tangent", "cluster_in_one_cell",
             "root_limit", "evaluation_limit"}
    checked = 0
    with localcontext() as context:
        context.prec = 180
        pi = Decimal("3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647")
        tolerance = Decimal("1E-12")
        references = {"cubic": [Decimal(-3), Decimal(0), Decimal(2)],
                      "sine": [k*pi for k in range(-3, 4)],
                      "cosine_fixed": [Decimal("0.7390851332151606416553120876738734040134117589007574649656806357732846548835475945993761069317665318498")],
                      "partial_log_domain": [Decimal(1)]}
        for row in rows:
            name = row["case"]
            assert name in known, row
            assert row["status"] == "0" and row["checked"] == "1", row
            xs = [Fraction(x) for x in row["xs"].split(";") if x]
            assert len(xs) == int(row["count"]), row
            assert all(a < b for a, b in zip(xs, xs[1:])), row
            assert 0 < int(row["evaluations"]) <= 4096, row
            assert 0 <= int(row["segments"]) <= 128, row
            stop = "root_limit" if name == "root_limit" else "evaluation_limit" if name == "evaluation_limit" else "scanned"
            assert row["stop"] == stop, row
            if stop == "scanned":
                assert row["segments"] == "128", row
            if name == "root_limit":
                assert xs == [Fraction(i, 128) for i in range(32)], row
                checked += len(xs)
            else:
                expected = references.get(name, [])
                assert len(xs) == len(expected), row
                for x, target in zip(xs, expected):
                    actual = Decimal(x.numerator)/Decimal(x.denominator)
                    assert abs(actual-target) <= tolerance, (row, actual, target)
                    if name == "cosine_fixed":
                        # Independent Taylor cosine at the full emitted coordinate.
                        term = value = Decimal(1)
                        for n in range(1, 300):
                            term *= -actual*actual/Decimal((2*n-1)*(2*n))
                            value += term
                            if abs(term) < Decimal("1E-170"):
                                break
                        assert abs(value-actual) <= tolerance, row
                    checked += 1
            if name in {"vanishing_jump", "scaled_pole", "evaluation_limit"}:
                assert int(row["rejected_brackets"]) > 0, row
            if name in {"sampled_pole", "partial_log_domain"}:
                assert int(row["domain_gaps"]) > 0, row
    print(f"{len(rows)} rows, {checked} full root coordinates independently verified (Decimal 180 / Fraction).")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: check_numeric_root_scan.py benchmark.csv")
    check(sys.argv[1])
