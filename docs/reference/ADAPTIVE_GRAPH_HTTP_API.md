# Adaptive graph HTTP API

`POST /api/graph/plot?min=-10&max=10` accepts one text expression. It is an internal
C application adapter, not an installed public SDK addition. The original
[uniform sampler](GRAPH_HTTP_API.md) and its schema 1 remain unchanged.

## Query and parameter selection

Required `min`/`max` use the uniform sampler's finite decimal literal grammar
and must be increasing. Optional `precision` (1–10000, default 34), `angle`
(rad/deg), retained `client` and pinned `at` have the same contracts. `points`
is rejected: the C sampler determines density. Many approximate functions retain
at least 34 working digits even when a lower precision is requested. Exact
arithmetic remains exact; plotting strings still use the 12-place projection. Optional `parameter` explicitly
selects a local name; otherwise it is inferred from the one parsed AST.

Inference selects the unique variable absent from the supplied session. Session
variables are coefficients, except that when no free names exist the local name
is x, shadowing a referenced session x. With no variable the expression is constant
on x. Multiple free names return `undefined variable` with the diagnostic code
`graph_parameter_ambiguous`. Constants, function names/aliases and string arguments
are never candidates. No session is created, revived or changed.

## Response schema 2

Success retains `ok`, `parameter`, `count`, `plot_places:12`, and classified
`points` from the uniform contract. It adds:

- `schema_version:2`, `sampling:"adaptive"`.
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
binary numeric projections and shows an accessible table. Newly discovered poles
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
the whole view, limited to depth 12 and 1024 total samples. For real arguments of
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
They share the original 1024-sample/request budget. Insufficient remaining points,
3 MiB allocation reserve or 300 ms time reserve mark the graph limited while
preserving already accepted chords; actual allocation/time failures remain atomic.

Before each new probe the sampler checks remaining cumulative bytes and time,
reserving 2 MiB and 200 ms for finishing/serialization plus twice the largest
observed probe cost. It can stop with `limited:true` before exhausting the budget.
This is an estimate, not a guarantee for an unexpectedly expensive next sample.
One five-second/64 MiB cumulative/512 KiB single-allocation budget covers the
whole operation and serialization; JSON still must fit 128 KiB. Resource failure
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
