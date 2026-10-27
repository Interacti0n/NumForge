# Browser function graphs

Open **Graphs / Grafy** (`/graph?lang=en` or `/graph?lang=sk`), enter `x*x`,
`sin(t)` or `1/x`, and press **Plot / Vykresliť**. Add up to four functions;
each row has its own expression, color, visibility and remove controls.
Blank rows are ignored. A failed function shows its own error while valid curves
remain visible. An empty plot still has axes and camera controls.

Both axes initially span −10..10. Expressions, colors, visibility and camera
persist in the same tab across language/tool navigation. Completed cached curves
return immediately; reload recalculates previously completed visible functions.
Editing clears only that row's old curve and keeps the camera and other results.

On desktop, the function inputs align with the graph preview; the function
library sits below the graph and edits the last focused row. It shares the
calculator's category order, button styles and description area: categories on
the left, functions on the right, with search across categories. The inline
library scrolls within its columns; the compact editor keeps pagination.
**Graph history**
keeps up to eight successful, explicitly plotted sets in the current browser
tab. Select one to restore its expressions, colors, visibility and camera.
Repeated sets are merged; errors and automatic pan/zoom requests add no entries.
Clear removes this local history without changing the current graph.
The highlighted entry matches the current inputs/view; colored dots also show
hidden curves dimmed. The × beside one entry removes only that set.

## Workspace files and image export

**Save workspace / Uložiť úlohu** downloads a local JSON file containing up to
four expressions, colors, visibility and the x/y view. **Load workspace** restores
these inputs, including unfinished/hidden rows. It cancels previous requests and
waits for explicit **Plot** (or **Locate function**) before calculating, including
after reload or resize. Invalid files leave the current work unchanged. Files
contain no sampled results, caches or calculator variables, and are not uploaded.

The UTF-8 format is `format:"numforge.graph-workspace"`, `version:1`,
`view:{x:[low,high],y:[low,high]}` and `curves:[{expression,color,visible}]`.
Limit: 100,000 bytes, 1–4 rows, 4096 UTF-8 bytes per expression, `#rrggbb`
colors and boolean visibility. Expressions cannot contain control characters;
axes must be finite, increasing and distinguishable at the browser's supported
relative span (at least `max(1,abs(low),abs(high))*1e-8`). Unknown versions,
invalid shapes/colors or oversized files are rejected. Saving a file does not
guarantee that its unfinished expressions are mathematically valid.

**Export → SVG / PNG** captures a completed visible view, with function labels,
current colors, confirmed markers and an approximate-result note. SVG carries
its own styles; PNG is a raster image up to 2048 pixels per side. Both work
offline once the graph is complete. Hidden curves and the hover crosshair are
omitted. Export performs no new mathematical evaluation and does not alter
history. Resolve or hide failed rows before exporting; pending sampling/marker
verification temporarily disables it. Extremely large exports are bounded to
8 MiB and 12,000 SVG elements; simplify the view if it exceeds these limits.
The image preserves the current approximation, not an exact mathematical proof.
Future engine versions can resample a workspace differently.

