# Numerical API

`<numforge/numerical.h>` exposes exact rational grids, adaptive interval scheduling
and bounded real-root searches in `NumForge::numforge`. It needs neither an
expression engine nor application state.
The header is authoritative for callbacks, precision, ownership and failures.

## Exact uniform grids

Call `numforge_grid_visit(lower, upper, count, visit, user)` to visit an inclusive,
equally spaced grid in increasing order. Bounds must satisfy `lower < upper` and
`count >= 2`. Each borrowed coordinate is exactly
`lower + index*(upper-lower)/(count-1)`; no binary or decimal rounding is involved.
The callback can evaluate a function or collect coordinates in its own format.
The [standalone C example](../../examples/sdk/numerical_grid.c) computes exact
`x*x` values at `0`, `1/3`, `2/3` and `1`.

The walker retains no output array and uses a constant number of scratch objects;
their byte size and arithmetic cost depend on the bounds and count. Callbacks own
their outputs, error state and value-accuracy policy. Exact coordinates do not
make approximate evaluations exact or prove continuity between points. The
walker stops on the first callback/resource failure; earlier callback effects
remain. Bounds are preserved. An inherited budget is checked before and after
callbacks, which must cooperate with cancellation themselves.

Private uniform graph sampling and the 33-point adaptive seed grid share this
kernel. Typed expression evaluation, phase tracking, adaptive acceptance and
screen projection remain private; the root scanner retains its own bounded
candidate traversal. There is no new Python, CLI or REST grid operation.
See the [benchmark guide](../../benchmarks/README.md#exact-grid-baseline).

## Adaptive interval scheduling

Initialize `NumForgeRefinementOptions` with `numforge_refinement_options_init`,
then call `numforge_refine_intervals(&stats, initial_points, &options, probe, user)`.
The caller owns an ordered point collection with capacity for `max_points`.
On each successful callback, insert exactly one interior point at `left+1` and
choose `SPLIT`, `ACCEPT` or `SKIP`. The scheduler visits initial intervals left to
right, then processes children breadth-first while accounting for shifted indices.
It does not evaluate functions or assume a numeric representation. The
[C example](../../examples/sdk/numerical_refinement.c) supplies exact rational
midpoints and quadratic values, with a caller-defined chord-error criterion.

The scheduler bounds points and depth and inherits the caller's optional resource
budget. Configurable reserves and twice the largest observed callback cost guide
admission; they predict resource needs heuristically. Queue storage also consumes
the budget. No limit is reset and no callback is preempted. Point/depth/reserve
stops return `OK` with diagnostic flags and pending/depth-limited interval counts.
Intervals explicitly skipped by callbacks are excluded from those counts.
An exhausted runtime budget, allocation error, invalid decision or fatal callback
status fails the call and preserves `stats`, while earlier callback effects remain.

`ACCEPT` and a drained queue describe the callback's decisions, not mathematical
certificates. `SKIP` may intentionally leave gaps. Exact coordinates do not make
approximate ordinates exact or establish continuity between points. Callers retain
their own error, domain, classification and connection metadata.

The precision graph path shares this scheduler, retaining its 33 seeds, point/depth
limits, projection reserves, phase/jump/pole checks and atomic outer result.
Its typed sampling, acceptance rules and periodic-extremum probes remain private;
the screen/double sampler is separate. This adds no Python, CLI or REST operation.
See the [checked benchmark](../../benchmarks/README.md#adaptive-refinement-baseline).

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
scanning rules and response schema remain compatible. Bounded scans use the
public API below; Brent/hybrid research remains private. Bracket refinement uses bisection.
This slice adds no direct Python root function, CLI root command or new REST route.

## Multiple-root scanning

Initialize `NumForgeRootScanOptions` with `numforge_root_scan_options_init`, then
call `numforge_roots_scan(&scan, evaluate, domain, user, lower, upper, &options)`.
The [complete C example](../../examples/sdk/numerical_scan.c) finds the two roots
of `x*x-2` without a parser. Call `numforge_root_scan_destroy` once to release the
scan and its roots. Individual roots returned by `numforge_root_scan_root` are
borrowed read-only handles; do not destroy them separately.

Scanning defaults to 128 rational grid cells, capacity for 32 roots and at most
4096 evaluations. These limits are configurable; `root_options` defines the
per-bracket precision, tolerances and work allowance. Callback and domain contracts
match single-root search. The scan callback additionally receives an optional
rational ordinate destination: when supplied, write the original `f(x)` there,
preferably exactly. If evaluation is approximate, convert its decimal value to a
rational. This ordinate only proposes candidates, and never accepts them. It is
NULL during bracket refinement and its higher-precision verification.

Each cell can propose a midpoint and local quadratic vertex. Candidates are
evaluated through the original callback. Only exact-zero proofs or accepted
guarded brackets become results; a parabola minimum alone is not a root.
This retains rational contacts such as `1/3` and avoids losing small differences
near large common offsets. Roots are ordered and exactly equal coordinates are
deduplicated, with no tolerance merge. Approximate coordinates for the same
mathematical root are not automatically equivalent.

| Stop reason | Interpretation |
| --- | --- |
| `FINISHED` | The selected grid was processed; further roots can still exist. |
| `ROOT_LIMIT` | Capacity was reached; the returned list is partial. |
| `EVALUATION_LIMIT` | The total work allowance cannot continue the scan/refinement. |

Before refining a bracket, the solver reserves the full per-bracket evaluation
allowance. It can therefore stop with some total slots unused. All callbacks,
including refinement and verification, count toward the same total. Explicit
numerical limits return successful diagnostic results; allocation, callback and
inherited runtime-budget failures preserve the caller's old result and publish no
partial list. Callback side effects remain the caller's responsibility.

`numforge_root_scan_stats` copies diagnostics without allocating: grid edges with
domain gaps, rejected refinements, candidate probes/gaps and exact candidate roots
are separate counters. See the header for their precise counting rules.
The zero function produces only a bounded sample list; no result claims complete
enumeration. Unseen contacts, unresolved domains and several roots inside a cell
can still be missed. More grid cells do not establish completeness.

## Cost and verification

High-precision rational bisection produces substantial cumulative allocation
traffic; requested bytes differ from peak live memory. Apply explicit budgets to
expensive workloads. `numerical_tests` covers references, exact zeros, continuity,
precision instability, invalid options/callbacks, limits, allocation failures,
inherited budgets and 1000-place accuracy. Existing application tests preserve
domain-hole, discontinuity and scanning regressions. The
[checked baseline](../benchmarks/NUMERICAL_2026_10_19.md) records costs, not a speedup.

`numerical_scan_tests` adds ordered exact/approximate roots, domain evidence,
rounded zeros, rational contacts, positive minima, 1000-digit offsets/tiny scales,
capacities beyond 32 roots, stop reasons, callback failure after partial discovery,
allocation failures and inherited budgets. Installed C/C++ consumers verify the
same API with the engine disabled. The
[scan baseline](../benchmarks/NUMERICAL_SCAN_2026_10_19.md) records standalone
measurements and application-result parity.
