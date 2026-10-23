# Single-root and multiple-root search

Part of the [numerical API](NUMERICAL_API.md). These operations link
`NumForge::numforge` without an expression engine or application state.
Installed headers define callback, precision, ownership and failure contracts.

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
max_iterations=96, max_evaluations=192, time_limit_ms=10000,
allocation_bytes=128*1024*1024, single_allocation=8*1024*1024)`.
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
runs from an installed wheel. The C [root benchmark](../performance/baselines/ROOTS.md#single-root)
remains the algorithm baseline; Python callback overhead is additional and no
cross-language speedup is claimed. Multiple-root scanning is available below;
exact Python grids are described in the [sampling reference](SAMPLING_API.md#python-exact-grids). Adaptive/curve/phase wrappers
are not currently exposed in Python.

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

## Python multiple-root scanning

`find_roots(function, lower, upper, *, continuous=False, decimal_places=34,
x_tolerance="1E-12", residual_tolerance="1E-12", max_iterations=96,
max_root_evaluations=192, segments=128, max_roots=32, max_evaluations=4096,
time_limit_ms=10000, allocation_bytes=128*1024*1024, single_allocation=8*1024*1024)`
delegates to `numforge_roots_scan`, sharing the
[single-root callback, exactness, continuity, input and budget rules](#python-single-root-search).
There is no additional Python discovery/refinement algorithm or private AST access.
The [Python guide](../guides/PYTHON.md#bounded-numerical-roots) has a working example.

`segments` is positive and less than SIZE_MAX; `max_roots` and `max_evaluations`
are positive platform-size integers. `max_iterations` and `max_root_evaluations`
(at least four) apply to each bracket. `max_evaluations` counts all grid, candidate,
refinement and verification calls. The C kernel reserves a full per-bracket
allowance before refinement and can therefore stop with total slots unused.

The callback's original exact integer/rational ordinate supplies the C candidate
proposal without passing through a rounded decimal. An approximate Result supplies
its stored decimal as an advisory rational. These values propose midpoint/local
parabola-vertex samples; they never prove a root without reevaluating the original
callback. Exact sampled contacts can succeed without a continuity assertion;
approximate bracket roots retain the single-root safeguards. This is not a general
tangency solver. A coarse grid can miss several roots in one cell even when the
callback is an exact polynomial.

The frozen `RootScanResult` owns a sorted tuple `roots` of independent `RootResult`
values. `count` is len(roots); `complete` is always False, including for zero roots.
Only exactly equal coordinates are deduplicated, not close coordinates within a
tolerance. Root values remain usable after discarding the scan, callback and inputs.
`kind` is `finished`, `root_limit` or `evaluation_limit`, following the C stop reasons
above. Explicit limits preserve a partial list; allocation, callback and cooperative
budget errors return no report, including after earlier roots were discovered.
Existing reports and input values remain usable; callback side effects remain.

Copied integer diagnostics are `evaluations`, `iterations`, `scanned_segments`,
`domain_gaps`, `rejected_brackets`, `candidate_samples`, `candidate_roots` and
`candidate_gaps`, with the C statistics meanings above. Every retained root has
its own bounds/residual/exact-zero flag and work counters. Aggregate counters can
include discarded candidates and rejected refinements, not just returned roots.

Python coverage includes exact shared endpoints/order, off-grid rational contacts,
close sampled roots without tolerance merging, rejected positive minima, an
independent 35-place Decimal reference for both signs of sqrt(2), prepared-engine
samples, 1000-digit offsets/tiny contacts, all stop reasons, missed contacts,
domain gaps, exceptions after partial discovery and budget failure/recovery.
The installed [standalone example](../../examples/python/standalone.py) exercises
both search modes. The existing C [scan baseline](../performance/baselines/ROOTS.md#multiple-roots)
is reused; Python callback overhead is additional and no speedup is claimed.

## Cost and verification

High-precision rational bisection produces substantial cumulative allocation
traffic; requested bytes differ from peak live memory. Apply explicit budgets to
expensive workloads. `numerical_tests` covers references, exact zeros, continuity,
precision instability, invalid options/callbacks, limits, allocation failures,
inherited budgets and 1000-place accuracy. Existing application tests preserve
domain-hole, discontinuity and scanning regressions. The
[checked baseline](../performance/baselines/ROOTS.md#single-root) records costs, not a speedup.

`numerical_scan_tests` adds ordered exact/approximate roots, domain evidence,
rounded zeros, rational contacts, positive minima, 1000-digit offsets/tiny scales,
capacities beyond 32 roots, stop reasons, callback failure after partial discovery,
allocation failures and inherited budgets. Installed C/C++ consumers verify the
same API with the engine disabled. The
[scan baseline](../performance/baselines/ROOTS.md#multiple-roots) records standalone
measurements and application-result parity.
