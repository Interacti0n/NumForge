# Numerical API

`<numforge/numerical.h>` exposes exact rational grids, adaptive interval scheduling
and bounded real-root searches and integration; `<numforge/curve.h>` supplies sampled curve
acceptance policies and `<numforge/phase.h>` locates periodic-phase anchors.
All belong to `NumForge::numforge` and need neither an
expression engine nor application state.
The public headers are authoritative for callbacks, precision, ownership and failures.

| Task | Reference |
| --- | --- |
| Exact grids, adaptive scheduling, curve acceptance and periodic-phase anchors | [Grids and sampling](SAMPLING_API.md) |
| Single bracket search, multiple-root scanning and Python callbacks | [Root search](ROOT_API.md) |
| Finite-interval Simpson quadrature | [Integration](#finite-interval-integration) |

Choose a topic below for its detailed C/Python contract.

## Finite-interval integration

`numforge_integrate(&result, evaluate, user, lower, upper, options)` calls the
standalone C kernel. Bounds must be finite exact rationals with `lower < upper`;
reversed/equal bounds are rejected. It reuses `NumForgeRootEvaluate`'s sample
callback: write a finite BigDecimal and set `real=true`, or leave `real=false`
for an undefined sample. Borrowed coordinates/output/state and callback errors
follow the public header. No expression engine or continuity callback is needed.

The initial method is composite Simpson quadrature on globally halved grids,
reusing previous trapezoid sums and sampling each new coordinate once. It keeps
a fixed number of numeric objects. Coordinates and accumulation of finite
sample decimals use exact rational arithmetic; the **integral remains approximate**.
This is not local adaptive quadrature or symbolic integration.

| Option | Default | Meaning |
| --- | --- | --- |
| `decimal_places` | 34 | Positive callback working precision, not guaranteed output accuracy |
| `absolute_tolerance` | `1E-12` | Positive absolute integral-error threshold, in integral units |
| `max_evaluations` | 4097 | At least 5; includes endpoints and every callback invocation |
| `max_refinements` | 12 | At least 2; complete grid halvings |

For trapezoid sums `T`, the Simpson estimate is `S_2n=(4*T_2n-T_n)/3`.
After at least five samples, `error=abs(S_2n-S_n)/15` estimates discretization
error under the usual smoothness/asymptotic assumptions. See
[NIST's quadrature formulas](https://dlmf.nist.gov/3.5#ii).
The latest Simpson value is returned without a further extrapolation correction.
`estimated` means this heuristic is within tolerance. It excludes callback
error and is **not a certified bound**. Missed oscillations, discontinuities,
narrow peaks or rounded samples can produce a false small/zero estimate.
Supply accurate deterministic samples; neither zero error nor exact-zero samples
prove an exact integral. Raising precision cannot repair an undersampled grid.

`evaluation_limit` and `refinement_limit` retain the latest value/error. A full
next grid must fit the allowance, so slots may remain unused. `domain_gap` stops
at the first non-real sample and exposes no value/error, including when an earlier
grid succeeded. Endpoint singularities and infinite intervals are unsupported.
Resource/callback failures return no new result and preserve the destination;
inherited budget exhaustion takes precedence. Successful results are owned,
accessor values are borrowed until `numforge_integral_destroy`.

The [C SDK example](../../examples/sdk/numerical.c) integrates `x*x-2` over `[0,2]`.
Python's `integrate(function, lower, upper, **options)` delegates to the same C
kernel and returns a frozen `IntegralResult`: `kind`, `value`, `error`,
`evaluations`, `refinements`, and `exact=False`. Value/error are copied Fractions,
or None for a domain gap. See the executable [Python example](../guides/PYTHON.md#finite-interval-integration).
This capability currently has C/C++ and Python interfaces; no calculator syntax,
HTTP route or additional binding is provided.

`integral_tests` covers analytic polynomials, error/limit behavior, thousand-digit
scales, allocation failures and inherited budgets. Python adds seeded independent
antiderivatives, a Decimal logarithm reference, alias blindness and exception/
serialization recovery. Installed C/C++ consumers exercise the exported API.
The existing `numerical_sampling_benchmark integral` selector measures three
independently validated quartic fixtures; [measurement scope](../performance/baselines/SAMPLING.md#integration-measurement).

## Exact uniform grids

See [exact uniform grids](SAMPLING_API.md#exact-uniform-grids).

## Python exact grids

See [python exact grids](SAMPLING_API.md#python-exact-grids).

## Adaptive interval scheduling

See [adaptive interval scheduling](SAMPLING_API.md#adaptive-interval-scheduling).

## Sampled curve acceptance

See [sampled curve acceptance](SAMPLING_API.md#sampled-curve-acceptance).

## Periodic-phase anchors

See [periodic-phase anchors](SAMPLING_API.md#periodic-phase-anchors).

## Use and result meaning

See [use and result meaning](ROOT_API.md#use-and-result-meaning).

## Examples and availability

See [examples and availability](ROOT_API.md#examples-and-availability).

## Python single-root search

See [python single-root search](ROOT_API.md#python-single-root-search).

## Multiple-root scanning

See [multiple-root scanning](ROOT_API.md#multiple-root-scanning).

## Python multiple-root scanning

See [python multiple-root scanning](ROOT_API.md#python-multiple-root-scanning).

## Cost and verification

See [cost and verification](ROOT_API.md#cost-and-verification).
