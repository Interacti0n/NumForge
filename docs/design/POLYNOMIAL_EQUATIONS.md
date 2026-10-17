# Exact linear and quadratic equations

The private C application layer solves `a*x^2 + b*x + c = 0` for real rational
coefficients. It preserves exact fractions and irrational radicals, including
complex conjugate roots. Declarations are in `src/application/equation.h` and
the implementation belongs to `numforge_application`.

This is an implemented coefficient-based foundation. It is not an installed
numeric SDK API or an HTTP operation. The browser Equations page remains an
informational placeholder. Parsing arbitrary equations, decimal root previews
and interactive equation controls are subsequent work.

## C operations and ownership

`application_equation_solve(&solution, a, b, c, &context, &error)` accepts
borrowed `BigRational` coefficients. Inputs may alias each other and remain
unchanged. It returns an owned `ApplicationEquationSolution`; release it with
`application_equation_destroy`. Fields are read-only to callers. Destroying a
null solution is safe.

Initialize output pointers to null. A successful call publishes a new object;
it does not destroy an existing destination. On failure the destination pointer
and inputs remain unchanged, including allocation failures and expired budgets.
`CalculatorError` reports the status with position zero because this operation
has no expression text or parser position.

`application_equation_root_exact(&root, solution, index, &context, &error)`
materializes a new owned `BigRationalComplex` when the root has rational real
and imaginary components. Release it with `bigrationalcomplex_destroy`. The
same output ownership and failure rules apply. An irrational radical returns
`CALCULATOR_NOT_IMPLEMENTED` from this accessor; its exact algebraic value is
still available in the successfully solved equation. Invalid root indices and
requests for roots of NONE/ALL results return `CALCULATOR_INVALID_ARGUMENT`.

## Degree reduction and exact representation

| Coefficients | Result |
| --- | --- |
| `a = b = c = 0` | `APPLICATION_EQUATION_ALL`: every complex number satisfies the equation. |
| `a = b = 0`, `c != 0` | `APPLICATION_EQUATION_NONE`: no solutions. |
| `a = 0`, `b != 0` | Degree one, one root `-c/b`, multiplicity one. |
| `a != 0` | Degree two, discriminant `D = b*b - 4*a*c`. |

NONE/ALL have degree and count zero, with null root fields. Finite solutions
contain `center`, nonnegative `radicand`, an `imaginary` flag, root count and
multiplicities. Quadratic solutions additionally retain the exact discriminant.

For quadratics, `center = -b/(2*a)` and `radicand = abs(D)/(2*a)^2`:

- `D > 0`: two real roots `center - sqrt(radicand)` and
  `center + sqrt(radicand)`, in increasing order, each with multiplicity one.
- `D < 0`: two roots `center - i*sqrt(radicand)` and
  `center + i*sqrt(radicand)`, each with multiplicity one.
- `D = 0`: one root equal to `center`, with multiplicity two.

Index zero selects the minus root and index one the plus root. A single root
uses index zero. Linear roots have zero radicand, zero offset and no
discriminant. The optional `offset` is the exact nonnegative rational square
root of `radicand`, or null when that square root is irrational. Radicals remain
exact without extracting square factors or promising a canonical symbolic form.

| Equation | Stored solution |
| --- | --- |
| `3*x - 2 = 0` | `center = 2/3`, one simple root. |
| `4*x^2 + 12*x + 9 = 0` | `center = -3/2`, one double root. |
| `x^2 - 2 = 0` | `center = 0`, `radicand = 2`, real roots, no rational offset. |
| `9*x^2 - 6*x + 5 = 0` | `center = 1/3`, `radicand = 4/9`, `offset = 2/3`, imaginary roots. |
| `3*x^2 + x + 1 = 0` | `center = -1/6`, `radicand = 11/36`, imaginary roots, no rational offset. |

All arithmetic is rational and exact. Division/display precision and angle
units do not round the coefficients, discriminant or roots. A caller supplying
a rational converted from an approximate value gets roots of that supplied
rational polynomial; this operation does not recover its mathematical origin.

## Resource limits and verification

The context is validated even for constant equations. Each operation uses the
existing application time limit (five seconds by default), cumulative 64 MiB
allocation budget and 512 KiB single-allocation limit. Nested calls inherit an
active outer budget and never reset or end it. Solving and materializing roots
are separate bounded operations unless the caller wraps them in one outer
scope. A future HTTP adapter must keep coefficient evaluation, solving,
formatting and serialization within one request budget.

No session, variables, ans, history or random state is accessed. Resource errors
abort the operation without publishing partial output. `equation_tests` covers
degree reduction, fractions, repeated/complex/irrational roots, large cancelling
discriminant terms, borrowed aliases, invalid contexts, every allocation-failure
position and forced deadline/memory exhaustion. Known-root families also
substitute materialized roots into the original polynomial using independent
complex arithmetic. Allocation-stat builds verify zero live allocations.

## Next integration

1. Add a read-only coefficient-expression adapter and documented HTTP contract,
   preserving exact typed inputs and one request budget.
2. Format exact algebraic roots and add optional cancellation-safe decimal
   projections, then build the equation UI and user guide around that contract.
3. Add bounded numerical roots of general prepared expressions on an explicit
   interval, with residuals and convergence limits; connect those results to
   graph navigation later. Finite sampling does not enumerate every root.
