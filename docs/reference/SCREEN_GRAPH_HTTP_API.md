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
| `interpolation` | `linear`, `hermite` (requires both y bounds) | `linear` |
| `ymin`, `ymax` | Both finite decimal bounds with finite positive difference | Seed-derived vertical scale |

When supplied together, `ymin` and `ymax` define the visible vertical span used
for pixel error estimates. They do not discard samples outside the camera or
change returned `y_min`/`y_max` extrema metadata. Missing pairs, nonfinite bounds
and reversed/equal ranges fail. Precision fallback accepts these camera fields
but retains its existing adaptive criteria. Without them, fast API clients keep
the seed-derived vertical scale.

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

With `interpolation=hermite`, schema 3 can also contain a `slopes` array aligned
with `points`: approximate derivatives dy/dx as finite JSON numbers (up to nine
significant digits), or null when unavailable. The five-field point rows are
unchanged. Slopes are omitted if they would exceed the response bound; schema 2
fallback also omits them. Clients must support linear drawing when slopes are
absent/null. Slopes do not authorize connections across a false `connect` flag.
This is drawing metadata, not a public symbolic differentiation API.

## Sampling and budgets

There are 33 seeds, breadth-first midpoint refinement and depth at most 16.
Coarse work is capped at twice width (640–4096 points); refined work at 8192.
Points follow observed curvature rather than being forced up to the cap. Parent
midpoint acceptance is estimated at 8 pixels coarse / 2 pixels refined. Accepted
half-chords generally have about a quarter of that error for locally smooth
curvature; this is a heuristic, not a certified pixel bound.

With explicit camera y bounds, a restricted globally continuous AST can also
accept a parent no wider than one horizontal pixel, emitting two half-pixel
chords even when vertical curvature exceeds the usual midpoint tolerance.
This prevents steep off-screen polynomial/exp sections from consuming the
entire point budget. Captured phase/domain checks still apply; hidden whole
periods, poles, non-real and unresolved values are never joined by this rule.
The horizontal floor is a display approximation and does not certify vertical
interpolation error. Requests without camera bounds retain their old criterion.
See the [connected-curve comparison](../benchmarks/GRAPH_CONNECTIONS_2026_10_13.md)
for before/after work bounds and local HTTP measurements.

Captured trig arguments limit hidden whole periods. Denominator/log/sqrt/power
guards reject sampled domain crossings and tangent phase regions separate poles.
Smooth trees receive actual sin/cos extremum evaluations, checked within 1e-6
radians using up to eight bracket updates. No +/-1 ordinates are synthesized.
With camera bounds, forward-mode automatic differentiation computes f and f'
in the same internal bytecode pass. Rules cover arithmetic, powers in their real
derivative domain, sin/cos/tan (including DEG factors), exp, ln, sqrt/cbrt and abs.
An unavailable slope does not invalidate a valid function value. Cancellation
can still require precision classification; slopes remain approximate.
Accepted edges in restricted smooth trees with opposite finite derivative signs
receive bounded stationary-point searches: a safeguarded secant/bisection bracket,
at most 24 probes, and actual function evaluations. Phase/domain and existing
resource checks remain in force. Endpoint signs cannot discover every interior
root, and f'=0 alone is not a classification of an extremum.
Non-real/unresolved points are disconnected. Finite sampling can miss narrow
features; `limited` does not imply a timeout or prove discontinuity.

Uncertain/non-real samples use precision classification at the same projected
coordinate. At most 128 optional probes are allowed, with a 3 MiB / 300 ms
reserve. One shared five-second / 64 MiB cumulative / 512 KiB single-allocation
budget covers the request and serialization. Compact JSON is capped at 512 KiB;
precision schema 2 stays at 256 KiB, uniform/session limits are unchanged. Actual
resource failure returns no partial success.

## Browser refinement

The web requests Hermite slopes and draws shape-preserving cubic segments from
accepted adjacent C samples when their projected horizontal spacing exceeds one
pixel. Tangents are limited to keep interpolation inside the endpoint ordinates;
missing/nonfinite slopes, equal ordinates and subpixel segments use straight
lines. This avoids artificial overshoot while preserving sampler gaps; it is not
a certified interpolation bound. See the [smooth-zoom audit](../benchmarks/GRAPH_SMOOTH_ZOOM_2026_10_13.md).

Camera movement immediately reuses old cached segments. An uncached settled move
requests coarse data after 90 ms idle or pointer release, paints it for a frame,
then requests refined data. During an active drag/pinch sampling waits for release. A
precision fallback is already final and is not requested twice. Each HTTP
operation has its own budget. Browser aborts reject obsolete responses but do
not promise interruption of C work already running on the server.

The browser camera starts at −10..10 on both axes, independently of the curve.
Its first request samples −30..30 with proportionally increased width (capped
at 2048) and the visible y span. Prepared coverage redraws immediately; finer
zoom may require refinement. Drag/pinch and arrows move in both dimensions;
vertical translation only reprojects data. Reset restores both axes. Nice ticks
use 1/2/5 steps and additional decimal places as zoom increases; zero is exact.
The browser does not use response extrema to resize its camera.

After 180 ms idle, quick resolved requests prepare the current x range for the
next zoom level (up to twice the width, half the vertical span), then missing
adjacent ranges. At most three sequential requests run per batch; slow/limited
responses stop preparation. Speculative data is rejected if retaining it with
the active view would exceed point/weight bounds. Fast local calculations do not
guarantee instantaneous responses over a remote tunnel or for arbitrary functions.

The browser supports up to four independent function rows in a shared camera.
Each row uses this existing single-expression endpoint, inferred parameter,
cache and request generation. A failed/deleted/edited row does not replace another
row's results. Foreground requests are serialized with priority over background
preparation; each request has its own original budget, not one shared five-second
budget across all four functions. A color change only repaints its own layer.

Cache bounds per function are 12 ranges / 12288 points / 8 MiB estimated weight
(up to 48 ranges / 49152 points / 32 MiB for four functions). Eviction
prefers off-screen/coarse data over the current fine view. Dense partial jobs
replace the whole viewport so needed tiles are not evicted mid-composition.
Coarse entries cannot satisfy a final cache hit; resizing can request more detail
without clearing useful old data. SVG commands share only accepted adjacent
segments; gaps start new paths. At a cache boundary, both neighboring accepted
real edges can be trimmed to the same existing C sample and drawn as one stroke,
without clipping the stroke at that internal boundary. False connections,
non-real values and missing coverage prevent joining. Cached entries themselves
are not mutated. The page has no sample table. Its only function
controls are expression/color rows, visibility toggles, add/remove actions and a searchable library
sharing calculator metadata plus C registry availability. Random calls are omitted.
Color changes repaint without a request. Row/legend visibility toggles retain
bounded per-function caches, abort pending hidden work and skip hidden functions
during camera sampling. Showing an unchanged completed view is a local repaint;
a changed view reuses cached coverage and samples missing detail if needed. The view controls are reset and a nearly
full-viewport native dialog, with the same canvas/camera and Escape/button return.
Wheel, pinch and keyboard +/− zoom remain available; expanded/resized views can
request detail for their new dimensions (up to the existing 2048×1200 API bounds). Browser requests use radians, default working precision and no retained
calculator client. Configurable HTTP parameters remain available to API clients.

Response `limited` describes the entire sampled range. The browser warning is
restricted to unresolved samples and disconnected real edges that overlap the
camera's x bounds and whose sampled ordinates intersect its y bounds. Known
complex/domain/quantity gaps do not produce a refinement warning by themselves.
Panning vertically rechecks this message without reevaluation.
