# Browser function graphs

Open **Graphs / Grafy** (`/graph?lang=en` or `/graph?lang=sk`). Enter one expression
such as `x*x`, `sin(x)` or `1/x`, choose the range, and press **Plot / Vykresliť**.
There is no automatic preview calculation while typing.

The parameter defaults to `x`; it can be another valid variable name. Range
endpoints accept finite decimal literals, including scientific notation, rather
than expressions (`-10`, `1e3`, not `-pi` or `1/3`). The range must be increasing.
Choose 2–1024 inclusive, uniformly spaced samples (default 257), calculation
precision 1–10000 (default 34), and radians or degrees.

All sample calculations use the [C application and HTTP sampler](../reference/GRAPH_HTTP_API.md).
Assignments and random calls are rejected. One request shares the five-second
budget and existing allocation/response limits; increasing the sample count or
precision does not guarantee completion. Errors clear the previous plot.

## Calculator variables

Check **Use calculator variables and ans** to read the existing calculator
session in the same browser tab. This does not start/reset a session or change
variables, ans, either history, previews or random state. The request reads the
current session revision before sampling and rejects a changed snapshot.
The graph's parameter shadows a calculator variable with the same name.
Unchecked graphs are independent of the calculator session.

## Interpreting the plot

The default view displays evaluated real points, with automatic vertical scaling
and coordinate axes. Hover a point for its rounded coordinate values, or open
**Sample values / Hodnoty bodov** for an accessible table of every sample.
Complex values (even if their imaginary component is zero), quantities and
point domain errors are classified as gaps. They are not projected onto a real
axis. The table retains classifications even if nothing can be plotted.

Coordinates are rounded scientific strings from the server, not exact exported
results. The browser only converts coordinates that fit its finite numeric
range; it omits overflowed or underflowed nonzero values instead of drawing a
false zero. Ranges whose x coordinates collapse to the same browser value cannot
be drawn. C calculation itself retains its arbitrary-precision types.

**Connect adjacent samples / Spojiť susedné body** optionally adds line segments.
It never connects across a classified/nonplottable sample. A uniform grid cannot
find every pole, rapidly changing feature or domain boundary. Connecting lines
can cross undetected discontinuities: neither dots nor segments prove continuity.
Bounded adaptive sampling, better discontinuity detection, pan/zoom and multiple
curves remain future work.

Form settings are saved per tab and survive language changes, navigation, Back
and Forward. Editing the form cancels the current request and clears stale
results. Calculations run again only when explicitly submitted.
