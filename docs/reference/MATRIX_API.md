# Exact rational matrices

Exact rational products, determinants, rank and linear-system solutions.
Use the browser editor below, or the C/Python APIs further down.

## Web editor and HTTP

Open `/matrices?lang=en` (or `sk`). Choose determinant, rank, multiplication,
square `A X = B` or **General system**, edit dimensions/cells, then Calculate. Pasting tab-separated rows
replaces that input matrix. Inputs survive navigation/reload in the current
tab's sessionStorage; results are not stored on the server. Matrix copies use TSV.
On phones, successful calculation closes the input keyboard and reveals the result.
General systems distinguish unique, infinite and inconsistent outcomes, including
rectangular and singular matrices. B must have exactly one column.

Zero, identity (square inputs only), transpose and A/B swap edit inputs without
rounding. Enter/Shift+Enter moves down/up a column, Alt+arrows moves between
cells, and Ctrl/Cmd+Enter calculates. Tab retains normal focus traversal.
Examples include unique, infinite, inconsistent and exact-fraction cases.

Desktop grids occupy the left column, with results/recent tasks on the right;
large grids and results scroll internally. **Recent matrices** keeps up to eight
completed input sets in the current tab. Restore recovers both grids and the
operation without calculating, and cancels older pending work. Entries can be
removed or cleared independently of the current inputs; results are not stored.
The [shared input-history limits](../design/WEB_DESIGN.md#shared-workbench-layout) apply.

Save/Load workspace uses local UTF-8 JSON: `format:"numforge.matrix-workspace"`,
`version:1`, `operation`, and rectangular string arrays `a`/`b`. File limit is
20,000 bytes; dimensions/cell limits below apply. Draft invalid/empty cell text
can be saved but must be corrected before calculation. Unknown versions, malformed
shapes, control characters in cells or oversized files leave current inputs unchanged. No file is uploaded;
only matrix text is sent when Calculate is chosen. Results are not in the file.

`### HTTP

POST /api/matrix?operation=determinant|rank|multiply|solve|solve_general` is stateless and
shares the server's Content-Length/origin rules. Send `text/plain`: whitespace
separates cells, LF separates rows, and one empty line separates A from B.
CRLF is accepted. Unary operations ignore optional B. For example:

```sh
curl -H 'Content-Type: text/plain' --data-binary '1 2
3 4' 'http://127.0.0.1:8765/api/matrix?operation=determinant'
```

Returns `{"ok":true,"exact":true,"scalar":"-2"}`. Rank also returns `scalar`;
multiply/solve return `matrix`, an array of row arrays containing canonical
rational strings, for example `[["17"],["39"]]`. No rounding or preview precision.

`solve_general` instead returns `solution` with `kind:"none"|"unique"|"infinite"`,
integer `rank`/`nullity`, `particular` (n×1 or null for none), `basis` (n×nullity
or null when nullity=0), and ascending zero-based `free_columns`. The C
[general-system contract](LINEAR_SYSTEM_API.md) owns their mathematical meaning.
For A=`1 2` and B=`3`: particular=`[["3"],["0"]]`, basis=`[["-2"],["1"]]`,
free_columns=`[1]`; every rational solution is `[3;0]+t1·[-2;1]`.
An inconsistent system is a successful mathematical outcome, not HTTP 400.
The browser shows `x = x₀ + N·t` for infinite families, with arbitrary rational
parameters; its family copy is a vector expression rather than TSV.

Adapter limits: 1–8 rows/columns, 64 bytes per input cell, 6000 body bytes,
128 KiB response, and inherited default calculation time/allocation budgets.
Accepted cells: signed integers, integer fractions with nonzero denominators,
finite dot decimals. Exponents, expressions, variables and units are rejected.
The standalone C/Python APIs retain their existing broader size/type contracts.

Errors return HTTP 400 with `ok:false`, `status`, `error`, and zero-based
`matrix` (0=A, 1=B) / row-major `cell` for parse failures, otherwise -1.
Bad syntax/shape uses `invalid argument`; zero denominator or singular square
solve uses `division by zero`. The existing square `solve` keeps this contract;
use `solve_general` for classification. Time, allocation and
size failures preserve inputs and publish no partial result. Shared origin,
framing and worker-overload handling can instead return 403/411/413/503.

## Initial operations

Include `numforge/matrix.h` and link `NumForge::numforge`; no engine, session or
server is required. Published 2.4.0 archives predate this API. Installed-header
Doxygen comments define the package's contract.

| Operation | Result and domain |
| --- | --- |
| `numforge_matrix_create(&m, rows, columns, entries)` | Deep copy a row-major array of borrowed BigRational pointers; positive dimensions |
| `numforge_matrix_rows(m)`, `numforge_matrix_columns(m)` | Shape, zero for a NULL handle |
| `numforge_matrix_entry(m, row, column)` | Borrowed immutable rational, NULL for invalid handle/index |
| `numforge_matrix_multiply(&product, a, b)` | Exact product, a.columns must equal b.rows |
| `numforge_matrix_determinant(value, a)` | Exact determinant into an initialized BigRational, square a |
| `numforge_matrix_solve(&x, a, b)` | Unique exact solution of a*x=b; square invertible a, matching rows, one or more RHS columns |
| `numforge_matrix_rank(&rank, a)` | Exact coefficient rank of a rectangular matrix |
| `numforge_matrix_solve_general(&solution, a, b)` | [Complete rectangular single-RHS solve](LINEAR_SYSTEM_API.md), including inconsistency and a nullspace basis |
| `numforge_matrix_destroy(m)` | Release all entries and storage; NULL accepted |

All arithmetic is exact over the rationals. Reduced entries have positive
denominators; no decimal rounding, precision option, binary float conversion,
pivot tolerance or approximate-zero test occurs. A determinant of zero is a
successful exact result. There is no ambiguity caused by near-singularity:
`[[N,N+1],[N-1,N]]` has determinant one even for N=10^1000.

The solver selects the first exactly nonzero pivot, exchanges rows, performs
rational Gaussian elimination and back substitution. It shares elimination with
the determinant operation. It does not form an inverse to solve a system. Multiple
RHS columns share elimination in one call; separate calls repeat factorization.
Classical arithmetic work grows cubically for square matrices. Numerator and
denominator growth/GCD reduction can dominate its cost; this is an initial dense
exact implementation, with no sparse representation or fraction-free optimization.

## Ownership, failures and limits

Matrices own immutable entries. Construction copies every value, so callers may
free or change their original scalar objects after success. An entry accessor
returns read-only storage valid until matrix destruction: never mutate/destroy it
or use it as a numeric output. Results retain no dependency on input matrices.

Matrix-producing calls allocate new handles. Every failure preserves the caller's
destination slot, including an existing non-NULL handle. Success never frees a
previous destination. Inputs can alias each other; a destination slot may initially
hold an input handle if the caller retains and later frees that old handle. The
determinant's scalar output must be independently owned; it too is unchanged on
failure. Shared handles require external synchronization.

| Status | Meaning |
| --- | --- |
| `OK` | A complete exact result was produced |
| `NULL_ARGUMENT` | A required pointer or constructor entry is NULL |
| `INVALID_ARGUMENT` | A zero dimension or invalid underlying numeric argument |
| `DIMENSION_MISMATCH` | Product, square-matrix or RHS row requirements fail |
| `SINGULAR` | The solver's coefficient matrix has no inverse |
| `OUT_OF_MEMORY` | Allocation failed or an inherited cooperative budget was exhausted |
| `VALUE_TOO_LARGE` | Dimension/pointer-storage overflow or underlying numeric size failure |
| `SCALE_OVERFLOW` | Underlying numeric scale overflow |

Status names have the `NUMFORGE_MATRIX_` prefix. Singular means no **unique**
solution; it deliberately does not distinguish inconsistency from infinitely many
solutions. Use [solve_general](LINEAR_SYSTEM_API.md) for classification,
coefficient rank and a complete particular-plus-nullspace representation.

No implicit time/allocation budget is started. Library callers can use
`numforge_budget_begin` from `runtime.h`, query `numforge_budget_failure` to
distinguish time/memory exhaustion, and end only the scope they own. Nested matrix
operations do not reset counters or end the caller's scope. Allocation volume is
cumulative requested bytes, not live memory; no scope means no library limits.
Dimension arithmetic is checked before storage/input access, but there is no fixed
matrix-size cap. Budgets are cooperative, not process isolation.

## C example

The complete checked example is [examples/sdk/matrix.c](../../examples/sdk/matrix.c).
It constructs A=[[0,2],[3,4]] and b=[[7],[15]], then calls:

```c
NumForgeMatrix *x = NULL;
/* a and b are owned matrices; determinant is an initialized BigRational. */
NumForgeMatrixStatus status = numforge_matrix_determinant(determinant, a);
if (status == NUMFORGE_MATRIX_OK)
    status = numforge_matrix_solve(&x, a, b);
/* Check status before inspecting x. det(A)=-6; x=[1/3; 7/2], all exact. */
numforge_matrix_destroy(x);
```

Build the example against an installed package:

```sh
cmake -S examples/sdk -B build/matrix-example -DCMAKE_PREFIX_PATH=/path/to/installed
cmake --build build/matrix-example --config Release
ctest --test-dir build/matrix-example -C Release --output-on-failure
```

## Python and other interfaces

The source-built Python adapter exposes the same C algorithms. Start with the
[matrix workflow](../guides/PYTHON.md#exact-matrices) for a working example.

Rows must be nonempty and rectangular. Entries accept the lossless types of
[`number()`](../guides/PYTHON.md#input-precision-and-results) and must be exact real
values. Construction copies inputs; immutable results outlive their input matrices.
`shape` is a (rows, columns) tuple and `exact` is True. `to_rows()` returns copied
tuples of Fraction, `determinant()` returns Fraction, and `.multiply(other)`/`@`
return Matrix. `.solve(right)` accepts one or more RHS columns and returns Matrix.
For rectangular rank and complete families, see
[`rank()` and `solve_general()`](LINEAR_SYSTEM_API.md#python).

Construction, `to_rows`, `determinant`, `multiply`, `solve`, `rank` and
`solve_general` accept `time_limit_ms`, `allocation_bytes` and `single_allocation`
with the [standard defaults](../guides/PYTHON.md#errors-lifetime-and-execution-policy).
Input `number()` conversions precede the matrix scope; Python object/Fraction
conversion is outside C budget accounting and retains interpreter integer-string
limits. There is no precision option: every matrix result is exact. `@` uses
default limits; use `.multiply(other, ...)` for explicit limits. Native calls retain
the GIL.

Non-square/incompatible shapes raise Error(status="dimension_mismatch"). A zero
determinant succeeds; `.solve()` raises Error(status="singular") when the
coefficient matrix has no inverse, including inconsistent and nonunique square
systems. Native errors have offset zero; resource errors retain `time_limit`/
`memory_limit`, while Python shape/type/policy validation raises ValueError/TypeError.
Failures return no partial solution and both inputs remain usable.

Matrix expression syntax, CLI controls and WASM bindings remain unavailable.
The bounded web/HTTP adapter below reuses these C operations. Approximate/complex matrix domains,
eigenvalues, decompositions and symbolic elements remain future work.

## Verification

`matrix_tests` exercises rectangular multiplication, rational determinants,
row exchanges, multiple RHS columns, singular/degenerate cases, 1000-digit
cancellation, ownership, aliases, overflow, allocation failures at each allocation
and deterministic budget exhaustion. Installed C/C++ consumers link only the
numeric target. Python tests independently check random 1..4-dimensional rational
determinants by permutation expansion, products and recovered known solutions
using Python Fraction arithmetic. The source archive/wheel check verifies the
installed adapter and example.

`matrix_adapter_tests` covers exact text/HTTP results, invalid domains, dimensions,
cell positions, allocation failures through square/general solve and general
serialization, and inherited time/memory exhaustion. Browser tests cover SK/EN
solution families, editing, keyboard navigation, TSV/file round trips, rejected
imports, navigation/reload, mobile overflow, retry, malformed/stale replies and origins.

The opt-in [benchmark](../performance/baselines/MODULES.md#matrices) records an initial
Release baseline, validates exact closed-form results outside timing and reports
allocation counts/volume. No speedup relative to another library is claimed.
The [general-system follow-up](../performance/baselines/MODULES.md#general-linear-systems) adds
rank and complete-family measurements under the same exact arithmetic contracts.
