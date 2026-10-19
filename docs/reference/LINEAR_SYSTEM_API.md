# Exact rank and general linear systems

Implemented in current post-2.4.0 source. Include `numforge/matrix.h` and link
`NumForge::numforge`; the API has no expression parser or application dependency.
The header's Doxygen comments define the full ownership/error contract. This
extends [exact matrices](MATRIX_API.md) without changing existing operations.

## Operations and outcomes

`numforge_matrix_rank(&rank, a)` returns the exact coefficient rank of any
positive rectangular rational matrix. The size_t destination changes only on
success. There is no precision option or numerical rank tolerance.

`numforge_matrix_solve_general(&solution, a, b)` analyzes a*x=b over the rationals.
If a is m-by-n, b must be m-by-1. Both underdetermined and overdetermined systems
are accepted. This operation accepts one RHS column; the existing square solver
continues to handle multiple RHS columns.

| Kind | Meaning | particular | nullspace |
| --- | --- | --- | --- |
| `NUMFORGE_LINEAR_NONE` | The exact system is inconsistent | NULL | Basis of ker(a), if nullity>0 |
| `NUMFORGE_LINEAR_UNIQUE` | Consistent, rank(a)=n | n-by-1 solution | NULL |
| `NUMFORGE_LINEAR_INFINITE` | Consistent, rank(a)<n | n-by-1 particular solution | n-by-nullity basis of ker(a) |

Every mathematical outcome returns `NUMFORGE_MATRIX_OK`. Always inspect kind
before interpreting a result as a solution. Nullity is n-rank(a) even for an
inconsistent system: a kernel can exist when the original system has no solution.
Zero-column matrices are not represented, so nullspace is NULL when nullity=0.

For consistent systems every rational solution is

```text
x = particular + nullspace * t
```

where t is an arbitrary nullity-by-1 rational vector. If nullity=0, particular is
the only solution. This is a complete exact parameterization, not a sampled
search, approximation or least-squares fit.

## Canonical coordinates and algorithm

Pivot and free coefficient column indices are zero-based and ascending. The
particular solution sets all free coordinates to zero. Nullspace column j assigns
one to free coordinate j and zero to the other free coordinates, then solves for
the pivot coordinates. Thus the free rows of the basis form identity; basis
columns are independent and contain exactly nullity free parameters. This basis
is not normalized or orthogonal.

The shared forward-elimination implementation scans columns left to right,
selects the first exactly nonzero pivot among remaining rows, exchanges rows and
eliminates entries below it. Columns without a pivot are skipped. The transformed
RHS is inconsistent if a zero coefficient row has a nonzero RHS. Shared back
substitution constructs the particular solution and all homogeneous basis
columns. Rank uses this same elimination without a RHS or output family.

