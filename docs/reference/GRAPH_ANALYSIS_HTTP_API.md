# Discrete graph points and C candidate verification

These are internal C application/HTTP adapters, not additions to the installed
numeric SDK. Expressions use the existing parser, real-domain classification,
read-only snapshots and resource budgets. Assignments and random calls are rejected.

## Integer points

`POST /api/graph/plot?mode=fast&render=auto&min=-10&max=10`
with body `x!` returns disconnected integer samples. The new `render` option is
allowed only with `mode=fast`; values are `curve` (API default), `auto`, `points`.
Omitting it preserves the existing screen/precision fallback contracts.

Auto conservatively detects a factorial postfix or factorial/gcd/lcm/mod/npr/ncr/
isqrt call whose argument is the inferred parameter itself, including nested
calls such as `sqrt(x!)`. It does not infer transformed lattices, e.g. `(2*x)!`.
Explicit Points samples integer x, not arbitrary real-domain isolated values.
Ordinary curves keep their existing sampler; constant factorial calls stay curves.

The returned schema is 3 with `sampling:"discrete"`, `evaluation:"precision"`,
`render:"points"` and positive integer `point_stride`. Point rows retain the
compact `[x,y,connect,kind,source_approximate]` format. `connect` is always 0;
`count` may be 0 or 1. Non-real/domain and unrepresentable values retain their
classification/null ordinate. `y_min`/`y_max` may be null for an empty result.
Quality is `refined` even if the request asked for coarse preview: points are
already actual precision evaluations, not an interpolated preview.

Coordinates are integers between ceil(min) and floor(max), with globally aligned
stride `ceil(integer_count/(limit-1))` (at least 1). Limit is
min(256, floor(width/8)); width retains its existing 320–2048 bounds. Stride > 1
sets `limited:true` and means omitted integer positions, not failed continuity.
Bounds outside ±(2^53−1) are rejected because integer coordinates would no longer
be reliably distinct in the browser. The same 5-second, cumulative allocation and
single-allocation budgets apply. An expensive factorial can still exhaust them;
failure is atomic. Mode participates in browser cache/session identity.

## Candidate verification

`POST /api/graph/verify` accepts a strict newline-delimited UTF-8 body, at most
4096 bytes including expressions and rows:

```text
sin(x)

0 3.1415 3.1 3.2 0.00001 0.000001
```

Line 1 is the first expression. Line 2 is the second expression, empty for single
function checks. Remaining 1–32 rows contain exactly six space-separated tokens:

`kind x low high x_tolerance y_tolerance`

All numbers must be finite decimal literals, each shorter than 48 ASCII bytes;
tolerances must be positive, and low ≤ x ≤ high. An optional final newline is
accepted. Expressions containing newlines cannot use this wire format; browser
expression inputs are single-line fields. Combined overlong expressions cannot
be submitted for verification and leave their candidate markers undisplayed.

| Kind | Check |
| --- | --- |
| 0 | First expression's root near x, within [low,high]. |
| 1 | Actual real value of the first expression at x (axis intercept uses x=0). |
| 2 | First/second expression intersection within [low,high]. |
| 3 | First/second value agreement at exactly x, without root refinement (discrete points). |

The only accepted query field is optional `precision=34..100` (default 34).
Each expression independently infers its parameter; `x` and `t` can therefore be
paired. There is no calculator session, assignment or angle override; radians
and an isolated read-only evaluation context are used.

The response is `ok:true`, `schema_version:1`, `evaluation:"precision"`, boolean
`limited`, `count`, and `points` in request order. Each point is either null
(unconfirmed) or `{x,y,residual,approximate}` with finite JSON numbers. Root y is
0; other ordinates come from the first expression. Rational differences are
subtracted before decimal projection, avoiding subtraction of rounded doubles.

Additive diagnostic counters describe this request: `evaluations` counts actual
expression evaluations (a pair miss normally costs two), `sample_requests` counts
sample lookups after the time/memory reserve check, and `cache_hits` counts reused
checks. A final lookup stopped by the evaluation cap can be included in
`sample_requests`; it is not an actual evaluation. Counters are not timing or
accuracy guarantees and do not change the response schema version.

