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

All six C rounding modes are supported by name: `toward_zero`, `away_from_zero`,
`floor`, `ceiling`, `half_up`, `half_even`. C failures raise `numforge.Error` with
stable `status` text and zero-based UTF-8 byte `offset`. Python argument errors
use TypeError/ValueError. Cooperative time/allocation budgets cover C operations;
Python object memory and input conversion are not a hard process limit. Native
calls retain the GIL; this initial adapter does not promise parallel C execution,
instant Ctrl-C cancellation, PyPy or free-threaded CPython support.

See the full source guide `docs/guides/PYTHON.md` for contracts, installation
verification and the scope of the initial adapter.