All fractions stay exact. In particular, cancellation in coefficients with 1000
digits does not cause a tolerance-based loss of rank. Complexity and reduced
fraction growth retain the initial [matrix implementation's limits](MATRIX_API.md).
Separate rank/solve calls repeat elimination; no persistent factorization is cached.

## C handles and errors

`NumForgeLinearSolution` is opaque and immutable. Its queries are:

| Query | Return value / NULL handle behavior |
| --- | --- |
| `numforge_linear_solution_kind(s)` | Kind, `NUMFORGE_LINEAR_INVALID` for NULL |
| `numforge_linear_solution_rank(s)` | Coefficient rank, zero for NULL |
| `numforge_linear_solution_nullity(s)` | n-rank(a), zero for NULL |
| `numforge_linear_solution_pivot_column(s, i)` | Pivot column i, SIZE_MAX for NULL/out-of-range |
| `numforge_linear_solution_free_column(s, i)` | Free column i, SIZE_MAX for NULL/out-of-range |
| `numforge_linear_solution_particular(s)` | Borrowed matrix, NULL for NONE/NULL |
| `numforge_linear_solution_nullspace(s)` | Borrowed matrix, NULL for zero nullity/NULL |

Destroy the owned analysis with `numforge_linear_solution_destroy`, accepting
NULL. Its borrowed matrices and entries remain valid until analysis destruction;
never destroy or mutate them independently. Queries do not allocate. Inputs are
copied and may alias each other; results outlive the input matrices. Independent
handles can be used independently; synchronize shared handles externally.

Success assigns a new owned analysis and never frees a previous destination.
Failure preserves an existing destination, including resource/numeric failure.
NULL required arguments return `NULL_ARGUMENT`; mismatched rows or b.columns!=1
return `DIMENSION_MISMATCH`. Allocation, numeric size/scale and cooperative budget
failures use the existing matrix statuses. SINGULAR is retained by the old
square solve, but is not a mathematical outcome of solve_general.

No implicit budget is started. All input copies, elimination, consistency checks,
particular and kernel construction inherit the caller's runtime scope without
resetting/ending it. A resource failure publishes no partial classification or
family. OUT_OF_MEMORY also represents cooperative exhaustion; runtime budget
queries distinguish time and memory. These limits do not provide process isolation.

The complete [C SDK example](../../examples/sdk/linear_system.c) constructs
A=[[0,2,4],[0,3,6]] and b=[[10],[15]], then reports rank=1, nullity=2 and
the family (x,y,z)=(s,5-2*t,t). Build it with the installed-package commands in
[the matrix guide](MATRIX_API.md#c-example).

## Python

The source-built [Python adapter](../guides/PYTHON.md) uses the public C operations:

```python
import numforge

a = numforge.Matrix([[0, 2, 4], [0, 3, 6]])
family = a.solve_general(numforge.Matrix([[10], [15]]))
assert a.rank() == 1
assert (family.kind, family.rank, family.nullity) == ("infinite", 1, 2)
assert family.pivot_columns == (1,)
assert family.free_columns == (0, 2)
assert family.particular.to_rows() == ((0,), (5,), (0,))
assert family.nullspace.to_rows() == ((1, 0), (0, -2), (0, 1))

none = a.solve_general(numforge.Matrix([[10], [16]]))
assert none.kind == "none" and none.particular is None
assert none.nullspace is not None  # ker(a) is independent of the RHS
```

`LinearSolution` is a frozen result with kind `none`/`unique`/`infinite`, rank,
nullity, pivot_columns/free_columns tuples, particular/nullspace matrices and
exact=True. Native matrix values retain shared ownership and remain usable after
the result wrapper and inputs are released. The matrices are immutable and work
with the other Matrix methods. `rank()` returns int.
Pivot/free columns are ascending zero-based tuples. Consistent systems represent
all solutions as `particular + nullspace*t`, with zero free coordinates in
particular. An inconsistent system has particular=None but retains its kernel;
nullity zero uses nullspace=None because zero-column matrices are not represented.

Both methods use the [matrix input, budget and error rules](MATRIX_API.md#python-and-other-interfaces),
with no precision parameter. Inconsistency is a successful `none` result.

Matrix expression syntax, CLI/web controls, public REST and WASM bindings remain
future interfaces. Multiple-RHS general classification, approximate least
squares, eigenvalues and orthogonal decompositions are outside this operation.

## Verification

`linear_system_tests` adds rectangular/rank-zero cases, skipped and exchanged
pivots, canonical basis/particular values, overdetermined unique/inconsistent
systems, a kernel for inconsistent systems, aliasing/lifetime, 1000-digit exact
cancellation, NULL queries and atomic allocation/deadline failures. The existing
matrix tests protect square solve/determinant contracts after sharing elimination.

Python adds an independent Fraction oracle based on nonzero minors and augmented
matrix rank, with 24 rational fixtures across six rectangular/square shapes.
Canonical free rows, independent basis rank, A*N=0 and A*x=b are checked directly
using Fraction arithmetic. Lifetime and limit-recovery tests cover native views.
Installed C/C++ consumers and SDK examples exercise numeric-only builds. See the
[verification and benchmark report](../benchmarks/MATRIX_SYSTEMS_2026_10_19.md).
