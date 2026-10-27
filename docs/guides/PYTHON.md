# Python adapter

Experimental, source-built; not on PyPI. Package `0.1.0.dev0` and
`numforge.core_version` identify adapter/core independently. Downloads may lag source.

## Build and install

Requires CPython 3.10+ development headers, CMake 3.20+, C17 compiler,
setuptools/wheel and a virtual environment:

```sh
python -m pip install .
```

Offline, with build tools installed:

```sh
python -m pip wheel . --no-build-isolation --no-deps --no-index --wheel-dir build/python-dist
```

Install the resulting wheel by filename. It statically links numeric/engine code;
no NumForge DLL, server, Node or source tree is needed at runtime. Wheels match
CPython ABI/OS/architecture, not abi3. Windows uses compatible MSVC `/MD`; keep
paths short. Ordinary C builds do not require Python. CI owns platform coverage.

## Initial API

```python
from decimal import Decimal
from fractions import Fraction
import numforge

root = numforge.sqrt(2, precision=1000)
print(root.kind, root.exact)               # decimal, False
print(numforge.number(Decimal("0.1")).as_fraction())  # 1/10
print(numforge.evaluate("0.1+0.2").as_fraction())    # 3/10

expression = numforge.prepare("x*x+y", ("x", "y"))
value = expression.evaluate({"x": Fraction(1, 3), "y": Fraction(1, 9)})
assert value.exact and value.as_fraction() == Fraction(2, 9)
```

`number`, `evaluate`, `prepare` and `sqrt` return typed C values/handles.
Prepared expressions use explicit parameters, not persistent sessions.

## Exact descriptive statistics

```python
s = numforge.describe([0, 0, 1])
assert s.count == 3 and s.exact
assert s.mean == Fraction(1, 3)
assert s.variance_population == Fraction(2, 9)
assert s.variance_sample == Fraction(1, 3)
assert numforge.describe([5]).variance_sample is None
```

