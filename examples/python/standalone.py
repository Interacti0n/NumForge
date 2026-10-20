"""Run with an installed NumForge wheel; no server or repository imports."""
from fractions import Fraction
import numforge

coordinates = numforge.grid(0, 1, 4)
assert coordinates == (0, Fraction(1, 3), Fraction(2, 3), 1)
print("Exact grid:", coordinates)

statistics = numforge.describe([0, 0, 1])
print("Exact statistics:", statistics.count, statistics.mean, statistics.variance_population, statistics.variance_sample)

covariance = numforge.covariance([1, 2, 3], [3, 1, -1], sample=True)
assert covariance == Fraction(-2)
print("Exact sample covariance:", covariance)

# The caller supplies exact samples and explicitly asserts continuity of x*x-2.
search = numforge.find_root(lambda x, places: x*x-2, 1, 2, continuous=True)
assert search.kind == "found" and not search.exact_zero
assert abs(search.x*search.x-2) <= Fraction(1, 10**12)
print("Bounded root:", search.x, "residual:", search.residual)

scan = numforge.find_roots(lambda x, places: x*(x-2)*(x+3), -4, 4, segments=8)
assert tuple(root.x for root in scan.roots) == (-3, 0, 2)
assert scan.kind == "finished" and not scan.complete
print("Sampled roots:", *(root.x for root in scan.roots), "complete:", scan.complete)

a = numforge.Matrix([[0, 2], [3, 4]])
b = numforge.Matrix([[7], [15]])
print("Exact determinant:", a.determinant())
print("Exact linear solution:", a.solve(b).to_rows())

family = numforge.Matrix([[0, 2, 4], [0, 3, 6]]).solve_general(numforge.Matrix([[10], [15]]))
print("General solution:", family.kind, "rank:", family.rank, "nullity:", family.nullity)
print("Particular:", family.particular.to_rows())
print("Nullspace columns:", family.nullspace.to_rows())

root = numforge.sqrt(2, precision=1000)
assert not root.exact
print("sqrt(2), 1000 working digits:", root)

expression = numforge.prepare("x*x+y", ("x", "y"))
value = expression.evaluate({"x": Fraction(1, 3), "y": Fraction(1, 9)})
assert value.as_fraction() == Fraction(2, 9)
print("exact parameterized result:", value)

solution = numforge.solve_quadratic(1, 0, -2)
assert all(item.exact and item.value is None for item in solution.roots)
print("exact roots:", *(str(item) for item in solution.roots))
