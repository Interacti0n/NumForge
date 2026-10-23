# Grids and sampling policies

Part of the [numerical API](NUMERICAL_API.md). These operations link
`NumForge::numforge` without an expression engine or application state.
Installed headers define callback, precision, ownership and failure contracts.

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
candidate traversal. Python uses the same walker below; no new CLI command or
REST grid route is added.
See the [grid baseline](../performance/baselines/SAMPLING.md#exact-grid-baseline).

## Python exact grids

`grid(lower, upper, count, *, time_limit_ms=10000, allocation_bytes=64*1024*1024,
single_allocation=512*1024)` returns an immutable tuple of exact Fraction
coordinates. `visit_grid(lower, upper, count, visit, *, same limits)` visits those
coordinates synchronously and returns None after all visits succeed. Both call
`numforge_grid_visit`; Python does not duplicate the spacing algorithm. See the
[working example](../guides/PYTHON.md#exact-grids).

Bounds accept the lossless inputs of number() and must be exact real values;
approximate/complex Results, binary floats and booleans are rejected. lower must
be strictly less than upper. count is an integer >= 2, excluding bool. For
visit_grid it may fit the full platform size_t range; grid additionally requires
count <= sys.maxsize because it constructs Python containers. There is no
precision/rounding parameter and no decimal projection of coordinates. Recurring
fractions, 1000-digit offsets and tiny spans retain exact spacing; this does not
establish accuracy or continuity of any sampled function.

`visit(x, index)` receives a new immutable Fraction and zero-based Python int.
Coordinates may be retained independently of the C walker and original bounds.
Return values, including False, are ignored. Exceptions stop traversal and retain
their original identity/type, including KeyboardInterrupt. Previously completed
callback effects remain visible; they cannot be rolled back. Nested NumForge
operations share the outer cooperative budget without resetting/ending it.
Callbacks run under the GIL and are not preempted by the native deadline.

Input number() conversion precedes the traversal scope. Native input copies,
coordinate arithmetic and per-visit field serialization share its limits. Python
Fraction/object allocation and arbitrary callback work are outside native memory
accounting. The C walker retains constant auxiliary object count, with byte size
depending on the bounds/count. grid collects O(count) Python objects and returns
no partial tuple on failure; use visit_grid to avoid retaining that point list.
Neither interface promises hard process time/memory isolation.

Invalid count/callback types raise TypeError; invalid Python count ranges raise
ValueError. Nonincreasing bounds and native numeric/resource failures use Error
with existing statuses and offset zero, including time_limit/memory_limit. No
callback runs on invalid bounds or failure before traversal. All inputs remain
usable and subsequent calls recover after errors. Python integer/text conversion
limits still apply to copying large coordinates and are not changed globally.

Python tests cover order, exact recurring coordinates, independent spacing/sum
identities, 1000-digit offsets with 1000-digit tiny spans, retained values, nested
calls, signed/unsigned count boundaries, exception identity, partial visits and
budget failure/recovery. The installed [standalone example](../../examples/python/standalone.py)
uses grid. The existing [C grid baseline](../performance/baselines/SAMPLING.md#exact-grid-baseline)
is reused; Python conversion/callback costs are additional and no speedup is claimed.

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
See the [checked benchmark](../performance/baselines/SAMPLING.md#adaptive-refinement-baseline).

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
[benchmark and graph parity record](../performance/baselines/SAMPLING.md#curve-acceptance-baseline).

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
See the [measurement and parity record](../performance/baselines/SAMPLING.md#phase-anchor-baseline).
No new Python, CLI or REST phase operation is added.
