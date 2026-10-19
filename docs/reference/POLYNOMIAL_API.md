# Exact polynomial solver C API

Status: implemented in current post-2.4.0 source. Published 2.4.0 SDKs predate this
API. Include `<numforge/polynomial.h>` and link **NumForge::numforge**. The parser,
`NumForge::engine`, application context, sessions and HTTP are not required.

## Solve and inspect exact roots

`numforge_polynomial_solve(&solution, a, b, c)` solves a*x^2+b*x+c=0 with borrowed
real BigRational coefficients. Coefficient aliases are accepted; all inputs are
preserved. The result owns its exact descriptors. `numforge_polynomial_destroy()`
releases it and accepts NULL. The compiled SDK example `examples/sdk/polynomial.c`
builds coefficients for x^2-2=0 and prints exact center=0 and radicand=2.

| Query | Meaning |
| --- | --- |
| `numforge_polynomial_kind()` | NONE, FINITE, ALL; NULL gives INVALID |
| `numforge_polynomial_degree()` | Reduced degree 0..2; NULL gives 0 |
| `numforge_polynomial_root_count()` | Distinct complex roots; 0 for NONE/ALL/NULL |
| `numforge_polynomial_root_multiplicity()` | 1 or 2 for a valid root; otherwise 0 |
| `numforge_polynomial_center()` | Exact rational center for FINITE |
| `numforge_polynomial_radicand()` | Exact nonnegative rational offset squared |
| `numforge_polynomial_is_imaginary()` | Whether to multiply the offset by i |
| `numforge_polynomial_rational_offset()` | Nonnegative rational square root of radicand, or NULL if irrational/inapplicable |
| `numforge_polynomial_discriminant()` | Exact b*b-4*a*c for degree 2; otherwise NULL |

For two roots, index 0 is center minus the offset and index 1 plus the offset.
The offset is sqrt(radicand), or i*sqrt(radicand) for a negative discriminant.
A single root is center, with its recorded multiplicity. Borrowed descriptors
are read-only and valid until solution destruction. Copy them using BigRational
operations to obtain independent ownership. Their form is exact but not promised
to be a canonical symbolic simplification.

`numforge_polynomial_root_exact(&root, solution, index)` produces a new owned
BigRationalComplex for a root with rational real/imaginary components. A real root
has imaginary zero. Destroy it with `bigrationalcomplex_destroy()`.
An irrational offset returns BIGRATIONAL_INVALID_ARGUMENT and preserves the output;
check the offset accessor to distinguish this from an invalid root index. Its
exact radical descriptor remains available; no approximation is substituted.

## Domains, status and precision

Zero leading coefficients reduce the equation to linear or constant form. A
nonzero constant returns NONE; the identically zero polynomial returns ALL.
These outcomes describe the full complex solution set, not convergence failures.
Rational, irrational real and nonreal conjugate quadratic roots are represented.
Repeated roots have one distinct root and multiplicity two. No general polynomial
parser, higher-degree solver, numerical scanning or symbolic assumptions are added.

Solve and root materialization return **BigRationalStatus**. NULL_ARGUMENT means
a required pointer was absent; INVALID_ARGUMENT covers invalid root materialization;
OUT_OF_MEMORY covers allocation/runtime-budget failure; arithmetic can propagate
representation errors such as VALUE_TOO_LARGE or SCALE_OVERFLOW. Enum status text
comes from `bigrational_status_to_string()`. Destination pointers and coefficients
are unchanged on failure. Successful calls assign a new handle without freeing a
previous destination. NULL/lifetime/thread rules match the other numeric APIs.

This exact solver takes no precision parameter and starts no implicit budget.
It imposes no engine digit/input cap. Install an optional caller-owned
[runtime scope](C_API.md#optional-resource-scope) to limit expensive inputs.
Budget exhaustion is reported as OUT_OF_MEMORY; query numforge_budget_failure()
before ending the scope to distinguish deadline and allocation limits. Outer
scope ownership/counters are preserved. Independent solutions may be used on
separate threads; shared use requires caller synchronization.

Decimal projection remains a separate operation using BigRational/BigDecimal
APIs. The application's existing guarded/stable root previews retain their
documented approximation contract and are not promoted to certified intervals.
The exact rational-square-root proof is shared by calculator functions and this
solver; the application calls this public solver through a policy/error adapter.

## Verification and performance

`polynomial_tests` links only the numeric target and checks known rational roots,
irrational/complex descriptors, multiplicities, constant/linear reductions,
1000-digit coefficients, aliasing, invalid queries, output preservation,
allocation failures, deadline injection and absence of implicit resource scopes.
Existing equation, numeric-root, calculator and HTTP tests protect application
behavior. Installed C/C++ consumers use only public headers and the numeric archive.

The opt-in `polynomial_benchmark` runs ten repetitions of rational, irrational,
complex and 1000-digit-coefficient cases, reporting mean milliseconds, allocation
calls and cumulative requested bytes (not peak live memory). Exact root
sum/product invariants are checked outside timing. It measures the extracted
existing algorithm, including numeric allocations but excluding coefficient setup,
descriptor verification and destruction; each solve has a 5-second / 64-MiB
cumulative / 512-KiB single-allocation scope. No speedup is claimed.
The [initial Windows/GCC baseline](../benchmarks/PLATFORM_SOLVER_2026_10_19.md)
records three runs, allocation traffic and validation scope.

CLI/Web/local HTTP already expose equation tools through the application adapter;
their syntax, formatting and limits are unchanged. The source-built
[Python adapter](../guides/PYTHON.md) exposes the exact coefficient solver. WASM
and a new public REST service remain future work. See [the existing equation design](../design/POLYNOMIAL_EQUATIONS.md).
