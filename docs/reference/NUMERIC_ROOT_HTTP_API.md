# Numerical interval root

`POST /api/equations/solve?mode=numeric&min=0&max=1&precision=34` accepts
one equation line, e.g. `cos(x)=x`, and searches for **one real root**. This
stateless private C operation is not an installed numeric SDK API and does not
enumerate roots or provide certified root-error bounds.

## Request and work limits

- Use lowercase unknown x, exactly one `=`, radians and calculator functions.
  Approximate real functions/constants are supported. Saved variables, ans,
  assignments and random calls are unavailable.
- Required min/max are finite decimal literals, including scientific notation,
  each at most 47 ASCII characters, with min < max. No bound expressions.
- Precision is 16–100 significant digits, default 34; evaluation adds 16 guard
  digits. A final non-exact candidate is reevaluated at twice that working
  precision. Only mode/min/max/precision are accepted; duplicate/unknown options fail.
- The body is at most 4096 UTF-8 bytes, without CR/LF or NUL. Existing framing
  limits apply: 8192 bytes including headers and a two-second receive deadline.
- Preparation, solving and serialization share five cooperative seconds, 64 MiB
  cumulative allocations and 512 KiB single allocations. Response limit: 128 KiB.
- At most 96 iterations and 192 evaluated coordinates. Each coordinate evaluates
  both original sides, not just one primitive math operation. This includes the
  repeated final coordinate used by the higher-precision verification.

The browser's Numerical interval root mode defaults to 0..10. Polynomial and
coefficient modes retain their existing exact semantics and precision ranges.

## C method and acceptance

`application_numeric_root_solve` in `src/application/equations/numeric_root.h` returns an
owned `ApplicationNumericRoot`, released by `application_numeric_root_destroy`.
Bounds/context are borrowed, failures preserve the destination, and nested calls
inherit an outer budget without resetting it. Both expressions are prepared once
through the shared C evaluator. Coordinates and bisection use BigRational;
values/residuals use BigDecimal. Exact integer/rational sides are subtracted
rationally before projection. No double conversion is used.

Exactly zero typed rational samples at endpoints or midpoints can be accepted
immediately. Otherwise opposite nonzero endpoint signs are required. Approximate
endpoint zeros alone do not establish a bracket. Sampled undefined, Quantity or
non-real values stop the search with a domain diagnostic.

For other candidates, absolute interval width and absolute residual must both
be <=1E-12, and residual must improve strictly on both original endpoints.
This rejects tested tiny constant jumps and scaled poles that would pass an
absolute residual check alone. Approximately zero midpoints also need resolvable
opposite-sign quarter samples. More requested precision does not tighten these
fixed tolerances or raise work limits.

Before publishing a non-exact candidate, both original expression trees must
also pass a conservative real-continuity/domain check on the **final search
interval**. Unsupported or unresolved domains return domain_unverified with
no root. The original input is not cancelled/simplified to discard domain holes.

The check uses exact BigRational enclosures for literals, the parameter,
arithmetic, abs and bounded integral powers. Division requires a denominator
range excluding zero; sqrt requires a nonnegative argument range, ln and unary
log require a strictly positive range. Sin/cos/exp and a restricted set of
globally continuous real functions accept continuous arguments. Mathematical
constants use coarse exact rational enclosures. No rounded transcendental
endpoint is treated as a guaranteed enclosure. Small powers propagate ranges;
large integral powers may retain continuity with an unknown output range.

Range overestimation/dependencies can reject valid expressions. Steps, tan,
multi-argument logarithms, unsupported calls and insufficiently bounded nested
ranges remain unverified. For example, ln(exp(x)) currently has an unknown inner
range and is conservatively unverified. This is not a claim of discontinuity.
The check inherits the operation budget and visits at most 2048 AST nodes.
Exactly zero typed rational samples retain their separate acceptance path:
the original equation is valid at that point without requiring continuity
throughout its surrounding interval.