Further additive counters are `refinement_steps`, `bisections`, `interpolations`,
`confirmed`, `non_real`, `residual_rejections`, `iteration_limits`,
`coordinate_limits`, and `unprocessed`. Refinement counts include attempted steps
stopped by the shared budget. The six outcome categories partition candidate
rows: confirmed, finite-real projection unavailable, residual/contact
check rejected, 24-step cap reached, coordinate resolution exhausted, or work
unfinished under the shared budget. More precisely, the partition is
`confirmed + non_real + residual_rejections + iteration_limits + coordinate_limits + unprocessed = count`;
`refinement_steps = bisections + interpolations`.
`non_real` includes missing/nonfinite projections, not only mathematical domains.

Actual C evaluations check the candidate and its neighbors. Sign-bracketed roots
use at most 24 safeguarded Brent-Dekker refinements (secant/inverse-quadratic
interpolation with bisection fallback), accepting a residual at
most min(y_tolerance, 1e-11 × max(1, absolute y)). Same-sign tangent checks require
a residual at most min(y_tolerance, 1e-24 × max(1, absolute y)) much smaller than
both neighbors. Decimal rounding can recover simple exact cusp coordinates.
An interval becoming small does not confirm a root: an actual original-expression
sample must meet the same residual check. Machine-coordinate exhaustion leaves
the candidate unconfirmed. The method does not establish continuity across a
bracket, and the existing pole/domain checks remain necessary.
Discrete pair checks never search away from their sampled integer x. Undefined,
complex and quantity samples do not become real zeros. Poles and step jumps are
rejected unless an actual checked value meets the numerical residual criterion.

This is numerical confirmation, not symbolic proof of an exact/unique root.
Very small positive minima below tolerance and ill-conditioned expressions remain
limitations. Wire coordinates are approximate finite screen projections.

The whole request shares a 1-second budget and the normal memory limits, with
at most 512 actual expression evaluations. A 50-ms/2-MiB soft reserve or probe
limit can return partial confirmations with `limited:true`; unprocessed rows are
null. Resource/structural failures return the normal error envelope without a
partial success body. C application output is atomic on such failures.

A request-local, 128-entry, four-way cache reuses complete original-expression
checks. Its key is the 16-significant-digit decimal coordinate actually evaluated
in C, plus whether the check uses one expression or their difference. It stores
the projected ordinate/difference together with the original difference sign and
classification; it never subtracts rounded cached ordinates. Non-real successful
checks may also be reused. Failures are not cached. The returned coordinate keeps
the current caller's approximate x, even when two binary coordinates map to the
same decimal input. There is no reuse across requests, expressions or sessions.

Cache hits do not spend the 512-evaluation allowance, but still check cancellation
and time/memory reserves. Candidate count, refinement limits and numerical
tolerances remain unchanged. The bounded stack storage retains no allocated
typed values. A private diagnostic C entry point permits controlled cache-off/on
benchmarks; production callers enable reuse. Optional C counters are atomic on
failure along with the other outputs.

## Browser scheduling and marker balance

The C verifier runs only after candidate geometry exists and the camera has
settled for 120 ms. Requests share the transport queue behind foreground plots;
new geometry/navigation aborts obsolete work and stale responses cannot repaint.
Only confirmed candidates become special markers. Original sampled discrete
axis values are already C-evaluated and need no verification round trip.

Discovery is bounded to 65,536 edge/overlap visits. Per axis/function-pair groups
keep one candidate per cell of a 64×16 screen grid (at most 14×1024 candidates for
four functions). Spatially spread ordering and round-robin quotas select at most
128 total markers across all groups. Dense envelopes and coincident stretches
are excluded. Finite sampling is not a completeness guarantee.

Persistent marker presentation is thinned when more than 12 confirmations share
a narrow canvas (CSS width at most 600 px) or a mobile layout (viewport at most
900 px). Selection gives each curve a turn, preserves its candidate order and keeps
24 CSS pixels between displayed dots, using the actual SVG screen transform.
All confirmed coordinates remain available to inspection; thinning does not
discard confirmations or change C work. Sparse plots and ordinary desktop
canvases keep their full marker set. Canvas expansion/resize recomputes spacing.

One geometry snapshot avoids repeated work for color/status-only redraws. A
DOM-free verification cache, at most 256 entries and 512 KiB conservative size,
survives same-tab tool/language navigation; expression/camera changes use different
keys. Pointer movement uses existing confirmations and makes no C requests.