From [Equations](EQUATIONS.md#show-a-solution-in-the-graph), **Show in graph**
adds the difference between equation sides and centers near a selected real root.
It reuses matching expressions and respects the four-function limit. Graph
markers still require independent verification.

## Automatic curves and points

Ordinary real expressions draw curves. Recognized integer-domain expressions,
such as `x!`, `factorial(t)`, `ncr(x;2)` or `sqrt(x!)`, draw actual sampled dots
without connecting them. A constant `factorial(5)` remains a constant curve.
Transformed lattices such as `factorial(2*x)` are not inferred. Domain errors
are omitted. A wide view can skip integers; zoom in for detail.

Hover/tap a dot to inspect its sampled coordinates. Floor/ceil and related
functions remain step curves because they also accept noninteger inputs.
Rendering is automatic; there is no manual sample-count or mode control.

## Function editor

**Edit / Upraviť** beside a row opens its function editor.
**Functions and constants / Funkcie a konštanty** opens the last focused row.
Search names, aliases or localized descriptions, or select a category. Pagination
keeps entries reachable on small screens. Close with × or Escape.

Typing edits the original row immediately. Function insertion wraps selected
text as the first argument, or inserts an empty call with the caret inside.
Further arguments use semicolons, for example `root(;)`. Insertion does not
calculate. **Clear** affects only this row; **Plot** or Enter closes and submits.
The color palette changes appearance immediately without recalculating.

One free variable is inferred: `sin(t)` uses t, `x*x` uses x; constants use the
x axis. Multiple free names are rejected. The browser graph is independent of
calculator variables and ans. Angles use radians. Random expressions are not
accepted. For complex expressions, plot a supported real component explicitly.

There is no precision, angle or numeric range field on the page. Pan/zoom
select the view; C adapts sampling to the screen. Plotted coordinates are
approximations, not a promise of 34 accurate digits.

## Coordinate inspection

Move the mouse over a curve to inspect it. On touch/pen input, tap near the curve
and drag the selected point horizontally. Drag elsewhere to pan; a second finger
starts the pinch gesture. Inspection also works in the expanded view.

Readouts interpolate already displayed samples without another calculation.
They disappear in gaps and do not invent values inside dense bands. Mouse exit,
camera gestures or Escape dismiss the readout. Drawing coordinates are separate
from high-precision calculator results.

### Intercept and intersection markers

Small colored dots mark numerically confirmed axis crossings/touches and curve
intersections. Hover or tap near one to see its label and coordinates. Candidate
points come from displayed geometry; C checks the original expressions before
showing them. Coincident stretches, unresolved regions and domain gaps do not
become confirmed points.

Markers appear asynchronously and may be limited. Zoom or expand for detail;
crowded mobile plots can hide persistent dots while retaining their inspection
coordinates. Finite checks can miss roots or accept tiny residuals near a positive
minimum: they do not prove existence, uniqueness or complete discovery.
Detailed tolerances/budgets belong to the
[graph verification reference](../reference/GRAPH_ANALYSIS_HTTP_API.md).

## Curves and gaps

The browser draws connections accepted by C sampling. Undefined, complex,
quantity-valued or unresolved regions remain gaps. Continuous steep sections
can also be omitted when detail cannot be resolved. Steps are drawn as separate
plateaus instead of diagonal shortcuts. Curves are clipped to the camera;
large function values do not resize the axes.

Sampling and interpolation are heuristic. Narrow peaks, poles, isolated valid
points or oscillations may escape finite sampling. More precision or more points
alone does not prove accuracy between samples. See the
[screen sampler](../reference/SCREEN_GRAPH_HTTP_API.md) and its
[precision fallback](../reference/ADAPTIVE_GRAPH_HTTP_API.md) for supported shapes,
classification and limits.

## Dense oscillations

When full periods fit inside pixel columns, supported sin/cos expressions such
as `sin(1000x)` or `x*sin(x)+2` can show a translucent band. It represents an
approximate range across each column, not one exact value or exact local extrema.
Zoom in to reveal individual waves. Inspection skips bands.

Changing amplitudes, trends and cancellation can widen a band beyond values
actually attained. Unsupported combinations can remain unresolved. Supported
models and exclusions are defined in the
[dense-envelope contract](../reference/SCREEN_GRAPH_HTTP_API.md#optional-dense-oscillation-envelopes).

## Moving the view

| Action | Control |
| --- | --- |
| Zoom both axes | Mouse wheel, two-finger pinch, or focused plot +/− keys |
| Pan | Drag, or focused plot arrow keys |
| Reset to −10..10 | **Reset view / Obnoviť pohľad**, or Home |
| Center near the selected function at x=0 | **Go to function / K funkcii** |
| Expand | **Expand graph / Rozšíriť graf** |
| Return | **Back / Späť** or Escape |

Go to function prefers the last focused visible row. An undefined/non-real value
at zero reports an error and leaves the camera unchanged. Expanded view retains
the camera and curves and adapts sampling to its larger canvas. In short mobile
landscape layouts, **Controls** toggles camera actions and the legend.

During movement, cached geometry provides an immediate preview; missing detail
arrives after the gesture settles. Old requests cannot replace a newer view.
Zoom stops before the camera loses usable pixel precision; zooming out/resetting
remain available. Axis labels adapt between ordinary and scientific notation.

## Prepared ranges

Completed ranges are cached per function. Covered views redraw immediately;
missing regions are sampled while existing curves remain visible. Color changes
and hide/show retain caches. Hidden rows do no new sampling. Camera movement may
need fresh detail when a previously off-screen region is revealed.

Caches and speculative work are bounded and live only in the tab's current
workspace. Foreground requests have priority and individual functions retain
separate calculation budgets, so a multi-function update can take longer than
one request. This is not a persistent saved project.

Developers can find cache sizes, scheduling, interpolation and request policies
in [browser refinement](../reference/SCREEN_GRAPH_HTTP_API.md#browser-refinement).
API clients also retain explicit precision, angle, uniform-grid and read-only
session options documented in the [graph HTTP references](../API.md).
