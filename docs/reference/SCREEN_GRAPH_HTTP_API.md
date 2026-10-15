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
| `dense` | `band` (requires both y bounds) | No envelopes |

When supplied together, `ymin` and `ymax` define the visible vertical span used
for pixel error estimates. They do not discard samples outside the camera or
change returned `y_min`/`y_max` extrema metadata. Missing pairs, nonfinite bounds
and reversed/equal ranges fail. Precision fallback also uses the camera: a parent
midpoint tolerance of at most one vertical plot pixel, retaining a finer seeded
tolerance when searching larger intervals. A supported smooth parent no wider
than one horizontal plot pixel may use the camera tolerance once its midpoint
error also passes; there is no width-only shortcut. This preserves tiny seed
signals that can reveal narrow peaks without oversampling discovered features.
Without these bounds, clients keep
the seed-derived vertical scale.

Default calculation precision 34 permits approximate C double arithmetic; this
does **not** mean 34 accurate digits. Non-default precision, unsupported syntax,
unrepresentable bounds or mostly uncertain seeds use BigDecimal/BigRational
instead. Such responses have schema 2 and `evaluation:"precision"`; clients must
support both schemas. Resource failures are not optional fallback and session
data is never mutated.

Step calls (`floor`, `ceil`, `trunc`, `round`) retain precision fallback. Its
shared trig/step metadata rejects connections whenever a captured step value
differs across the endpoint/midpoint triple, including nested/composed calls.
This guard is independent of vertical pixel tolerance and does not change
calculator rounding or add HTTP fields. Dense steps can reach the existing
2048-point/depth limits and leave gaps; finite sampling is not a completeness
proof. See the [precision contract](ADAPTIVE_GRAPH_HTTP_API.md).

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

With explicit y bounds, schema 3 or schema 2 precision fallback may include
`screen_clipped:true`. This means
some off-screen refinement was deferred; their `connect` flags remain false.
Absence/false means no such deferral. Clients must resample before using these
gaps in a new y view outside the requested y bounds. It is separate from
`limited`, which reports unresolved sampling, and from mathematical domain gaps.
Point rows and legacy requests without camera bounds retain their contracts.
Camera-aware precision fallback returns its requested `width`/`height` and
`quality:"refined"`, even for a coarse request: it uses the same bounded precision
refinement rather than the double sampler's coarse shortcut. It still has no
slopes or bands. All numeric decisions remain BigDecimal/BigRational operations.

Precision deferral requires three ordered real probes beyond the same camera
edge by two pixel tolerances, a parent at most eight horizontal plot pixels wide,
and the existing strict trig-phase checks. Deferred edges remain false and must
be resampled when a new y view reveals them. This sampled display heuristic does
not prove that no unseen turn exists. Point/depth/time/allocation limits stay
unchanged; `limited` remains distinct from deliberate screen deferral.

## Optional dense-oscillation envelopes

With `dense=band`, supported schema 3 results may include
`dense_representation:"pixel-envelope"` and `bands`. Each band is
`[x_min, x_max, y_min, y_max]` with four finite JSON numbers, ordered in x,
non-overlapping, and at most `width` bands (maximum 2048). They describe an
approximate range of possible ordinates across one requested pixel column;
they are not points, edges, exact local extrema or a certified error enclosure.
Point rows and connection flags are unchanged. `y_min`/`y_max` at the top level
remain sampled-point extrema and do not include envelope bounds.

The C bytecode recognizes a sum/difference of up to four sin/cos carrier
occurrences with affine amplitudes, affine/quadratic phases and an affine trend:
`sum((a*x+b)*sin/cos(q*x*x+k*x+c)) + d*x+e`. Constants, arithmetic, constant-only
operations and nonzero constant division can form those coefficients. Identical
phase/kind terms are combined in the metadata. Multiplication, square/postfix
square and power two can form a quadratic phase. More than four occurrences,
products of carriers, variable denominators, tangent, higher/non-polynomial phases,
nonlinear amplitudes, clipping and domain-sensitive outer transformations retain
ordinary sampling. Examples include `sin(1000x)`, `2-3*cos(1000x+0.3)`,
`x*sin(1000x)+2`, `sin(1000x)+x`, `sin(1000x)+sin(1001x)`,
`x*(sin(1000x)+cos(1100x))+2`, `sin(x*x)` and `cos(-3*(x-0.1234)^2)`.
Parameter inference and requested RAD/DEG are retained.

