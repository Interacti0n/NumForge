# Browser function graphs

Open **Graphs / Grafy** (`/graph?lang=en` or `/graph?lang=sk`). Enter an expression
such as `x*x`, `sin(t)` or `1/x`, then press **Plot / Vykresliť**. Computation runs
only on submission. Empty input, editing and request errors retain a coordinate
grid from −10 to 10 on both axes; empty submissions do not contact the server.
The requested x view starts at −10 to 10 around zero.

Sample density and the parameter are automatic. The C parser chooses the unique
variable not present in the selected calculator session. `sin(t)` uses `t`;
`a*x` uses `x` if `a` is stored. Multiple unknown names are rejected rather than
guessed. With no free variable the graph uses `x`, shadowing a referenced session
`x` or plotting a constant expression. Constants, function aliases and strings
are AST nodes, not candidate letters. Names remain case-sensitive.

**View and settings / Rozsah a nastavenia** contains optional increasing x and y bounds,
calculation precision (1–10000 significant digits, default 34) and radians/degrees.
Bounds are finite decimal literals, including scientific notation, not expressions
such as `pi` or `1/3`. They describe the displayed interval, not a domain restriction.
Default precision 34 permits fast approximate drawing in C. Another precision
value uses the precision evaluator; unsupported or uncertain expressions also
retain that path. Coordinates have at most 17 significant digits regardless of
calculation precision. Density is automatic. The uniform API retains 12 places.

## Calculator variables

Enable **Use calculator variables and ans** for read-only snapshots in the same
browser tab. Sampling does not create/reset a session or change variables, ans,
histories, previews or random state. The graph parameter shadows the session name
with the same spelling. A changed session revision rejects the graph request.

## Curves and gaps

The [screen-space C sampler](../reference/SCREEN_GRAPH_HTTP_API.md) evaluates
midpoints and refines intervals with curvature or mixed point classifications.
The browser draws only connections accepted by the sampler. Domain errors,
complex values (including zero imaginary components), quantities and intervals
still unresolved at refinement bounds remain gaps. There are no point markers
or manual connection/sample-count controls. The sample table stays accessible.

Screen-space refinement follows visible curvature and captured trig/domain
checks. Precision fallback can use coarser chords for recognized continuous
expressions such as sin/cos, polynomials and exp. Neither calculation precision
nor point count alone guarantees interpolation accuracy. Pole/jump operations
keep stricter checks; unsupported operations retain the precision sampler.

Coarse trigonometric curves also evaluate points near sin/cos phase extrema, so
missed peaks do not create a false changing amplitude. These are actual C samples;
they do not replace function values with assumed ±1 results. Complex expressions
and unresolved detail still have finite sampling limits.

Connections are heuristic, not proofs of continuity. Narrow poles, discontinuities,
isolated valid values or oscillations can escape finite sampling. Gaps can also
omit a continuous but unresolved steep curve. A message reports exhausted
refinement. Forward trigonometric phases receive an extra check against hidden
whole periods. If a wide range contains too many oscillations, narrow the x range;
the plot may omit all connections rather than draw misleading chords.
Both axes start at −10..10 and stay independent of the function's extrema.
The curve is clipped to the chosen camera; large values do not resize the axes.
Explicit y bounds are available alongside x bounds in the settings.

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

Sampling waits 300 ms after wheel/button changes or pointer release. During a
gesture the previous curve is projected into the new view; after sampling the
visible interval is refined without changing the camera. Vertical panning
reprojects cached values without a calculation. Edits/navigation cancel pending
work and old responses cannot replace a newer view. A newly opened page or edited
expression starts around zero; expression, precision, angle and session settings
persist per tab.

## Prepared ranges

The current page caches sampled ranges at several densities. Moving inside a
prepared range redraws immediately without another graph calculation. Zooming
uses available points immediately and refines them only when their range is too
coarse for the new view. Missing regions are requested separately; the existing
curve stays visible while those regions are calculated. Independently sampled
ranges retain their own connection/gap flags and are clipped at their boundaries.

The first calculation prepares three times the visible x range in one bounded
request: the default −10..10 view uses samples from −30..30. Sampling width is
scaled to preserve visible detail, up to the API width cap, and the current y
span supplies the vertical pixel scale. The browser displays only the camera's
central part. A sufficiently detailed buffered range supports immediate panning
without another calculation. Zooming farther in still needs additional detail.

After a quick, resolved request the page prepares missing adjacent left/right regions
and a wider overview during idle time, one request at a time. Slow/limited/failed
requests stop optional background work. Every request retains the C API's normal
time, allocation and output limits; unprepared intervals cannot be instantaneous.

Fast refined requests use at most 8192 points; precision fallback uses at most
2048. Unprepared settled moves display coarse data before adding detail.
The cache is bounded to 12 sampled
ranges, 12288 total cached points and an estimated 8 MiB weight, and lives only
in the current page and clears on expression/precision/angle/session changes or
navigation. Session revisions are rechecked before settled cache reuse, so
changed coefficients/ans trigger new calculations. At most 1024 cached sample
rows are displayed in the value table, with an explicit truncation message.

Coordinates are rounded JSON numbers or scientific strings from C. Browser overflow or underflow
is omitted, never plotted as a false zero. Collapsed x coordinates prevent drawing.
Each C sampling request has one five-second budget for preparation, every sample and serialization, with
existing allocation and response bounds. Refinement can finish early with a
limited plot when its remaining budget is too small for another probe. Actual
resource failures still fail atomically.

Settings persist per tab across language/navigation. Editing cancels pending work
and clears stale results. The original uniform sampling HTTP endpoint remains
available to API clients, but the web uses the adaptive endpoint.
