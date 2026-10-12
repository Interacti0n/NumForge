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

**View and settings / Rozsah a nastavenia** contains optional increasing x bounds,
calculation precision (1–10000 significant digits, default 34) and radians/degrees.
Bounds are finite decimal literals, including scientific notation, not expressions
such as `pi` or `1/3`. They describe the displayed interval, not a domain restriction.
Precision controls approximate C mathematics; it does not control graph density.
Some functions retain at least 34 working digits. Plot coordinates still use the
rounded 12-place projection, regardless of a higher calculation precision.

## Calculator variables

Enable **Use calculator variables and ans** for read-only snapshots in the same
browser tab. Sampling does not create/reset a session or change variables, ans,
histories, previews or random state. The graph parameter shadows the session name
with the same spelling. A changed session revision rejects the graph request.

## Curves and gaps

The [adaptive C HTTP sampler](../reference/ADAPTIVE_GRAPH_HTTP_API.md) evaluates
midpoints and refines intervals with curvature or mixed point classifications.
The browser draws only connections accepted by the sampler. Domain errors,
complex values (including zero imaginary components), quantities and intervals
still unresolved at refinement bounds remain gaps. There are no point markers
or manual connection/sample-count controls. The sample table stays accessible.

Recognized continuous expressions, such as `sin(33*x)` or combinations of
sin/cos, polynomials and exp, permit a coarser line once intervals are small
relative to the selected x range. This favors a connected visual overview over
strict interpolation accuracy; calculation precision controls the sampled values,
not the accuracy of the line between them. Pole/jump operations keep stricter checks.

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
The y axis automatically fits discovered real extrema for resolved/recognized
continuous curves, with padding. Unresolved pole/domain trees retain the initial
overview so values near an asymptote do not flatten the rest of the plot. The
curve is clipped to that view. These are sampled extrema, not guaranteed global
maxima/minima; explicit y control remains future work.

## Moving the view

Use the mouse wheel, the +/− buttons or a two-finger pinch to change the x range.
Drag horizontally to pan. **Reset view / Obnoviť pohľad** restores x to −10..10;
y fits the resulting curve automatically. The focused plot also accepts +/−,
Left/Right arrows and Home. Empty plots support view controls without computing.

Sampling waits 300 ms after wheel/button changes or pointer release. During a
gesture the previous curve is projected into the new view; after sampling the
visible interval and automatic y bounds are refreshed. Edits/navigation cancel
pending work and old responses cannot replace a newer view. Bounds changes are
saved in the same per-tab settings used by advanced controls.

## Prepared ranges

The current page caches sampled ranges at several densities. Moving inside a
prepared range redraws immediately without another graph calculation. Zooming
uses available points immediately and refines them only when their range is too
coarse for the new view. Missing regions are requested separately; the existing
curve stays visible while those regions are calculated. Independently sampled
ranges retain their own connection/gap flags and are clipped at their boundaries.

After a quick, resolved request the page prepares adjacent left/right regions
and a wider overview during idle time, one request at a time. Slow/limited/failed
requests stop optional background work. Every request retains the C API's normal
time, allocation and output limits; unprepared intervals cannot be instantaneous.

The cache is bounded to 12 sampled ranges (each at most 1024 points), lives only
in the current page and clears on expression/precision/angle/session changes or
navigation. Session revisions are rechecked before settled cache reuse, so
changed coefficients/ans trigger new calculations. At most 1024 cached sample
rows are displayed in the value table, with an explicit truncation message.

Coordinates are rounded scientific strings from C. Browser overflow or underflow
is omitted, never plotted as a false zero. Collapsed x coordinates prevent drawing.
Each C sampling request has one five-second budget for preparation, every sample and serialization, with
existing allocation and response bounds. Refinement can finish early with a
limited plot when its remaining budget is too small for another probe. Actual
resource failures still fail atomically.

Settings persist per tab across language/navigation. Editing cancels pending work
and clears stale results. The original uniform sampling HTTP endpoint remains
available to API clients, but the web uses the adaptive endpoint.
