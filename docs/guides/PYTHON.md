# Python adapter

Status: experimental, implemented in current post-2.4.0 source. Package version
`0.1.0.dev0` is independent of the C core version reported by
`numforge.core_version`. No Python distribution has been published to PyPI and
existing published C/application archives do not include it.

## Build and install

Use CPython 3.10+ with development headers/import libraries, CMake 3.20+, a C17
compiler, setuptools and wheel. Create a virtual environment and run from the
repository root:

```sh
python -m pip install .
```

For an offline build with the build tools already installed:

```sh
python -m pip wheel . --no-build-isolation --no-deps --no-index --wheel-dir build/python-dist
```

Install the resulting platform-specific wheel by its filename. Source builds
reuse the project CMake targets with applications/tests/benchmarks disabled.
NumForge numeric/engine code is statically linked into the extension. No separate
NumForge DLL, server, browser assets, Node or repository source files are needed
at runtime. Ordinary C SDK builds keep `NUMFORGE_BUILD_PYTHON=OFF` (the default)
and do not need a Python installation.

Windows requires a compatible MSVC toolchain and the dynamic Visual C++ runtime
(`/MD`). A wheel is specific to its CPython ABI, OS and architecture; this is not
an abi3 wheel. Local execution has been checked on Windows x64 / CPython 3.12.
Keep the source/output path short on Windows: MSBuild can still reject deeply
nested paths even when Windows long-path support is enabled. The adapter keeps
its CMake cache directly under `build/nf-<interpreter-tag>` to reduce nesting.
The Python CI workflow adds Linux 3.10/3.12, Windows 3.12 and macOS 3.12 checks;
configured CI coverage is not a claim that those remote jobs have already run.

## Initial API

| Operation | Contract |
| --- | --- |
| `number(value)` | Owned typed C value from int, Fraction, finite Decimal or a numeric literal string; existing Result is reused |
| `evaluate(text, *, variables=None, precision=34, ...)` | Shared C scalar real/complex evaluator, optional explicit variable mapping |
| `prepare(text, parameters=())` | Owned prepared C expression; `.parameters` is the copied declaration order |
| `Expression.evaluate(arguments=(), *, precision=34, ...)` | Reuse the AST with positional values or an exact-name mapping |
| `sqrt(value, *, precision=34, ...)` | Principal root through the C typed evaluator, including exact rational roots and supported complex promotion |
| `solve_quadratic(a, b, c, *, ...)` | Exact real coefficients; linear/constant reduction, irrational/complex descriptors and multiplicities |
| `Matrix(rows, *, ...)` | Owned immutable dense rational matrix; exact C product, determinant and unique square-system solve |
| `describe(values, *, ...)` | Exact rational batch count/sum/extrema/mean and population/sample variance, copied into StatisticsSummary |

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

These are actual source-built Python APIs. The standalone example in
`examples/python/standalone.py` is tested after installing the wheel.

## Exact descriptive statistics

```python
s = numforge.describe([0, 0, 1])
assert s.count == 3 and s.exact
assert s.mean == Fraction(1, 3)
assert s.variance_population == Fraction(2, 9)
assert s.variance_sample == Fraction(1, 3)
assert numforge.describe([5]).variance_sample is None
```

The frozen `StatisticsSummary` also contains sum, minimum and maximum. Numeric
fields are copied Fraction values; a singleton's sample variance is None because
division by n-1 is undefined. Population variance divides by n. Repeated values
count separately, including repeated Result handles. Empty input raises ValueError.
Inputs accept the same lossless types as number() and must be exact real values;
missing values, binary floats, booleans and approximate/complex Results are rejected.
No input/native state is retained by the report.

`describe` accepts time_limit_ms, allocation_bytes and single_allocation with the
standard defaults. It has no precision parameter: mean and variances remain exact
even when recurring. Input conversion precedes the C summary scope; native copies,
computation and field serialization share it. Python object/Fraction conversion
is outside C accounting and retains interpreter integer-string limits. Native
calls retain the GIL. See the [complete C/Python contract](../reference/STATISTICS_API.md).

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