A non-exact candidate is also reevaluated through both original expression trees
at `2*(precision+16)` digits. Its checked residual must remain <=1E-12 and
improve on both initial endpoint residuals; a changed nonzero sign or failure of
these checks returns precision_limit without a root. The emitted residual comes
from this higher-precision evaluation. This catches apparent cancellation zeros
such as exp(x)=1E100 at insufficient precision. The extra trees/sample inherit
the same request budget and are freed before publication. This is a stability
check, not certified error propagation through arbitrary ill-conditioned ASTs.

These checks do not give certified existence or root-error bounds: signs and
residuals still come from finite-precision evaluation. Even-multiplicity
roots without a sign change can be missed. Extremely scaled/ill-conditioned
functions and very wide intervals may fail to converge. Exact-zero acceptance
establishes only the typed sampled equation value, not uniqueness/completeness.

The [numerical audit](../benchmarks/NUMERIC_ROOT_AUDIT_2026_10_18.md) records the
original false acceptance of a vanishing jump. The subsequent domain check now
rejects that case as domain_unverified. Width, residual and endpoint improvement
alone remain insufficient. See the [domain follow-up](../benchmarks/NUMERIC_ROOT_DOMAINS_2026_10_18.md).

## Response and errors

HTTP 200 numerical results have ok:true, schema_version:1, kind:"numeric" and
**complete:false**, including nonconvergence:

| Field | Meaning |
| --- | --- |
| outcome | found, not_bracketed, domain_gap, domain_unverified, iteration_limit or precision_limit. |
| count, roots | One root when found; otherwise zero and an empty array. |
| lower, upper | Final rational search bounds as strings, not certified enclosures. |
| iterations, evaluations | Refinement steps and evaluated coordinate pairs, including final reevaluation. |
| x_tolerance, residual_tolerance | Both "1E-12"; exact-zero samples are the exception. |
| roots[0].x | Full decimal coordinate checked by C, as a string. |
| roots[0].approximation | Rounded display coordinate, as a string. |
| roots[0].residual | Absolute original-equation difference at full x, as a string. |
| roots[0].exact_zero | Whether the typed rational difference at the sample is zero. |

Copy point copies full x. The preview may round to a different coordinate;
residuals apply to full x. There is no exact symbolic-root/multiplicity field or
lossy JSON numeric coordinate. A non-found outcome never means “no roots”.

Malformed options return HTTP 400/numeric_options_error. Syntax, unresolved-name,
preparation and resource failures use numeric_equation_error and standard status
and UTF-8 character columns. Malformed query parsing may use request_error.
Numerical nonconvergence is separate from structural/resource failure; no partial
root is published. Existing low-memory server fallback errors remain available.
domain_gap means an actually sampled undefined/non-real value; domain_unverified
means the final candidate interval could not be verified and exposes no x or
residual. Clients should handle the additional diagnostic outcome.

## Multiple-root scan

`POST /api/equations/solve?mode=numeric_scan&min=-10&max=10&precision=34`
accepts the same equation, bounds and precision options. For example, `sin(x)=0`
finds seven roots in this interval. This is **bounded discovery**, not complete
enumeration or a certified existence test. No new public numeric SDK is installed.

The private `application_numeric_roots_scan` returns an owned `ApplicationRootScan`
(destroy with `application_numeric_roots_destroy`). It prepares both original
sides for a grid of 128 equal **BigRational** cells and samples up to 129 vertices.
Exact typed zeros are retained immediately. Opposite nonzero endpoint signs
trigger the existing production bisection solver with its unchanged acceptance
checks and 96-step/192-coordinate ceiling. A grid value rounded to zero needs
opposite-sign real neighbours on both sides; it is never accepted by itself.
Refinement prepares its own original ASTs; no simplified difference loses holes.

