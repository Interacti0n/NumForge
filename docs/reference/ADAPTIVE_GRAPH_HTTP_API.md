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
- `limited`: refinement encountered a depth/local evaluation bound; unresolved
  intervals are disconnected instead of being silently joined.
- `y_min`/`y_max`: rounded scientific strings from real seed samples, or discovered
  real samples if the seeds had none; null when there are no real samples. These
  suggest a display view, not a domain/continuity/range certificate.

The browser adds padding to the suggested view, clips curves, rejects unsupported
binary numeric projections and shows an accessible table. Newly discovered poles
cannot inflate the seeded y range, but newly discovered extrema can be clipped.

## Bounded C refinement

Preparation parses once and snapshots only referenced values. Thirty-three exact
inclusive seed coordinates span the requested view. Midpoints are exact rational
coordinates. Every new sample uses the same typed C evaluator and inherited scope.

All-real triples compare absolute midpoint/chord error with 0.001 of the seeded
y span (constant span uses absolute seed magnitude, or 1 if zero). A rejected
interval is bisected; mixed real/non-real triples are refined to locate boundaries.
All-non-real triples stop with gaps. Work is limited to depth 12 and at most 30
new evaluations per initial interval, preventing one pole from consuming every
other interval. The hard sample-array bound is 1024; this policy uses at most 993.
One five-second/64 MiB cumulative/512 KiB single-allocation budget covers the
whole operation and serialization; JSON still must fit 128 KiB. Resource failure
returns no partial success. No per-point budgets or binary floating-point math
are used in the C sampler.

Finite midpoint sampling cannot prove continuity or find every narrow feature,
pole, isolated domain island or oscillation. It may miss an oscillation that
aliases its probes; it may omit a steep continuous interval. `connect` describes
the sampled heuristic only. It must never be advertised as a continuity proof.
