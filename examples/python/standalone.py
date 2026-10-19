"""Run with an installed NumForge wheel; no server or repository imports."""
from fractions import Fraction
import numforge

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
