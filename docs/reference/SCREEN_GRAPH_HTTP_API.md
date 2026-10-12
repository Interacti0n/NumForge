# Screen-space graph HTTP API

`POST /api/graph/plot?min=-10&max=10&mode=fast&width=800&height=440`
accepts a text expression through the existing C parser. This internal C
application adapter does not change the installed SDK or calculator arithmetic.
Without `mode=fast`, the [precision adaptive API](ADAPTIVE_GRAPH_HTTP_API.md)
still returns schema 2.

## Requests and precision

Bounds, optional parameter, angle, retained client and pinned session revision
follow the precision API. Assignments/random calls and `points` are rejected.
Additional fields require `mode=fast`:

| Field | Allowed values | Default |
| --- | --- | --- |
| `width` | Integer 320–2048 | 800 |
| `height` | Integer 160–1200 | 440 |
| `quality` | `coarse`, `refined` | `refined` |

Default calculation precision 34 permits approximate C double arithmetic; this
does **not** mean 34 accurate digits. Non-default precision, unsupported syntax,
unrepresentable bounds or mostly uncertain seeds use BigDecimal/BigRational
instead. Such responses have schema 2 and `evaluation:"precision"`; clients must
support both schemas. Resource failures are not optional fallback and session
data is never mutated.

## Compact response, schema 3

Fast responses have `sampling:"screen"`, `evaluation:"double"`, `parameter`,
requested `width`/`height`/`quality`, `count`, `limited`, `exact_probes`,
`plot_places:16`, numeric-or-null `y_min`/`y_max`, and `points`. Each row is:

`[x, y, connect, kind, source_approximate]`

Finite coordinates are JSON numbers formatted with up to 17 significant digits.
Connection/source approximation flags are integers 0/1; the first connection is
0. Kind codes are 0 real, 1 domain error, 2 complex, 3 quantity, 4 unresolved.
Non-real/unresolved values have null y; real null y means outside double plotting
range. Fast arithmetic has source approximation 1; precision probes preserve
their typed source flag but still project coordinates to double. Per-point error
strings/columns are omitted; structural errors keep the normal API envelope.

## Sampling and budgets

There are 33 seeds, breadth-first midpoint refinement and depth at most 16.
Coarse work is capped at twice width (640–4096 points); refined work at 8192.
Points follow observed curvature rather than being forced up to the cap. Parent
midpoint acceptance is estimated at 8 pixels coarse / 2 pixels refined. Accepted
half-chords generally have about a quarter of that error for locally smooth
curvature; this is a heuristic, not a certified pixel bound.

Captured trig arguments limit hidden whole periods. Denominator/log/sqrt/power
guards reject sampled domain crossings and tangent phase regions separate poles.
Smooth trees receive actual sin/cos extremum evaluations, checked within 1e-6
radians using up to eight bracket updates. No +/-1 ordinates are synthesized.
Non-real/unresolved points are disconnected. Finite sampling can miss narrow
features; `limited` does not imply a timeout or prove discontinuity.

Uncertain/non-real samples use precision classification at the same projected
coordinate. At most 128 optional probes are allowed, with a 3 MiB / 300 ms
reserve. One shared five-second / 64 MiB cumulative / 512 KiB single-allocation
budget covers the request and serialization. Compact JSON is capped at 512 KiB;
precision schema 2 stays at 256 KiB, uniform/session limits are unchanged. Actual
resource failure returns no partial success.

## Browser refinement

Camera movement immediately reuses old cached segments. An uncached settled move
requests coarse data, paints it for a frame, then requests refined data. A
precision fallback is already final and is not requested twice. Each HTTP
operation has its own budget. Browser aborts reject obsolete responses but do
not promise interruption of C work already running on the server.

Cache bounds are 12 ranges / 12288 points / 8 MiB estimated weight. Eviction
prefers off-screen/coarse data over the current fine view. Dense partial jobs
replace the whole viewport so needed tiles are not evicted mid-composition.
Coarse entries cannot satisfy a final cache hit; resizing can request more detail
without clearing useful old data. SVG commands share only accepted adjacent
segments; gaps start new paths. The table shows at most 1024 samples plus a
truncation row.
