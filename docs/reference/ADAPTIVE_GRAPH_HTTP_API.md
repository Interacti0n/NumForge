# Adaptive graph HTTP API

`POST /api/graph/plot?min=-10&max=10` accepts one text expression. It is an internal
C application adapter, not an installed public SDK addition. The original
[uniform sampler](GRAPH_HTTP_API.md) and its schema 1 remain unchanged.
The web opts into [screen-space mode](SCREEN_GRAPH_HTTP_API.md), which returns
compact schema 3 or falls back to this precision schema 2. Requests without
`mode=fast` retain the sampler described here.

## Query and parameter selection

Required `min`/`max` use the uniform sampler's finite decimal literal grammar
and must be increasing. Optional `precision` (1–10000, default 34), `angle`
(rad/deg), retained `client` and pinned `at` have the same contracts. `points`
is rejected: the C sampler determines density. Many approximate functions retain
at least 34 working digits even when a lower precision is requested. Exact
arithmetic remains exact; adaptive plotting strings use a 16-place scientific projection
(17 significant digits) to preserve close values during zoom. Optional `parameter` explicitly
selects a local name; otherwise it is inferred from the one parsed AST.

Inference selects the unique variable absent from the supplied session. Session
variables are coefficients, except that when no free names exist the local name
is x, shadowing a referenced session x. With no variable the expression is constant
on x. Multiple free names return `undefined variable` with the diagnostic code
`graph_parameter_ambiguous`. Constants, function names/aliases and string arguments
are never candidates. No session is created, revived or changed.

## Response schema 2

Success retains `ok`, `parameter`, `count`, and classified
`points` from the uniform contract. It adds:

- `schema_version:2`, `sampling:"adaptive"`.
- `plot_places:16` for adaptive x/y and view bounds. The uniform schema 1 still
  uses `plot_places:12`; clients must honor the field rather than assuming a value.
- Per-point `connect`: whether a sampled chord from the previous point met the
  heuristic error criterion. The first is false. Classified gaps never connect.
- `limited`: refinement encountered a depth, point or remaining resource bound; unresolved
  intervals are disconnected instead of being silently joined.
- `y_min`/`y_max`: rounded scientific strings covering all discovered real samples
  for resolved or recognized smooth trees. Unresolved pole/domain trees retain
  the real seed overview (or discovered real samples if the seeds had none);
  null when there are no real samples. These
  suggest a display view, not a domain/continuity/range certificate.

The browser adds padding to the suggested view, clips curves, rejects unsupported
binary numeric projections to draw the curve. Newly discovered poles
cannot inflate the seeded overview for unresolved pole/domain trees. Smooth
curves include newly discovered extrema instead of clipping them to the seeds.

## Bounded C refinement

Preparation parses once and snapshots only referenced values. Thirty-three exact
inclusive seed coordinates span the requested view. Midpoints are exact rational
coordinates. Every new sample uses the same typed C evaluator and inherited scope.

All-real triples compare absolute midpoint/chord error with 0.001 of the seeded
y span (constant span uses absolute seed magnitude, or 1 if zero). A rejected
interval is bisected; mixed real/non-real triples are refined to locate boundaries.
All-non-real triples stop with gaps. Intervals are processed breadth-first across
the whole view, limited to depth 12 and 2048 total samples. For real arguments of
forward sin/cos/tan calls, corresponding phase probes must also differ by at most
0.5 radians (30 degrees). Nested calls are checked too; unknown phases are never
accepted solely because their output values happen to agree. This avoids obvious
whole-period aliasing on wide intervals without projecting C mathematics to double.
Phase arguments are evaluated through the same prepared C AST; at most 64 forward
call sites are captured, with overflow/unsupported arguments treated as unknown.

For a restricted continuous expression tree, the ordinary display error tolerance
is relaxed to 0.005 of the seeded y span. A parent interval no wider than
1/256 of the requested x span can instead accept a coarser pair of chords. The
whitelist contains constants/variables, unary signs, sums/products, nonnegative
literal integer powers, square/cube postfixes, division by a literal constant,
sin/cos, exp and abs, recursively composed. All three samples must still be real;
captured phases must stay within 3 rad / 170 deg. The ordinary tighter phase
check continues to apply to early error-based acceptance. Variable denominators,
tan, domain-restricted functions and jump operations do not use this fallback.
This trades line-interpolation precision for connected, useful display curves;
individual C sample precision is unchanged. It is not a general continuity solver.

Accepted edges in recognized smooth trees receive optional sin/cos extremum
anchors. Captured phases locate the next half-period extremum (sin: π/2+kπ;
cos: kπ; degree equivalents 90+180k/180k). A secant coordinate estimate is checked
by actual C evaluation and, if needed, refined within the phase bracket up to
eight times. The measured phase must agree within 1e-6 angle units; existing
near-extremum endpoints are not duplicated. No synthetic ±1 results are inserted.
This prevents false amplitude modulation from coarse uniform grids, including
sin(x) on −100..611. For composite/nonlinear phases these are component anchors,
not a proof that every extremum of the complete expression was found.
They share the 2048-sample/request budget. Insufficient remaining points,
3 MiB allocation reserve or 300 ms time reserve mark the graph limited while
preserving already accepted chords; actual allocation/time failures remain atomic.