A column needs at least a full phase turn for every remaining carrier,
with numerical margins; the phase must be monotone across that column.
Phase endpoints must stay within the fast evaluator's existing ±1e6-radian range.
The sum of absolute amplitude endpoint bounds plus the trend endpoint range
produces the approximate band bounds. A varying amplitude/trend or cancellation
between different carriers can overestimate the values actually attained in that
column, including the envelope of beats. Floating coefficients and rounding remain approximate; this is not
public interval arithmetic. Unsupported or mostly uncertain expressions (including
some cancelled intermediate results) keep
the precision fallback, which has no bands. The option never relaxes point
connection, pole or domain guards.

Quadratic phase vertices are evaluated in the original expression as additional
discovery seeds, without inventing output extrema or accepting neighbouring
edges. Columns containing a phase turn retain ordinary samples. Coefficient
cancellation/roundoff margins can also defer a column to sampling. Resolvable
parts of recognized quadratic models retain midpoint-error and strict phase
checks even below a horizontal pixel; the smooth short-chord shortcut is disabled
there. Bands and ordinary curves can coexist in the same response. A fast carrier
mixed with a slow/turning carrier may still exhaust the point limit in the
unbanded region; this remains visible as unresolved detail.

Band-covered seed/refinement intervals do not consume more midpoint samples
or become accepted chords. Other intervals retain the normal bounded sampler.
`limited` reports unresolved sampling work, not the presence of a recognized
band. Clients requesting bands must interpret their separate representation;
legacy requests without the option retain their original point results.

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

For other ASTs, a parent up to eight horizontal pixels wide may stop refining
when its three real probes are guarded, have finite slopes of the same sign,
and all lie beyond the same y edge plus a tolerance margin. It accepts no new
connections and sets `screen_clipped`; viewport-aware clients request those
regions again if they become visible. Pole crossings, non-real/uncertain probes
and possible sampled turning points retain refinement. This is a screen-space
heuristic, not a proof that hidden extrema/discontinuities cannot exist.

Exact probes continue to classify complex/domain results. Confirmed non-real
seed values do not force the whole real portion into precision fallback. If a
known non-real intermediate becomes a real final value, such as `abs(sqrt(x))`
at negative x, the precision sampler remains necessary. Numeric uncertainty,
resource errors and the shared five-second budget retain their original rules.

Captured trig arguments limit hidden whole periods. Denominator/log/sqrt/power
guards reject sampled domain crossings and tangent phase regions separate poles.
For non-globally-smooth expressions with screen bounds, a sampled derivative sign
change prevents immediate midpoint acceptance. A significant sampled bend in the
derivative also explores intervals wider than one horizontal pixel, so a small
trend does not hide a narrow peak from the first probes. Where endpoint derivatives form
a guarded bracket, at most 32 bisection probes seek a turning-point candidate.
Numeric uncertainty uses the existing reserved exact classifier. An actual
sample replaces the midpoint only after real/finite/domain/phase checks; the
two new intervals are always refined at their own midpoints before connecting.
Optional search skips candidates visibly outside the camera and retains the
same shared time/allocation/point/exact-probe limits. An internal anchor flag
prevents repeated search at a numerically located turn without changing its
value or derivative. It adds no HTTP field and is not an exact extremum or
continuity certificate; an unobserved narrow feature can still evade sampling.
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

The web requests Hermite slopes and optional dense bands, and draws shape-preserving cubic segments from
accepted adjacent C samples when their projected horizontal spacing exceeds one
pixel. Tangents are limited to keep interpolation inside the endpoint ordinates;
missing/nonfinite slopes, equal ordinates and subpixel segments use straight
lines. This avoids artificial overshoot while preserving sampler gaps; it is not
a certified interpolation bound. See the [smooth-zoom audit](../benchmarks/GRAPH_SMOOTH_ZOOM_2026_10_13.md).

Camera movement immediately reuses old cached segments. When accepted cached
strokes cover the view without visible unresolved detail, only refinement is
requested; the cache already supplies the preview. An uncached settled move
requests coarse data after 90 ms idle or pointer release, paints it for a frame,
then requests refined data. During an active drag/pinch sampling waits for release. A
precision fallback is already final and is not requested twice. Each HTTP
operation has its own budget. Browser aborts reject obsolete responses but do
not promise interruption of C work already running on the server.

The browser camera starts at −10..10 on both axes, independently of the curve.
Its first request samples only −10..10 with the visible width and y span.
All visible functions complete foreground work before background buffers run. Prepared coverage redraws immediately; finer
zoom may require refinement. Drag/pinch and arrows move in both dimensions;
vertical translation only reprojects data. Reset restores both axes. Nice ticks
use 1/2/5 steps and additional decimal places as zoom increases; zero is exact.
The browser does not use response extrema to resize its camera.

