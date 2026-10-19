# NumForge Python adapter

Experimental CPython adapter over the public NumForge C APIs. The Python package
version is `0.1.0.dev0`; `numforge.core_version` identifies the linked C core.
It is built locally from source and has not been published to PyPI.

```python
from fractions import Fraction
import numforge

root = numforge.sqrt(2, precision=1000)
print(root, root.exact)  # approximate stored C value, False
print(numforge.evaluate("0.1+0.2").as_fraction())  # Fraction(3, 10), exact

statistics = numforge.describe([0, 0, 1])
print(statistics.mean)                 # 1/3
print(statistics.variance_population)  # 2/9
print(statistics.variance_sample)      # 1/3; None for a singleton

# Exact samples of a known continuous polynomial; the search itself runs in C.
search = numforge.find_root(lambda x, places: x*x-2, 1, 2, continuous=True)
print(search.kind, search.x, search.residual, search.exact_zero)

scan = numforge.find_roots(lambda x, places: x*(x-2)*(x+3), -4, 4, segments=8)
print(tuple(root.x for root in scan.roots))  # (-3, 0, 2), as Fraction
print(scan.kind, scan.complete)             # finished, False

expression = numforge.prepare("x*x+y", parameters=("x", "y"))
value = expression.evaluate({"x": Fraction(1, 3), "y": Fraction(1, 9)})
print(value.as_fraction())  # Fraction(2, 9)

solution = numforge.solve_quadratic(1, 0, -2)
for exact_root in solution.roots:
    print(exact_root.center, exact_root.sign, exact_root.radicand)

a = numforge.Matrix([[0, 2], [3, 4]])
b = numforge.Matrix([[7], [15]])
print(a.determinant())       # Fraction(-6)
print(a.solve(b).to_rows())  # ((Fraction(1, 3),), (Fraction(7, 2),))

family = numforge.Matrix([[0, 2, 4], [0, 3, 6]]).solve_general(numforge.Matrix([[10], [15]]))
print(family.kind, family.rank, family.nullity)  # infinite 1 2
print(family.particular.to_rows())              # ((0,), (5,), (0,)), as Fraction
print(family.nullspace.to_rows())               # ((1, 0), (0, -2), (0, 1)), as Fraction
```

The source build needs CPython 3.10+ with development headers, CMake 3.20+,
a C17 compiler, setuptools and wheel. Windows builds use a compatible MSVC toolchain
and `/MD`; NumForge is linked into the extension, with no separate NumForge DLL.
Use a virtual environment and run `python -m pip install .` from the repository root.
Existing installed C SDKs and numeric-only builds do not require Python.

Inputs accept `int`, `Fraction`, finite `Decimal`, numeric literal strings and
existing NumForge `Result` objects. Binary floats, complex floats and booleans
are rejected. Input strings/Decimal objects denote exact finite values; approximate
Result objects preserve their provenance. Calculator text is passed separately to
`evaluate()`. Precision means working significant digits, not a certified final
error bound or display rounding. Prepared expressions have explicit arguments,
not persistent sessions. Polynomial descriptors remain exact even for irrational
roots. Arithmetic, evaluation and solving are implemented in C.
Dense Matrix values contain exact real rationals, with `@`/multiply, determinant
and unique square-system solve performed by the public C matrix module. Singular
solves raise Error(status="singular"); shape failures use "dimension_mismatch".
`rank()` and `solve_general(right)` cover rectangular systems with one RHS column.
General solves return complete `none`/`unique`/`infinite` analyses with canonical
particular solutions and kernel bases. Inconsistency is a successful `none` result.
`describe(values)` computes an exact rational batch summary in C and returns a
frozen StatisticsSummary with count, sum, minimum, maximum, mean and both variances.
Population variance divides by n; sample variance divides by n-1 and is None for
a singleton. Inputs must be nonempty and exact real values. There is no precision
option; no dataset is retained by the copied Fraction result fields.

`find_root(function, lower, upper)` delegates bounded bisection to the public C
numerical kernel. The callback receives an exact Fraction and requested fractional
scale; it returns a real sample or None for a domain gap. `continuous=True` is the
caller's continuity assertion, required for approximate acceptance; it is not
automatic domain analysis. The frozen RootResult has diagnostic kind, optional
Fraction coordinate/Decimal residual, final bounds, exact_zero and work counts.
An exact coordinate is not proof of an exact root or complete discovery. Separate
absolute tolerances and work limits control convergence. Callback exceptions
propagate; Python callback work is not preempted by the cooperative C budget.
`find_roots` shares those rules and adds a bounded grid/candidate scan. It returns
an immutable tuple of sampled roots plus work/domain diagnostics. `root_limit` and
`evaluation_limit` return partial lists; `finished` only means the selected grid
was processed. Discovery is never complete, so `complete` remains False. Exact
coordinates are deduplicated without a tolerance merge; unseen contacts and
several roots inside a grid cell can still be missed.

All six C rounding modes are supported by name: `toward_zero`, `away_from_zero`,
`floor`, `ceiling`, `half_up`, `half_even`. C failures raise `numforge.Error` with
stable `status` text and zero-based UTF-8 byte `offset`. Python argument errors
use TypeError/ValueError. Cooperative time/allocation budgets cover C operations;
Python object memory and input conversion are not a hard process limit. Native
calls retain the GIL; this initial adapter does not promise parallel C execution,
instant Ctrl-C cancellation, PyPy or free-threaded CPython support.

See the full source guide `docs/guides/PYTHON.md` for contracts, installation
verification and the scope of the initial adapter.