Before each new probe the sampler checks remaining cumulative bytes and time,
reserving 2 MiB and 200 ms for finishing/serialization plus twice the largest
observed probe cost. It can stop with `limited:true` before exhausting the budget.
This is an estimate, not a guarantee for an unexpectedly expensive next sample.
One five-second/64 MiB cumulative/512 KiB single-allocation budget covers the
whole operation and serialization; adaptive graph JSON must fit 256 KiB.
Uniform graph and other session responses retain their 128 KiB limits. Resource failure
still returns no partial success. No per-point budgets or binary floating-point math
are used in the C sampler.

Finite midpoint sampling cannot prove continuity or find every narrow feature,
pole, isolated domain island or oscillation. It may miss an oscillation that
aliases its probes; it may omit a steep continuous interval. `connect` describes
the sampled heuristic only. It must never be advertised as a continuity proof.
An interval containing too many oscillations may have no accepted connections:
choose narrower x bounds rather than interpreting an empty curve as a zero function.

See the [graph performance audit](../benchmarks/GRAPH_AUDIT_2026_10_12.md)
for allocation/time measurements and independent point/chord comparisons.

## Browser range cache

The browser reuses this same endpoint for bounded range tiles; no new numeric
HTTP schema is introduced by caching. The current web page uses the screen sampler
with automatic precision fallback, radians and no calculator client. Its cache
identity is the expression; curve color is only presentation. API clients can
still select calculation precision, angle and read-only session snapshots. A missing or too-coarse
range is sampled through the existing endpoint (up to four foreground pieces,
otherwise a complete view); cached data remains as a clipped preview meanwhile.

After 180 ms idle, quick resolved responses may trigger at most two sequential
prefetches: an optional three-times-wide buffer for inexpensive views, then next-zoom
detail. Views using more than one third of the fast 8192-point cap skip wide
preparation. Foreground sampling of all visible functions finishes first.
In the normal canvas the current x range is prepared at up to twice the width
and half the y span, also supporting expansion. In the expanded canvas only
the central next-zoom interval is prepared at the current width and half the y
span. Coverage can combine prepared tiles; a single missing tail is sampled
alone. A completed batch is not repeated after a resize when visible detail is
already sufficient.
Speculative data cannot evict the active prepared view. Missing foreground detail
waits 90 ms after a settled camera change; active drag/pinch waits for release. Responses taking
more than 1500 ms including transport, limited responses and errors stop that
batch. There is no repeated timer/polling loop. Moves, edits and disposal abort
browser requests and reject obsolete responses; aborting a fetch does not promise
to interrupt a C evaluation already running on the server. Each server request
still independently shares the original five-second/allocation budget.

The current page supports at most four curves and retains per function at most
12 ranges, 12288 cached point-or-band records, an 8 MiB estimated cache weight, and 48 bounded
prefetch-attempt keys. Four curves permit 49152 records and 32 MiB estimated weight.
A shared transport queue prioritizes foreground requests over background tiles.
Each function/range request has its own server budget; these budgets do not merge
into an overall five-second multi-curve timeout. Weight is twice the JSON
character count plus 256 bytes per point and 128 bytes per dense band, not a
browser heap measurement.
Oldest ranges are evicted when any bound is exceeded; visible chunks can still
retain references until the next view is drawn. No sample table is rendered;
sample data is not persisted in sessionStorage. The finest available cached range is
chosen per visible segment, prioritizing adequate refined resolution before
range width and retaining independent C connection/gap flags. Coarse previews
do not discard useful refined levels of the same interval. Already accepted
cached strokes covering a view without visible unresolved gaps supply its
preview, so that view requests refinement directly rather than another coarse
pass. Missing/gapped views retain coarse-then-refined sampling.
Coordinates at endpoints may be aligned to the requested camera coverage within
the current projection/binary rounding tolerance, without changing sample values.
The browser camera starts at −10..10 on both axes and remains independent of
returned extrema. Dragging pans in both dimensions; reset restores both axes.
The first request covers only the visible x span. Off-screen coverage is optional
background work and does not take points from the first visible result. The screen API's
optional camera bounds set the vertical error scale; precision fallback retains
its own sampling criteria. Nice axis ticks use 1/2/5 steps, exact zero and decimal
places appropriate to zoom. Camera zoom stops before either axis loses reliable
pixel coordinate precision.

The browser also requests optional [dense screen-space envelopes](SCREEN_GRAPH_HTTP_API.md#optional-dense-oscillation-envelopes).
They are schema 3 display metadata; this precision schema 2 API does not produce
bands or change its connections. Supported dense carriers display a colored
range with an explicit message and no invented coordinate-inspector y value.