`describe`, `covariance` and `linear_regression` compute exact rational
results. [Fields, inputs and singleton rules](../reference/STATISTICS_API.md#python-and-interface-availability).

## Exact matrices

```python
a = numforge.Matrix([[0, 2], [3, 4]])
b = numforge.Matrix([[7], [15]])
x = a.solve(b)
assert a.shape == (2, 2) and a.exact
assert a.determinant() == -6
assert x.to_rows() == ((Fraction(1, 3),), (Fraction(7, 2),))
assert (a @ x).to_rows() == b.to_rows()
```

```python
a = numforge.Matrix([[0, 2, 4], [0, 3, 6]])
family = a.solve_general(numforge.Matrix([[10], [15]]))
assert (a.rank(), family.kind, family.nullity) == (1, "infinite", 2)
assert family.particular.to_rows() == ((0,), (5,), (0,))
assert family.nullspace.to_rows() == ((1, 0), (0, -2), (0, 1))
```

Immutable exact rational matrices support `@`, determinant, rank, square
solves and complete single-RHS families. [Matrix rules](../reference/MATRIX_API.md#python-and-other-interfaces) ·
[General systems](../reference/LINEAR_SYSTEM_API.md#python).

## Exact grids

```python
coordinates = numforge.grid(0, 1, 4)
assert coordinates == (0, Fraction(1, 3), Fraction(2, 3), 1)

visited = []
numforge.visit_grid(0, 1, 4, lambda x, index: visited.append((index, x)))
assert visited == list(enumerate(coordinates))
```

`grid` collects exact Fraction coordinates; `visit_grid` visits them without
retaining a list. [Grid contract](../reference/SAMPLING_API.md#python-exact-grids).

## Bounded numerical roots

```python
root = numforge.find_root(lambda x, places: x*x-2, 1, 2, continuous=True)
assert root.kind == "found" and not root.exact_zero
assert abs(root.x*root.x-2) <= Fraction(1, 10**12)
```

```python
scan = numforge.find_roots(lambda x, places: x*(x-2)*(x+3), -4, 4, segments=8)
assert tuple(root.x for root in scan.roots) == (-3, 0, 2)
assert scan.kind == "finished" and not scan.complete
```

Callbacks receive exact Fraction coordinates and requested fractional scale.
`continuous=True` is the caller's continuity assertion, not an automatic proof.
`finished` means the grid was processed, never complete discovery.
[Single-root contract](../reference/ROOT_API.md#python-single-root-search) ·
[Scan contract](../reference/ROOT_API.md#python-multiple-root-scanning).

## Input, precision and results

Inputs: int, Fraction, finite Decimal, numeric literal strings and existing
Result. Float/complex float, bool, NaN/infinity and NUL are rejected. Strings
denote finite exact literals; use Fraction/evaluate for recurring fractions.
Existing approximate Results retain provenance.

`Result.kind` identifies storage; `.exact` reports proven exactness. `str(result)`
serializes stored digits, possibly scientifically. `.as_fraction()` copies exact
real values only; approximate/complex values raise TypeError. Interpreter
integer/text limits remain unchanged.

`precision=34` means 1–10000 working significant digits, not display places or a
certified error bound. Exact work stays exact; higher precision cannot repair
approximate inputs. Rounding: toward_zero, away_from_zero, floor, ceiling,
half_up, half_even (default). Expressions use radians; sessions/ans/assignment,
random calls, quantities and custom functions are outside this subset.

## Exact equation descriptors

```python
solution = numforge.solve_quadratic(1, 0, -2)
assert solution.kind == "finite" and solution.degree == 2
for root in solution.roots:
    assert root.exact and root.value is None
    print(root.center, root.sign, root.radicand)  # 0, -1/+1, 2
```

`solve_quadratic` accepts exact real coefficients. Frozen roots keep Fraction
center/radicand, sign, imaginary flag and multiplicity; optional typed value
exists for rational components. Irrational descriptors remain exact.
[Polynomial contract](../reference/POLYNOMIAL_API.md).

## Errors, lifetime and execution policy

C failures raise `numforge.Error` (RuntimeError) with string `status` and
zero-based UTF-8 `offset`; Python validation uses TypeError/ValueError. Failures
preserve arguments/reusable expressions. Python references own C capsules;
results outlive expressions/coefficient inputs.

Defaults: `time_limit_ms=10000`, `allocation_bytes=128*1024*1024`,
`single_allocation=8*1024*1024`. Zero denies work; None disables allocation
limits. Time fits nonnegative int64. C bytes are cumulative traffic, not live
RAM. Solver descriptor copying stays inside its scope; construction/preparation/
Fraction conversion have separate defaults. Serialization inherits result policy.
Python memory/conversion/finalizers are outside these cooperative limits.

Native calls retain the GIL. No parallel-native, instant Ctrl-C, PyPy or
free-threaded CPython guarantee. Callbacks are not preempted.

## Finite-interval integration

```python
area = numforge.integrate(lambda x, places: x*x, 0, 1)
assert area.kind == "estimated" and area.value == Fraction(1, 3)
assert area.error == 0 and not area.exact
limited = numforge.integrate(lambda x, places: x**4, 0, 1, max_evaluations=8)
assert limited.kind == "evaluation_limit" and limited.evaluations == 5
```

Callbacks return supported real samples or None for a gap; exceptions propagate.
Bounds/tolerances must be exact with lower<upper. Defaults: decimal_places=34,
absolute_tolerance=1E-12, max_refinements=12, max_evaluations=4097. Check kind;
Fraction value/error remain approximate even when estimated error is zero.
[Error meaning](../reference/NUMERICAL_API.md#finite-interval-integration).

## Reproduce installation verification

```sh
python scripts/check_python_package.py --output build/python-package-check
```

Use a fresh directory and installed build tools. The check builds an sdist/wheel
offline, installs locally and tests imports/examples from a separate isolated
consumer. Nothing is uploaded or installed globally. See [Testing](../TESTING.md#installed-packages-and-python).