After 180 ms idle, quick resolved views using no more than one third of the
8192-point cap may prepare a three-times-wide buffer with scaled width capped
at 2048, followed by next-zoom detail over the current x range (up to twice
the width, half the vertical span). In the expanded canvas, next-zoom preparation
uses only the central half of x at the current width and half the vertical span.
Prepared tiles are checked together and a single missing tail is sampled alone.
A completed batch is not repeated for a resize with adequate foreground detail.
Denser views skip wide preparation. At most
two sequential requests run per batch; slow/limited responses stop preparation. Speculative data is rejected if retaining it with
the active view would exceed point/weight bounds. Fast local calculations do not
guarantee instantaneous responses over a remote tunnel or for arbitrary functions.

The browser supports up to four independent function rows in a shared camera.
Each row uses this existing single-expression endpoint, inferred parameter,
cache and request generation. A failed/deleted/edited row does not replace another
row's results. Foreground requests are serialized with priority over background
preparation; each request has its own original budget, not one shared five-second
budget across all four functions. A color change only repaints its own layer.

Cache bounds per function are 12 ranges / 12288 point-or-band records / 8 MiB estimated weight
(up to 48 ranges / 49152 records / 32 MiB for four functions). Eviction
prefers off-screen/coarse data over the current fine view. Dense partial jobs
replace the whole viewport so needed tiles are not evicted mid-composition.
Remembering the same response object is idempotent, including a limited
precision preview that already supplies the refined result. If bounded cache
eviction still leaves visible coverage missing after the planned partial jobs,
the foreground operation makes at most one complete-view repair request.
It keeps the same camera and operation generation; an edit/hide/navigation
cancels or discards the stale response. A failed repair finishes in an error
state rather than repeatedly retrying or leaving a loading message. Successful
completion refreshes the status and drawing after clearing the busy flag.
This does not join rejected numeric edges or relax cache/request limits.
Coarse entries cannot satisfy a final cache hit or discard a useful refined
level of the same range. Adequate refined tiles take precedence over narrower
but insufficient tiles; resizing can request more detail
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

For screen sampling with a vertical span, exponential underflow may project to
zero with an absolute error allowance of `(ymax-ymin)/height/64` (or the supplied
vertical span). The internal evaluator bounds an underflowed exp by `DBL_MIN`
only when its argument plus estimated error is below `log(DBL_MIN)`. Supported
arithmetic propagates that bound, with a nonzero floor to prevent underflow of
the bound itself. Denominators containing projected tails and nonlinear
magnification retain uncertainty/exact fallback; a zero display value is never
reused as an exact mathematical zero. Small propagated argument roundoff can
also be resolved at a well-conditioned exp within the screen allowance.

Before refinement, up to 40 bisection probes locate sampled derivative turns
of exp arguments, including turns exactly on a seed. Actual function values at
these candidates and curvature-estimated shoulders become discovery seeds.
Their edges still require the usual domain/phase and midpoint checks; curves
with discovered exp turns retain the midpoint error test below a horizontal
pixel. Counts, shared budgets, optional-work reserves and public schemas are
unchanged. This is a finite display heuristic, not a completeness/interval proof.
Ordinary fast probes, calculator evaluation and explicit precision fallback do
not opt into this screen projection; extreme exp scale errors can still occur
there or in unsupported compositions.

Response `limited` describes the entire sampled range. The browser warning is
restricted to unresolved samples and disconnected real edges that overlap the
camera's x bounds and whose sampled ordinates intersect its y bounds. Known
complex/domain/quantity gaps do not produce a refinement warning by themselves.
Panning vertically rechecks this message without reevaluation.

Mouse/touch coordinate inspection is a client-only overlay on accepted cached
edges. It uses the exact same limited cubic/linear drawing representation,
excludes hidden/non-real/rejected edges and labels its values approximate. Hover
and point tracing do not call the HTTP or C evaluator. Two-finger gestures and
empty-area dragging retain normal camera sampling behavior.

The multi-function UI counts accepted connections intersecting the camera. A
numeric sample set without accepted visible edges is not counted as a plotted
function; this does not change HTTP schemas or numerical acceptance rules.

Dense ranges use the curve color with a translucent fill and an explicit SK/EN
message. Zooming refines them into accepted strokes; band cache hits require
at least the current horizontal density. Color/visibility/expanded-view controls
apply equally. Coordinate inspection never assigns a single y value to a band.
Bands count as displayed functions, separately identified in the status. A
subpixel-height band paints a one-pixel trace clipped to the camera, without
changing its numeric bounds. Entirely off-screen bands are omitted. Each
point costs 256 bytes and each band 128 bytes in conservative cache accounting,
in addition to twice the response text size; these weights are not heap sizes.
