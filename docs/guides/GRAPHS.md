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
curve and retains the camera and other results. Expressions, colors, visibility
and the camera persist in the same tab across language changes/navigation.
Completed curves and their bounded caches return immediately during navigation
between tools, without repeating completed calculations. Reloading the tab
recalculates previously completed visible curves at the saved camera; unfinished
or edited inputs remain unsubmitted. A fresh tab starts around zero.

## Function editor

Open **Edit function / Upraviť funkciu** below the expression rows to open the
**Function editor / Editor funkcie** for the last focused row. Search by
name, common abbreviation or localized synonym, or choose a category. Names,
aliases and descriptions are shared with the calculator, and the C registry owns
availability. Constants π, e and φ support the same search aliases. Random calls
are excluded because plotting rejects them.
The catalogue opens in a compact modal instead of extending the page. Previous/
next buttons keep every entry reachable without an internal scrollbar; short
landscape windows show fewer entries per page. Close with × or Escape to return
to the selected expression. Search begins across all categories.
The selected row's complete expression is editable at the top of the open
editor. Typing, selection and function insertion update that same row; other
rows remain unchanged. Like the calculator, the full-width input has Plot and
Clear actions below it. Clear empties only this row without submitting it. On
large screens the calculator-style category buttons sit beside the function
buttons; smaller windows use a category selector to keep everything on screen.
Press Enter there, or Plot, to close the editor and draw.

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
isolated samples, dense ranges and rejected/unresolved connections are not inspected; the
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
checks. Precision fallback also follows the visible camera, so huge values outside
the view do not reduce detail on visible sinh/cosh branches. Neither calculation precision
nor point count alone guarantees interpolation accuracy. Pole/jump operations
keep stricter checks; unsupported operations retain the precision sampler.

In the double screen sampler, for a restricted continuous expression with explicit camera bounds, short
segments can remain connected once they are below half a horizontal pixel.
This avoids spending the point budget on steep off-screen curves such as x^10
or exp(x). Trigonometric phase checks still reject unresolved whole periods;
division, tangent and other domain-sensitive expressions keep stricter rules.
The displayed chord is an approximation, not an accuracy guarantee between samples.
Expressions containing `abs` retain vertical-error checks at subpixel widths.
Captured inner sign changes locate actual corners before accepting their two
neighboring intervals. The real screen form `sqrt(u^2)` shares this handling;
complex intermediates still use the original evaluator for classification.
For a recognized outer absolute sine/cosine model, dense envelopes are mapped
through absolute value. A single carrier without an additive trend needs a full
π phase span per column, so ranges such as `abs(x*sin(x))` include zero rather
than losing their valleys. Other absolute sums retain the full 2π criterion.

Where slopes are available, the browser draws smooth transitions between
accepted points, with tangent limits to avoid overshoot. The C sampler also
searches for stationary points inside accepted intervals of supported smooth
expressions, so shifted peaks and combined functions receive actual evaluated
anchors. These checks help shape and do not prove that all extrema were found.
For guarded domain-sensitive expressions such as `1/(1+((x-0.1234)/0.04)^2)`,
observed changes in slope can also trigger a bounded turning-point search.
The resulting actual sample splits the interval; both sides are checked again
before drawing. This helps narrow peaks and valleys without joining across gaps.

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
the plot may omit unresolved connections rather than draw misleading chords.
For supported dense sin/cos expressions, a translucent range replaces unresolved
individual waves; see the next section.
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

## Dense oscillations

When a full period fits into one pixel column, supported expressions such as
`sin(1000x)`, `2+3*cos(1000x)` or `x*sin(1000x)+2` show a translucent band in the
function color. The message identifies it as an approximate range. Zoom in to
reveal the individual curve; zoom out to return to the range. The band represents
possible values across a column, not one exact y value or exact local extrema.
A changing amplitude/trend or cancellation between terms can widen it beyond
the values actually attained, including the changing envelope of beats.
Coordinate inspection therefore omits bands rather than inventing a y value.

The implementation recognizes sums/differences of up to four sin/cos occurrences
with linear or quadratic arguments, constant/linear amplitudes and trend, for example
`sin(1000x)+sin(1001x)` or `sin(x*x)`. At least one term must complete a full
period within one column without a phase turn inside it; other terms contribute
bounded ranges including their interior extrema. This also supports a fast/slow
mixture such as `sin(x*x)+cos(1000x)`. Near a quadratic vertex, evaluated points
keep the ordinary curve if no other term is dense. Products of trig terms, larger sums, tangent, variable
fractions such as `sin(1000x)/x`, other nonlinear arguments and outer domain operations
retain normal sampling and may still exhaust its finite limits. Strong numerical
cancellation can also keep the precision fallback. Envelopes do not
join poles, domain errors or rejected sample edges. Their computation stays in
the same C evaluator/application layer and the existing time/space budgets.
Unsupported or unresolved mixtures can still exhaust the sample limit where
bands are unavailable; the unresolved-detail notice remains applicable.

## Moving the view

At intermediate zoom levels, supported quadratic oscillations use smooth
interpolation even below a horizontal pixel. C checks those arcs against extra
evaluated points and keeps actual stationary anchors. This avoids spending the
point budget on unnecessary straight segments while preserving the visible
curve. Sampling limits still apply to more demanding or unsupported expressions.

Use the mouse wheel or a two-finger pinch to zoom both axes.
Drag in any direction to pan horizontally and vertically.
**Reset view / Obnoviť pohľad** restores both axes to −10..10.
The focused plot also accepts +/−, all four arrow keys and Home.
**Expand graph / Rozšíriť graf** moves the same canvas into a modal covering nearly
the whole viewport. Its camera and curves are retained; sampling adapts to the
larger screen area. Return with **Return to page / Vrátiť na stránku** or Escape.
The expanded plot uses the available height, omits the repeated gesture hint and
ordinary success text, and retains the legend, controls and important warnings.
There are no +/− buttons above the graph.
Empty plots support view controls without computing.