Same-sign cells also sample a midpoint and, when its local interpolating
parabola has a distinct vertex inside the cell, that vertex. Proposals use
BigRational ordinates: exact sampled differences are retained without rounding;
decimal differences are used only as proposal data. Midpoint/vertex samples
reevaluate **both original sides**. A typed exact zero may be retained;
opposite signs split into two ordinary guarded brackets. A small or rounded-zero
minimum alone is never accepted. This finds rational quadratic contacts such as
`(x-1/3)^2=0` and some pairs inside a single grid cell. It is bounded local
discovery, not symbolic differentiation or a proof of complete subdivision.

The scan retains up to 32 ordered roots, removing exact-coordinate duplicates
without merging distinct points by tolerance. It caps all grid/refinement work
at 4096 coordinate pairs, conservatively reserving 192 before each refinement.
The entire scan, all nested solvers and serialization share **one** cooperative
five-second/64-MiB/512-KiB operation budget. A resource/structural failure preserves
the C destination and publishes no partial successful HTTP result. A work/result
ceiling is a successful, explicitly limited discovery result instead.

Responses use schema_version:1, kind:"numeric_scan", complete:false, count and
roots with x/approximation/residual/exact_zero fields. In **scan mode**, x is a
canonical exact rational string (integer or numerator/denominator), with
coordinate_kind:"rational". Its residual is evaluated at that rational point.
For example `(x-1/3)^2=0` emits x:"1/3", residual:"0"; approximation is a rounded
decimal preview. Local quadratic candidates and their bisections can produce
non-terminating fractions, so converting x to a finite decimal would lose the
checked coordinate. Single-root mode retains its existing full decimal x
contract. `lower` and `upper` are the original interval. Additional fields:

| Field | Meaning |
| --- | --- |
| outcome | scanned (grid visited), root_limit (32 stored roots), evaluation_limit (next work cannot fit). None means all mathematical roots were found. |
| segments, scanned_segments | 128 and the number of processed grid cells, respectively. |
| max_roots, max_evaluations | 32 and 4096. |
| evaluations, iterations | Total coordinate-pair evaluations (including final checks) and nested refinement iterations. |
| domain_gaps | Processed grid cells with an undefined/non-real sampled endpoint. |
| rejected_brackets | Opposite-sign candidates whose refinement did not confirm a root, including domain/precision/nonconvergence diagnostics. |
| candidate_samples | Additional midpoint/vertex evaluations, included in evaluations and its 4096 ceiling. |
| exact_candidate_roots | Roots retained from typed exact zeros at additional samples; bracket-derived discoveries are counted in roots instead. |
| candidate_gaps | Additional midpoint/vertex samples outside the real domain, separate from grid endpoint domain_gaps. |

The scan continues across sampled gaps and rejected candidates; valid roots in
other cells remain eligible. Domain diagnostics do not prove every unsampled
point is valid. Several roots inside one cell can be missed, as can non-grid
tangencies, approximate endpoint zeros, unresolved domains and very dense
oscillations. `(x-0.01)*(x-0.02)=0` on 0..128 and
`(x-0.1234)^2=0` on 0..1 now find their roots through local candidates. General
non-polynomial tangencies such as `(cos(x)-x)^2=0` may remain unconfirmed; a
tiny positive minimum such as `(x-0.1234)^2+1E-100=0` is not a root. Conversely,
`x=x` reaches the result limit rather than enumerating an infinite set. An empty
array never proves “no solutions”. Narrow/shift the interval when appropriate.

The browser offers **Multiple interval roots / Viac koreňov na intervale**,
retains the same copy/graph actions and session behavior, and distinguishes
finished discovery from work/result limits. See the
[scan benchmarks](../benchmarks/NUMERIC_ROOT_SCAN_2026_10_19.md) and
[candidate follow-up](../benchmarks/NUMERIC_ROOT_CANDIDATES_2026_10_19.md).
General tangent discovery and configurable tolerances remain future work.
See the [equation guide](../guides/EQUATIONS.md) for graph navigation.
