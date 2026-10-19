# Numerical API

`<numforge/numerical.h>` exposes exact rational grids, adaptive interval scheduling
and bounded real-root searches; `<numforge/curve.h>` supplies sampled curve
acceptance policies and `<numforge/phase.h>` locates periodic-phase anchors.
All belong to `NumForge::numforge` and need neither an
expression engine nor application state.
The public headers are authoritative for callbacks, precision, ownership and failures.

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
kernel. Typed expression evaluation, phase capture and
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
Its typed sampling and expression metadata extraction remain private;
the screen/double sampler is separate. This adds no Python, CLI or REST operation.
See the [checked benchmark](../../benchmarks/README.md#adaptive-refinement-baseline).

## Sampled curve acceptance

Create an owned `NumForgeCurveClassifier` with `numforge_curve_classifier_create`
from `<numforge/curve.h>`, then call `numforge_curve_assess` with left, midpoint and
right sample views. The classifier copies all policy decimals, camera settings
and channel kinds; those inputs can be changed or released afterwards. Samples
are borrowed for the assessment only. Destroy the thread-confined handle with
`numforge_curve_classifier_destroy`. Failures preserve the output/handle pointer;
runtime budgets are inherited without being reset. A failed assessment's scratch
state can be reused after ending the caller's failure scope.

The policy supplies chord tolerance, broad/tight phase spans, coarse width,
optional expanded camera bounds/tolerance/pixel width and explicit permission
for smooth display relaxation. Captured channels describe corresponding periodic
arguments (`PHASE`, in the same units as the spans) or step-function values
(`STEP`, requiring equality). Unknown, missing or mismatched channels block
acceptance. The caller must provide the actual parameter midpoint, even when its
decimal projection collapses onto another coordinate; real x projections must
be nondecreasing. The [C example](../../examples/sdk/curve.c) accepts a linear
triple and then rejects it when a captured phase becomes unsafe.

The result is `ACCEPT`, `SPLIT` or `SKIP`, with a flag for offscreen deferral.
All-non-real triples skip; mixed triples split. A monotone triple narrowly outside
one camera edge may defer refinement, without connecting it. STEP changes cannot
be accepted, including under smooth-width relaxation. Camera relaxation needs
both a small pixel width and a passing camera chord error with tight phases.
The public header specifies the precise sampled policy.

Arithmetic is exact on the supplied finite decimal values, including cancellation
at 1000-digit offsets. Those values may approximate the underlying function.
Acceptance is a display heuristic: it does not certify continuity, exclude hidden
features or bound error throughout an interval. Original typed graph values and
their domain/complex/quantity classifications remain intact in the adapter.
Precision graphs now use this policy; expression metadata extraction and conservative
domain analysis remain private. Phase-anchor location uses the public API below.
This adds no new
Python function, CLI command or HTTP route. See the
[benchmark and graph parity record](../../benchmarks/README.md#curve-acceptance-baseline).

## Periodic-phase anchors

`<numforge/phase.h>` supplies a reusable `NumForgePhaseLocator`. Initialize
`NumForgePhaseOptions` with `numforge_phase_options_init`, provide a positive
phase-lattice period and create the locator. Options, period and phase tolerance
are copied; the caller can then release or change their originals. The defaults
are 34 significant proposal digits, eight callback evaluations, phase tolerance
`1E-6` and no optional resource reserve. Phase units are caller-defined and must
agree across endpoint samples, period, offset, tolerance and callback results.

Call `numforge_phase_locate` with exact increasing rational coordinate bounds,
captured endpoint phases, an optional lattice offset and an evaluator callback.
The search considers the first target `offset+k*period` above the smaller phase,
even for a phase decreasing with x. It skips candidates outside the sampled phase
range or near its endpoints. This does not enumerate all crossings. Interpolated
finite decimal ratios become exact rational proposals; rounding that cannot make
strictly interior progress returns `PRECISION_LIMIT` without evaluating that point.

`FOUND` means an actual usable callback sample met the absolute phase tolerance.
Other successful diagnostic outcomes are `NO_CANDIDATE`, `UNUSABLE`,
`EVALUATION_LIMIT`, `RESOURCE_LIMIT` and `PRECISION_LIMIT`. They expose no accepted
coordinate. Optional reserves stop work before callbacks; an exhausted inherited
runtime budget instead fails with `OUT_OF_MEMORY` and a runtime failure reason.
No scope is begun, reset or ended by the locator.

The report contains the outcome and callback count. After `FOUND`, getters return
borrowed read-only x and target views, valid until the next call or destruction.
Every new search invalidates those views, including on failure. Fatal errors
preserve the report but cannot undo callback effects. Callers can retain their
last callback's original typed sample for insertion without evaluating it twice.
Destroy the thread-confined handle with `numforge_phase_locator_destroy`.
The [standalone C example](../../examples/sdk/phase.c) locates a half-period anchor
of a linear phase in one evaluation.

The coordinate is exact as a rational proposal, not necessarily an exact crossing.
Phase/period inputs can be approximate; continuity, monotonicity, callback accuracy
and x-error bounds are not proved. In particular, a sin/cos phase anchor need not
be an extremum of a larger composed expression. Precision graphs use this kernel
and retain only actual original-expression samples. Their phase extraction, point
capacity, angular-unit selection and graph insertion remain in the adapter.
See the [measurement and parity record](../../benchmarks/README.md#phase-anchor-baseline).
No new Python, CLI or REST phase operation is added.

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
public API below; production single-root solving also calls the public entry point.
Formatting and HTTP serialization read opaque results through public getters.
Brent/hybrid research retains a private callback hook without result storage.
Bracket refinement uses bisection.
The Python adapter below exposes this same kernel. No new CLI command or REST
route is added.

## Python single-root search

The source-built [Python adapter](../guides/PYTHON.md#bounded-numerical-roots)
provides `find_root(function, lower, upper, *, continuous=False,
decimal_places=34, x_tolerance="1E-12", residual_tolerance="1E-12",
max_iterations=96, max_evaluations=192, time_limit_ms=5000,
allocation_bytes=64*1024*1024, single_allocation=512*1024)`.
It calls `numforge_root_solve`; there is no separate Python search algorithm.

`function(x, decimal_places)` receives an exact Fraction coordinate and the
requested working fractional scale, including a doubled-scale final check.
It must return an accurate deterministic real sample as int, Fraction, finite
Decimal/numeric literal text or Result; None denotes an undefined/non-real point.
Binary floats, booleans and complex samples are rejected. Exceptions, including
KeyboardInterrupt, propagate unchanged after native cleanup. Callback side effects
are not rolled back. Text expressions are not accepted directly; a callback can
use a prepared expression or another public numeric operation.

Fraction/int arithmetic can supply exact samples. Decimal/text are interpreted
as exact finite input, including zero: do not use a rounded decimal zero as proof
of a mathematical zero. For unproven computed approximations return an approximate
Result instead. The adapter proves zero only from exact integer/rational storage;
it does not validate that a callback implements the intended mathematical function.
For approximate evaluation the callback must choose sufficient working precision
for its magnitudes and cancellation. Engine `precision` means significant digits;
it is not interchangeable with the callback's fractional scale. Reusing a stale
approximation at doubled scale does not establish improved accuracy.

`continuous=True` is the caller's assertion of real continuity on the interval,
not automatic domain analysis. The default supplies no evidence, so approximate
acceptance reports `domain_unverified`; proven sampled zeros can still succeed.
Never assert continuity for an unknown expression or across a pole to force a
successful result. The HTTP expression workflow has a separate private domain
adapter; this callable interface does not inherit that analysis.

Bounds and x_tolerance accept exact real number() inputs, with lower < upper
and x_tolerance > 0. residual_tolerance accepts a positive finite decimal literal,
Decimal or int, avoiding silent conversion of a recurring tolerance. decimal_places
is an integer 1..10000; max_iterations is positive and max_evaluations >= 4,
both fitting the platform size range. Increasing scale alone does not tighten
the absolute tolerances; raise scale, iteration/evaluation limits and budgets
together for demanding searches.

The frozen `RootResult` copies all fields and retains no native or callback handle:

| Field | Python value |
| --- | --- |
| `kind` | `found`, `not_bracketed`, `domain_gap`, `domain_unverified`, `iteration_limit` or `precision_limit`, with the meanings above |
| `x` | Sampled Fraction coordinate for `found`; otherwise None |
| `lower`, `upper` | Final Fraction search bounds, not certified enclosures |
| `residual` | Nonnegative stored Decimal residual for `found`; otherwise None |
| `exact_zero` | Whether the accepted sample had an exact-zero proof from its returned value |
| `iterations`, `evaluations` | Native work counts, including final verification |

An exact Fraction coordinate does not make the root exact. A non-`found` result
does not prove there are no roots. There is no universal error certificate,
multiple-root enumeration or automatic tangent discovery.

Input number() conversion precedes the search scope. Native input copies, callback
conversion, search and field serialization inherit one cooperative scope. Nested
NumForge calls do not reset/end it. Python Fraction/Decimal/object work is outside
C allocation accounting; callbacks retain the GIL and cannot be preempted by the
native deadline. Runtime failures publish no partial report and use the existing
Error time/memory/numeric statuses with offset zero. Callback exceptions keep their
original type, identity and diagnostics. Python validation uses TypeError/ValueError;
invalid native bounds/tolerances use Error. Results and inputs remain usable
independently after success or failure.

Python tests compare a 70-place result with an independent Decimal square root,
check doubled-scale verification, 1000-digit offsets/tiny spans, exact endpoints,
diagnostic outcomes, frozen lifetime, callback exceptions, nested calls and budget
failure/recovery. The [standalone example](../../examples/python/standalone.py)
runs from an installed wheel. The C [root benchmark](../benchmarks/NUMERICAL_2026_10_19.md)
remains the algorithm baseline; Python callback overhead is additional and no
cross-language speedup is claimed. Multiple-root scanning and other numerical
Python adapters remain future work.

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