Sampling waits 90 ms after wheel/button changes or pointer release. During a
gesture the previous curve is projected into the new view; after sampling the
visible interval is refined without changing the camera. Vertical panning
reprojects complete cached curves; deferred off-screen regions receive fresh
samples when revealed, including precision fallback. Edits/navigation cancel pending
work and old responses cannot replace a newer view. A fresh tab or edited single
expression starts around zero. The saved tab camera and completed graphs survive
navigation; the memory cache holds only the latest graph workspace and retains
its existing point/byte bounds. A larger canvas can refine restored geometry.

## Prepared ranges

The current page caches sampled ranges at several densities. Moving inside a
prepared range redraws immediately without another graph calculation. Zooming
uses available points immediately and refines them only when their range is too
coarse for the new view. Missing regions are requested separately; the existing
curve stays visible while those regions are calculated. Adequate refined entries
are preferred over narrower but insufficient entries; a coarse preview does not
discard a useful refined level of the same interval. Independently sampled
ranges retain their own connection/gap flags. Adjacent accepted edges share an
actual sampled boundary point and one continuous stroke, avoiding visible seams
between different densities. Rejected edges and missing coverage remain separate.

The first calculation samples only the visible camera interval: the default
view requests −10..10, using the visible x/y pixel scales. With several functions,
all foreground ranges complete before optional background requests can run.
This keeps off-screen oscillations and curvature from consuming the visible
view's point budget. A very dense visible function can still exhaust that budget;
`sin(1000x)` at the default scale uses the dense-range representation. Unsupported
dense expressions remain unresolved rather than falsely connected.

After 180 ms idle following a quick resolved request, an inexpensive view may
prepare one three-times-wide buffered range (−30..30 by default), with scaled
width capped at 2048. Views exceeding one third of the 8192-point fast cap skip
that wide work. In the normal canvas, next-zoom detail covers the current x range
at up to twice the width and half the y span, also preparing for expansion. In the
expanded canvas it covers only the central half of x at the current width and
half the y span. Prepared tiles are checked together; a single missing tail is
requested alone. A completed preparation is not repeated for a resize whose
visible detail is already sufficient. A batch makes at most two sequential
requests per function, with foreground priority over all speculative work.
A prepared buffer supports immediate panning; without coverage the existing
curve remains a preview while missing intervals are sampled after the gesture.
Cached slopes keep curves smooth while additional detail arrives. Preparation
cannot evict the active view for speculative data, and slow/limited/failed
responses stop the batch. Every request retains the normal C time, allocation
and output limits; unprepared intervals cannot be instantaneous.

The multi-function status counts accepted connected segments that intersect the
camera, not isolated numeric samples. Valid sampled values alone can leave an
unresolved function without a displayed stroke. Single-function detail and gap
warnings retain the same meaning.

Notes distinguish a curve outside the current view, no sampled real values,
unresolved detail, unit-valued results and browser numeric projection limits.
Domain notes explain why parts are empty; they are not parser failures. Each
function keeps its own note in a multi-function plot. Dense oscillation ranges
retain their separate zoom hint.

Camera-aware sampling can defer difficult off-screen refinement without joining
its edges. Moving vertically into those regions requests fresh C samples;
ordinary fully sampled curves still reproject directly from cache. For example,
the conventional real-domain expression `1/ln(ln(sqrt(x)))^2` starts at x>1
and has a pole at e²: its two branches must stay separate.

Step functions such as `floor`, `ceil`, `trunc` and `round` are drawn as separate
plateaus at observed jumps. The same guard applies inside combined expressions.
A wide view with many steps may leave unresolved gaps; zoom in to request more
detail. The graph does not join different levels with a diagonal shortcut.

Fast refined requests use at most 8192 points; precision fallback uses at most
2048. Unprepared settled moves display coarse data before adding detail. When
accepted cached strokes already cover the view without visible unresolved gaps,
they supply the preview and only refined data is requested.
The cache is bounded to 12 sampled
ranges, 12288 cached point-or-band records and an estimated 8 MiB weight **per function**, and lives only
in the tab's current graph workspace. Navigation retains one DOM-free snapshot;
returning consumes it, and editing the expression clears that function's cache.
Color changes keep the cache. Four functions therefore permit at most 48 retained ranges,
49152 cached records and 32 MiB estimated cache weight. This is a conservative
accounting weight, not a browser heap measurement. Foreground C requests and
optional preparations are serialized; visible work has priority over background
tiles. Each range/function request retains its own five-second budget, so a
multi-function update can take longer overall. HTTP clients can still request sample data, configurable precision,
angles and read-only session snapshots through the documented API; those options
are independent of the browser graph controls.

The C screen sampler can project negligible exponential tails to zero when its
propagated error fits a small fraction of a pixel. It searches exponent turns
and nearby shoulders so narrow Gaussian peaks remain visible. This display
approximation does not change calculator results or precision sampling.
Coordinates are rounded JSON numbers or scientific strings from C. Further browser overflow or underflow
is omitted. Collapsed x coordinates prevent drawing.
Each C sampling request has one five-second budget for preparation, every sample and serialization, with
existing allocation and response bounds. Refinement can finish early with a
limited plot when its remaining budget is too small for another probe. Actual
resource failures still fail atomically.

Settings persist per tab across language/navigation. Editing cancels pending work
and clears stale results. The original uniform sampling HTTP endpoint remains
available to API clients, but the web uses the adaptive endpoint.
