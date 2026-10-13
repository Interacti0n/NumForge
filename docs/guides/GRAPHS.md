# Browser function graphs

Open **Graphs / Grafy** (`/graph?lang=en` or `/graph?lang=sk`). Enter an expression
such as `x*x`, `sin(t)` or `1/x`, then press **Plot / Vykresliť**. Computation runs
only on submission. Empty input, editing and request errors retain a coordinate
grid from −10 to 10 on both axes; empty submissions do not contact the server.
The requested x view starts at −10 to 10 around zero.

The controls contain only the function expression and **Curve color / Farba krivky**.
Changing color repaints cached curves without another calculation. The expression
and color persist per tab across language changes and navigation; a new page starts
at −10..10 around zero on both axes.

The C parser infers the unique variable in the expression. `sin(t)` uses `t`,
`x*x` uses `x`, and constant expressions use the x axis. Multiple names such as
`a*x` or `x+y` are rejected. The graph is independent of calculator variables and
ans; built-in constants such as pi remain available. Names are case-sensitive.

Angles always use radians. The page selects the C screen sampler with the server's
default working precision 34, automatic pixel-scale refinement and precision
fallback for unsupported or uncertain expressions. Fast projected coordinates have
at most 17 significant digits; this is an approximate drawing, not a promise of 34
accurate digits. There is no precision, angle or range input on the page: zoom and
pan choose the view, and the sampler adapts its density to that view.

## Curves and gaps

The [screen-space C sampler](../reference/SCREEN_GRAPH_HTTP_API.md) evaluates
midpoints and refines intervals with curvature or mixed point classifications.
The browser draws only connections accepted by the sampler. Domain errors,
complex values (including zero imaginary components), quantities and intervals
still unresolved at refinement bounds remain gaps. There are no point markers
or manual connection/sample-count controls. Sample values are not listed below the plot.

Screen-space refinement follows visible curvature and captured trig/domain
checks. Precision fallback can use coarser chords for recognized continuous
expressions such as sin/cos, polynomials and exp. Neither calculation precision
nor point count alone guarantees interpolation accuracy. Pole/jump operations
keep stricter checks; unsupported operations retain the precision sampler.

For a restricted continuous expression with explicit camera bounds, short
segments can remain connected once they are below half a horizontal pixel.
This avoids spending the point budget on steep off-screen curves such as x^10
or exp(x). Trigonometric phase checks still reject unresolved whole periods;
division, tangent and other domain-sensitive expressions keep stricter rules.
The displayed chord is an approximation, not an accuracy guarantee between samples.

Where slopes are available, the browser draws smooth transitions between
accepted points, with tangent limits to avoid overshoot. The C sampler also
searches for stationary points inside accepted intervals of supported smooth
expressions, so shifted peaks and combined functions receive actual evaluated
anchors. These checks help shape and do not prove that all extrema were found.

Coarse trigonometric curves also evaluate points near sin/cos phase extrema, so
missed peaks do not create a false changing amplitude. These are actual C samples;
they do not replace function values with assumed ±1 results. Complex expressions
and unresolved detail still have finite sampling limits.

Connections are heuristic, not proofs of continuity. Narrow poles, discontinuities,
isolated valid values or oscillations can escape finite sampling. Gaps can also
omit a continuous but unresolved steep curve. A message reports exhausted
refinement in a visible sampled region; unresolved detail elsewhere in the
prepared buffer does not show that warning. Known domain/complex gaps alone are
not a refinement warning. Forward trigonometric phases receive an extra check against hidden
whole periods. If a wide range contains too many oscillations, narrow the x range;
the plot may omit all connections rather than draw misleading chords.
Both axes start at −10..10 and stay independent of the function's extrema.
The curve is clipped to the chosen camera; large values do not resize the axes.
Move either axis with the plot controls or pointer gestures.

Axis marks are multiples of steps 1, 2 or 5 times a power of ten. The available
screen width determines their density. Normal views use integers; zooming adds
decimal places as needed. Zero has its own exact mark labelled `0`, regardless
of nearby sample values. Extremely small/large scales use scientific notation.
Interactive zoom stops when either axis span would fall below
`1e-8 * max(1, abs(axis_min), abs(axis_max))`, leaving precision for individual pixels.
The last valid view stays visible and zooming out or resetting remains available.
Extremely close to a sine maximum the curve is almost horizontal.

## Moving the view

Use the mouse wheel, the +/− buttons or a two-finger pinch to zoom both axes.
Drag in any direction to pan horizontally and vertically.
**Reset view / Obnoviť pohľad** restores both axes to −10..10.
The focused plot also accepts +/−, all four arrow keys and Home.
Empty plots support view controls without computing.

Sampling waits 90 ms after wheel/button changes or pointer release. During a
gesture the previous curve is projected into the new view; after sampling the
visible interval is refined without changing the camera. Vertical panning
reprojects cached values without a calculation. Edits/navigation cancel pending
work and old responses cannot replace a newer view. A newly opened page or edited
expression starts around zero. The expression and curve color persist per tab.

## Prepared ranges

The current page caches sampled ranges at several densities. Moving inside a
prepared range redraws immediately without another graph calculation. Zooming
uses available points immediately and refines them only when their range is too
coarse for the new view. Missing regions are requested separately; the existing
curve stays visible while those regions are calculated. Independently sampled
ranges retain their own connection/gap flags. Adjacent accepted edges share an
actual sampled boundary point and one continuous stroke, avoiding visible seams
between different densities. Rejected edges and missing coverage remain separate.

The first calculation prepares three times the visible x range in one bounded
request: the default −10..10 view uses samples from −30..30. Sampling width is
scaled to preserve visible detail, up to the API width cap, and the current y
span supplies the vertical pixel scale. The browser displays only the camera's
central part. A sufficiently detailed buffered range supports immediate panning
without another calculation. Zooming farther in still needs additional detail.

After 180 ms idle following a quick resolved request, the page first prepares
finer detail over the current x range for the next zoom level, then missing
adjacent left/right regions, one request at a time. Cached slopes keep curves
smooth while additional detail arrives. Preparation never evicts the active
prepared view just to retain speculative data. Slow/limited/failed
requests stop optional background work. Every request retains the C API's normal
time, allocation and output limits; unprepared intervals cannot be instantaneous.

Fast refined requests use at most 8192 points; precision fallback uses at most
2048. Unprepared settled moves display coarse data before adding detail.
The cache is bounded to 12 sampled
ranges, 12288 total cached points and an estimated 8 MiB weight, and lives only
in the current page and clears on expression changes or navigation. Color changes
keep the cache. HTTP clients can still request sample data, configurable precision,
angles and read-only session snapshots through the documented API; those options
are independent of the browser graph controls.

Coordinates are rounded JSON numbers or scientific strings from C. Browser overflow or underflow
is omitted, never plotted as a false zero. Collapsed x coordinates prevent drawing.
Each C sampling request has one five-second budget for preparation, every sample and serialization, with
existing allocation and response bounds. Refinement can finish early with a
limited plot when its remaining budget is too small for another probe. Actual
resource failures still fail atomically.

Settings persist per tab across language/navigation. Editing cancels pending work
and clears stale results. The original uniform sampling HTTP endpoint remains
available to API clients, but the web uses the adaptive endpoint.
