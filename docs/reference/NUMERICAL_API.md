# Numerical root API

`<numforge/numerical.h>` exposes a single real-root search in the numeric target
`NumForge::numforge`. It needs neither an expression engine nor application state.
The header is authoritative for callbacks, precision, ownership and failures.

## Use and result meaning

Initialize options with `numforge_root_options_init`, then call
`numforge_root_solve(&result, evaluate, domain, user, lower, upper, &options)`.
Bounds and tolerances are borrowed; release the owned successful result with
`numforge_root_destroy`. Failure preserves the existing output. An optional
`runtime.h` budget is inherited and never reset or ended by the solver.

The evaluator receives an exact rational coordinate, a working decimal scale,
a borrowed decimal destination and initialized sample flags. Set `real` for a
finite real value, and `exact_zero` only for a mathematical proof, never rounded
zero. Undefined samples are diagnostics; fatal callback failures are status
errors. Caller-owned scratch state is allowed. Callbacks must cooperate with
inherited budgets; their execution is not preempted or isolated.

The domain callback supplies evidence of continuity on the complete final
interval. Missing/unknown evidence prevents approximate acceptance. Exact sampled
zeros require no continuity assumption. The solver cannot establish the truth of
an arbitrary callback's mathematical claims or bound its evaluation error.

| Result | Meaning |
| --- | --- |
| `FOUND` | Proven sampled zero or accepted approximate candidate. |
| `NOT_BRACKETED` | Endpoint samples did not establish opposite signs; roots may still exist. |
| `DOMAIN_GAP` | A required sample was undefined/non-real. |
| `DOMAIN_UNVERIFIED` | Approximate acceptance lacked continuity evidence. |
| `ITERATION_LIMIT` | Available iterations/evaluations did not establish convergence. |
| `PRECISION_LIMIT` | Rounded-zero probing or higher-precision verification was unstable. |

Only `FOUND` has `x` and absolute residual; every diagnostic result has final
bounds and work counts. Rational coordinates do not make an approximate root
exact. Approximate acceptance checks width, residual, strict improvement over
both initial endpoints, continuity and reevaluation at twice the working scale.
These checks do not establish a universal certified root-error bound.

Default width/residual tolerances are independently `1E-12`. Raising working
scale alone does not tighten them. For greater precision, supply positive
tolerances and raise working scale and work limits together. From width 2,
bisection needs about 3.322 iterations per requested decimal place, before
residual and verification requirements. The tests compare a 1000-place result
with the independent decimal square-root implementation.

## Examples and availability

The complete [C example](../../examples/sdk/numerical.c) solves `x*x-2=0` using
[rational callback intermediates](../../examples/sdk/numerical_function.h). The
standalone SDK example project builds/runs it. Installed C/C++ consumers separately
check callbacks, getters, ownership and exact sampled zeros without an engine.

The existing [HTTP equation workflow](NUMERIC_ROOT_HTTP_API.md) now delegates to
this kernel through private expression/domain adapters. Its limits, syntax,
scanning rules and response schema remain compatible. Multiple-root scans and
Brent/hybrid research remain private; public search currently uses bisection.
This slice adds no direct Python root function, CLI root command or new REST route.

## Cost and verification

High-precision rational bisection produces substantial cumulative allocation
traffic; requested bytes differ from peak live memory. Apply explicit budgets to
expensive workloads. `numerical_tests` covers references, exact zeros, continuity,
precision instability, invalid options/callbacks, limits, allocation failures,
inherited budgets and 1000-place accuracy. Existing application tests preserve
domain-hole, discontinuity and scanning regressions. The
[checked baseline](../benchmarks/NUMERICAL_2026_10_19.md) records costs, not a speedup.