Rows must be nonempty and rectangular. Entries accept the same lossless input
types as `number()` but must be exact real values; approximate Result inputs and
complex values are rejected. Construction copies inputs, and operation results
outlive both input matrices. `to_rows()` returns copied tuples of Fraction;
`determinant()` returns Fraction; `.multiply(other)` and `@` return Matrix.
`.solve(right)` accepts a Matrix with one or more RHS columns and returns Matrix.
The native matrix storage has no mutating public operation.

`a.rank()` returns the exact coefficient rank. `a.solve_general(right)` accepts
a rectangular a and one RHS column, returning a frozen `LinearSolution` with
kind `none`, `unique` or `infinite`, rank/nullity, ascending pivot/free column
tuples and owned particular/nullspace matrix views. A consistent system has all
solutions `particular + nullspace*t`; free coordinates of particular are zero.
For example:

```python
a = numforge.Matrix([[0, 2, 4], [0, 3, 6]])
family = a.solve_general(numforge.Matrix([[10], [15]]))
assert (a.rank(), family.kind, family.nullity) == (1, "infinite", 2)
assert family.particular.to_rows() == ((0,), (5,), (0,))
assert family.nullspace.to_rows() == ((1, 0), (0, -2), (0, 1))
```

An inconsistent system returns kind `none` with particular=None; its nullspace
still describes the kernel of a. Nullity zero uses nullspace=None, since no
zero-column matrix is represented. Native views outlive the analysis wrapper and
input matrices. See the [complete system contract](../reference/LINEAR_SYSTEM_API.md).

Construction, `to_rows`, `determinant`, `multiply`, `solve`, `rank` and `solve_general` accept
`time_limit_ms`, `allocation_bytes` and `single_allocation` with the usual defaults.
Input `number()` conversions precede the matrix scope; conversion of native
results into Python Fraction objects is Python work outside C allocation accounting.
There is no precision option: every matrix result is exact. `@` uses default limits;
use `.multiply(other, ...)` to supply limits. Native calls retain the GIL.

Non-square/incompatible shapes raise Error with status `dimension_mismatch`.
A zero determinant succeeds; solve raises `singular` when the coefficient matrix
has no inverse, for both inconsistent and underdetermined systems. Matrix errors
have offset zero. Resource errors retain `time_limit`/`memory_limit`; Python
shape/type/policy validation raises ValueError/TypeError. No partial solution is
returned and both inputs remain usable after errors. The [matrix C contract](../reference/MATRIX_API.md)
describes the exact algorithm, ownership and initial scope.

## Input, precision and results

Inputs never pass through a Python float. Binary floats, built-in complex floats,
booleans and arbitrary coercible objects raise TypeError. Numeric input strings
use the C decimal/exponent literal syntax; they are not expressions. Use Fraction
or `evaluate("1/3")` for a recurring fraction. Strings and Decimal values explicitly
denote exact finite input; passing an existing approximate Result retains its
original provenance. NaN/infinity and embedded NULs are rejected.

`Result.kind` identifies integer/rational/decimal/complex-rational/complex-decimal
storage. `Result.exact` reports proven exactness. `str(result)` serializes the
stored value without additional display rounding, possibly in scientific notation.
`result.as_fraction()` copies exact real integers/rationals into Python Fraction;
it raises TypeError for approximate or complex results. Fraction creation is output
type conversion, not a second implementation of NumForge arithmetic. Python's
integer/text conversion limits still apply and are never changed globally.

`precision` is 1..10000 **working significant digits**, not decimal places,
display digits or a certified final-expression accuracy. Algorithms can use minimum
precision/internal guards. Exact operations stay exact. Increasing precision cannot
recover digits lost in an approximate input. Existing C transcendental/composed
complex limitations remain in effect. See [the engine contract](../reference/ENGINE_API.md).

