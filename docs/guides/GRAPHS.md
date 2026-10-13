# Browser function graphs

Open **Graphs / Grafy** (`/graph?lang=en` or `/graph?lang=sk`). Enter an expression
such as `x*x`, `sin(t)` or `1/x`, then press **Plot / Vykresliť**. Add up to four
functions with **Add function / Pridať funkciu**; each has its own expression,
color picker, visibility button and remove button. The legend identifies their colors. Each function
has a different default color, and colors remain adjustable without calculation.
Toggle the eye button beside a row or click its colored legend entry to hide or
show it, including in the expanded graph. Hidden functions keep their expressions,
colors and bounded sample caches; they are skipped by Plot and camera sampling.
Showing a completed curve in the same view needs no new request. After moving or
zooming, cached coverage is reused and missing detail is sampled if needed.
Visibility is saved per tab alongside expressions and colors.

Blank rows are not evaluated. An empty plot still shows axes; a failed function
shows its own error while valid curves remain visible.

Both axes start at −10..10. Editing one of several functions clears only its old
curve and retains the camera and other results. The expressions and colors persist
per tab across language changes/navigation; a newly opened page starts around zero.

## Function library

Open **Function library / Knižnica funkcií** below the expression rows. Search by
name, common abbreviation or localized synonym, or choose a category. Names,
aliases and descriptions are shared with the calculator, and the C registry owns
availability. Constants π, e and φ support the same search aliases. Random calls
are excluded because plotting rejects them.

Clicking inserts into the last focused function row. Selected text is wrapped
as the first argument. Without a selection, a call such as `sqrt()` is inserted
with the caret between the parentheses. Required extra arguments get semicolon
separators, for example `root(;)`; type the first argument, then move past the
semicolon to enter the second. When wrapping selected text in a multi-argument
call, the caret moves to the second argument. Closing parentheses remain required.
Insertion does not calculate; press Plot or Enter when the expression is ready. Integer/domain-sensitive calls
can produce gaps; use real components to plot complex expressions.

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

## Coordinate inspection

Move the mouse over the plot to inspect the closest visible curve at the pointer's
horizontal coordinate. A colored marker/crosshair identifies the function and
shows approximate `x`, `y` coordinates. On touch/pen input, tap within 32 CSS pixels
vertically of a curve, then drag the selected point horizontally along that curve.
The readout remains after release. Drag elsewhere to pan; a second finger switches
to the normal pinch gesture. Inspection also works in the expanded graph.

Values come from already displayed cached samples, using the same limited cubic
or linear interpolation as the stroke. No HTTP request or expression evaluation is
triggered by inspection. These are approximate drawing coordinates, not a separate
high-precision calculation. Hidden functions, non-real values, off-screen values,
isolated samples and rejected/unresolved connections are not inspected; the
marker disappears in gaps rather than joining across them. The display adapts
its decimal precision to the view and formats rounded zero as `0`. Mouse exit,
Escape with the canvas focused, and camera gestures dismiss the readout.

## Curves and gaps

The [screen-space C sampler](../reference/SCREEN_GRAPH_HTTP_API.md) evaluates
midpoints and refines intervals with curvature or mixed point classifications.
The browser draws only connections accepted by the sampler. Domain errors,
complex values (including zero imaginary components), quantities and intervals
still unresolved at refinement bounds remain gaps. Samples have no permanent markers
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

Use the mouse wheel or a two-finger pinch to zoom both axes.
Drag in any direction to pan horizontally and vertically.
**Reset view / Obnoviť pohľad** restores both axes to −10..10.
The focused plot also accepts +/−, all four arrow keys and Home.
**Expand graph / Rozšíriť graf** moves the same canvas into a modal covering nearly
the whole viewport. Its camera and curves are retained; sampling adapts to the
larger screen area. Return with **Return to page / Vrátiť na stránku** or Escape.
There are no +/− buttons above the graph.
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
ranges, 12288 cached points and an estimated 8 MiB weight **per function**, and lives only
in the current page and clears on expression changes or navigation. Color changes
keep the cache. Four functions therefore permit at most 48 retained ranges,
49152 cached points and 32 MiB estimated cache weight. This is a conservative
accounting weight, not a browser heap measurement. Foreground C requests and
optional preparations are serialized; visible work has priority over background
tiles. Each range/function request retains its own five-second budget, so a
multi-function update can take longer overall. HTTP clients can still request sample data, configurable precision,
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
