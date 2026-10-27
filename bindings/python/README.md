# NumForge Python adapter

Experimental source-built CPython adapter; not published to PyPI. Package
`0.1.0.dev0` has its own version; `numforge.core_version` identifies the C core.

```python
from fractions import Fraction
import numforge

assert numforge.evaluate("0.1+0.2").as_fraction() == Fraction(3, 10)
assert numforge.Matrix([[1, 2], [3, 4]]).determinant() == -2
root = numforge.sqrt(2, precision=1000)
assert not root.exact
```

From the repository root, install in a virtual environment with
`python -m pip install .`. Requires CPython 3.10+ development headers, CMake,
a C17 compiler and Python build tools; Windows uses compatible MSVC `/MD`.
The extension statically links NumForge and needs no running server/source tree.

[Build, API and working examples](../../docs/guides/PYTHON.md) ·
[Native source ownership](../../docs/design/ARCHITECTURE.md#source-ownership) ·
[Installation checks](../../docs/TESTING.md#installed-packages-and-python)

Add native files explicitly to this directory's CMake manifest. Mathematical
algorithms stay in C core/modules; adapters use public headers only.