`rounding` accepts `toward_zero`, `away_from_zero`, `floor`, `ceiling`, `half_up`
or `half_even` (default). All expressions use radians. Sessions, ans, assignment,
random calls, quantities and user-defined functions remain outside this subset.

## Exact equation descriptors

```python
solution = numforge.solve_quadratic(1, 0, -2)
assert solution.kind == "finite" and solution.degree == 2
for root in solution.roots:
    assert root.exact and root.value is None
    print(root.center, root.sign, root.radicand)  # 0, -1/+1, 2
```

The frozen solution has `kind` (`finite`, `none`, `all`), reduced `degree`, exact
Fraction `discriminant` when applicable, and a tuple of roots. A root has Fraction
`center`, nonnegative Fraction `radicand`, `imaginary`, `sign`, `multiplicity` and
optional typed `value`. It represents center +/- sqrt(radicand), multiplied by i
for the offset when imaginary. A single root has sign zero. `value` is available
when the components are rational; an irrational root retains an exact descriptor
and never silently becomes a decimal approximation. Approximate/complex coefficients
raise TypeError. No numerical scanning or higher-degree solving is implied.
See [the polynomial contract](../reference/POLYNOMIAL_API.md).

## Errors, lifetime and execution policy

C failures raise `numforge.Error`, a RuntimeError subclass, with stable string
`status` and zero-based UTF-8 byte `offset`. Python input/policy mistakes raise
TypeError/ValueError. A failed evaluation does not modify arguments and a prepared
expression remains reusable after a domain or budget error. Python references own
opaque C capsules and release native values/ASTs when the last reference disappears.
Results outlive their source expression and coefficients.

Evaluation/solve accept `time_limit_ms=5000`, `allocation_bytes=64*1024*1024` and
`single_allocation=512*1024`. Zero denies C work; None disables either allocation
limit. `time_limit_ms` must fit a nonnegative signed 64-bit value. C heap budgets
count cumulative requested bytes, not peak/live heap. Native solver descriptor
copying occurs inside the solver scope. Input construction, expression preparation
and `as_fraction()` conversion have separate default budgets. Python object memory,
Python type conversion and finalizers are not a hard process time/memory limit.
Serialization inherits the result's copied C policy and the engine output limit.

Native calls retain the GIL, serializing access to thread-confined C handles.
There is no promise of parallel native evaluation, instant Ctrl-C interruption,
PyPy or free-threaded CPython support. Work limits are cooperative.

## Reproduce installation verification

With setuptools/wheel/pip and a compiler already available:

```sh
python scripts/check_python_package.py --output build/python-package-check
```

Use a fresh output directory. The checker builds a source archive, extracts only
confined regular files, builds a wheel from the extracted source with network
lookup disabled, installs into a dedicated directory, and runs Python with `-I`
from a separate consumer folder. It asserts both Python and native module imports
come from the installed directory. The copied tests and standalone example have
no runtime access to the source package. Nothing is uploaded or globally installed.

The 43 adapter tests include an independent 1000-digit Decimal root reference,
six rounding modes, lossless inputs, parameter binding, aliases/lifetime,
exact/approximate provenance, diagnostics, budget recovery and solver degeneracies.
Matrix checks add independent Fraction determinant/product/known-solution oracles,
exact 1000-digit cancellation, ownership, shape/singularity and limit recovery.
General-system tests add independent minor/augmented-rank references, canonical
particular/kernel identities, rectangular classification and shared native lifetime.
Statistics adds independent pairwise Fraction variance, shift/scale/permutation
identities, singleton semantics, 1000-digit cases and serialization budget recovery.
Existing C tests remain the algorithm-level correctness evidence; this adapter
does not introduce another mathematical implementation or claim a speedup.
