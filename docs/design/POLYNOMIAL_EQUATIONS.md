# Exact linear and quadratic equations

The public [numeric coefficient solver](../reference/POLYNOMIAL_API.md) solves
`a*x^2 + b*x + c = 0` for real rational coefficients in `NumForge::numforge`.
It preserves exact fractions and irrational radicals, including complex conjugate
roots. `src/application/equations/equation.h` retains private context/error/parsing/formatting
adapters over that implementation for existing application clients.

This is an implemented coefficient-based foundation with a stateless
[HTTP operation](../reference/EQUATION_HTTP_API.md). The numeric solver is now in
the source-built SDK; application adapters remain private. The [browser equation editor](../guides/EQUATIONS.md) consumes
this contract with exact root text and decimal previews. A separate bounded
[numerical interval solver](../reference/NUMERIC_ROOT_HTTP_API.md) handles general
real equation expressions without changing exact polynomial semantics.

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

`application_equation_solve_text` accepts one complete equation in lowercase x
with one `=`. It shares the calculator AST/parser and exact typed constant
evaluation. It converts both sides into three rational coefficients and subtracts
them before calling the same solver. Output ownership and budgets match the other
entry points; errors retain byte offsets in the original full input, including
the right-hand side. Input length is bounded to 4096 bytes and line breaks are
rejected. No AST is retained after solving.

The converter accepts unary signs, +, -, *, division by x-free constants,
powers 0..2 and postfix square. Every intermediate polynomial must remain degree
at most two; higher intermediate degrees are rejected rather than expanded and
cancelled later. Calls/exponents/denominators depending on x are unsupported.
This deliberately preserves domain restrictions: x/x is never cancelled.
Only lowercase x is available, independently of calculator sessions. A bounded
AST inspection rejects random calls and unresolved names before constant
evaluation, including expressions multiplied by zero. Resource failures remain
atomic; repeated AST inspections also poll the inherited deadline.

`application_equation_solve_expressions` evaluates three borrowed coefficient
strings in a,b,c order, then calls the same exact solver. The shared C evaluator
must produce exact real integer/rational values; approximate values, quantities
and complex typed results are rejected. Exact evaluation is enabled regardless
of the caller's display division policy. Assignments/random calls are forbidden;
no session variables or ans are available. All three evaluations and solving
share one budget. Optional `failed_coefficient` receives 0..2 for coefficient
failures or `SIZE_MAX` for success and non-coefficient errors; error offsets are
local to the identified coefficient. Ownership matches the typed solver.

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

`application_equation_format_root` returns an owned `ApplicationEquationRootText`
with exact and approximate strings; destroy it with
`application_equation_root_text_destroy`. Failures preserve the destination.
Rational-component roots use the shared complex formatter; irrational roots
use their stored exact center and radical without square-factor extraction.
Decimal components use `division_scale` significant digits, half-even rounding
and 24 working guard digits. Display places, notation and angle settings do not
change this preview policy. It is an approximation, not a certified interval or
a guarantee of correctly rounded last digits.

For real irrational roots, compute the root with the same sign as the center by
addition of magnitudes. Recover its partner from the exact rational root product
`center^2 - radicand` divided by that stable root, avoiding cancellation. Complex
roots project real and imaginary components separately. Formatting and previews
inherit the operation budget; HTTP includes both within its request scope.

The context is validated even for constant equations. Each operation uses the
existing application time limit (five seconds by default), cumulative 64 MiB
allocation budget and 512 KiB single-allocation limit. Nested calls inherit an
active outer budget and never reset or end it. Solving and materializing roots
are separate bounded operations unless the caller wraps them in one outer
scope. The HTTP adapter keeps coefficient evaluation, solving and successful
serialization within one request budget.

No session, variables, ans, history or random state is accessed. Resource errors
abort the operation without publishing partial output. `equation_tests` covers
degree reduction, fractions, repeated/complex/irrational roots, large cancelling
discriminant terms, borrowed aliases, invalid contexts, every allocation-failure
position and forced deadline/memory exhaustion. Known-root families also
substitute materialized roots into the original polynomial using independent
complex arithmetic. Allocation-stat builds verify zero live allocations.

## Next integration

1. The read-only coefficient-expression adapter and documented HTTP contract
   are implemented, preserving exact typed inputs and one request budget.
2. Exact algebraic text, cancellation-safe decimal previews, browser coefficient
   controls and a dedicated user guide are implemented around that contract.
3. Bounded numerical roots of general prepared expressions on an explicit
   sign-changing interval are implemented with residuals and convergence limits.
   Before accepting a non-exact candidate, exact rational interval enclosures
   conservatively check both original sides' local real domains/continuity.
   Unresolved domains publish no root (domain_unverified); no algebraic
   cancellation discards holes. Exact typed zeros keep their pointwise path.
   Non-exact candidates undergo a final original-expression reevaluation at
   twice the working precision; unstable residuals/signs publish no root.
   Production retains bisection after a private Brent comparison showed mixed
   costs, including flat-root/pole regressions.
   A third private hybrid uses bounded decimal interpolation weights and a
   permanent rational-bisection fallback on poor progress. Its faster ordinary
   cases do not imply unconditional promotion; production method selection is
   unchanged.
   Bounded scanning now discovers multiple brackets across a 128-cell rational
   grid with at most 32 stored roots/4096 coordinate pairs and one shared budget.
   Additional midpoint/local-quadratic samples discover simple rational contacts
   and some clustered pairs without accepting rounded minima. Scan coordinates
   retain canonical rationals across HTTP, including recurring fractions.
   Roots link to graphs of the original side difference. Finite sampling does
   not enumerate every root; general tangencies remain future work. See the
   [scan contract](../reference/NUMERIC_ROOT_HTTP_API.md#multiple-root-scan).
