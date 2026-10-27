# Browser function graphs

Open **Graphs / Grafy** (`/graph`), enter `x*x`, `sin(t)` or `1/x`, then **Plot**.
Up to four rows have independent expressions, colors and visibility. Blank rows
are ignored; one failed row does not hide valid curves. Both axes start at −10..10.

Inputs and camera survive tool/language navigation in the same tab. Editing
clears only that row's curve. Reload recalculates previously completed visible
functions. **Graph history** keeps eight explicitly plotted sets; restoring one
recalls inputs and camera. Clear/× removes history without changing the graph.

## Workspace files and image export

**Save workspace** downloads expressions, colors, visibility and camera as local
JSON. **Load workspace** restores inputs and waits for explicit Plot; invalid
files leave current work unchanged. Files contain no sampled data or calculator
variables and are not uploaded. Format v1 allows four rows and 100 kB; the
[file owner](../../apps/web/assets/graph-files.js) validates fields and view bounds.

**Export → SVG / PNG** saves a completed visible view with labels, colors and
confirmed markers. No new calculation runs. Pending verification or failed
visible rows must finish/be hidden first. SVG is self-contained; PNG is up to
2048 pixels per side. Exports are bounded to 8 MiB/12,000 SVG elements and retain
the displayed approximation, not a mathematical proof.

[Equations can send a real solution to Graphs](EQUATIONS.md#show-a-solution-in-the-graph).

## Automatic curves and points

Ordinary real expressions draw curves. Recognized integer-domain expressions
such as `x!` and `ncr(x;2)` draw sampled dots; wide views may skip integers.
Transformed lattices such as `factorial(2*x)` are not inferred. Floor/ceil remain
step curves. Sampling mode is automatic.

## Function editor

**Edit** opens a row; **Functions and constants** targets the last focused row.
Search names/aliases, choose a category, or type directly. Insertion wraps
selected text; arguments use `;`. Clear affects only that row; Plot/Enter submits;
Escape closes. Color changes do not recalculate.

One free variable is inferred (`sin(t)` uses t). Multiple free names, calculator
variables/ans and random calls are unsupported. Angles use radians. For complex
expressions, plot an explicitly real component. Pan/zoom controls the range;
plotted coordinates are approximations.

## Coordinate inspection

Hover over a curve, or tap near it and drag the selected point horizontally.
Drag empty space to pan; a second finger starts pinch zoom. Readouts interpolate
displayed samples without new calculations and disappear in gaps/bands. Escape
or a camera gesture dismisses them.

### Intercept and intersection markers

Colored dots identify C-checked crossing/contact/intersection candidates.
Hover/tap for coordinates. Verification is asynchronous and bounded; crowded
plots may hide dots while retaining inspection. Finite checks can miss roots or
accept tiny residuals near a positive minimum: they do not prove existence,
uniqueness or completeness. [Verification contract](../reference/GRAPH_ANALYSIS_HTTP_API.md).

## Curves and gaps

Undefined, non-real or unresolved regions stay disconnected. Steps remain
separate plateaus. C accepts connections heuristically; finite sampling can
miss narrow features and poles. More points/digits alone do not prove accuracy.
See [screen sampling](../reference/SCREEN_GRAPH_HTTP_API.md) and
[precision fallback](../reference/ADAPTIVE_GRAPH_HTTP_API.md).

## Dense oscillations

Supported subpixel oscillations, such as `sin(1000x)`, show translucent approximate
range bands. They are not individual values or exact extrema; inspection skips
them. Zoom in to resolve waves. Unsupported shapes may remain unresolved.
[Supported envelope models](../reference/SCREEN_GRAPH_HTTP_API.md#optional-dense-oscillation-envelopes).

## Moving the view

| Action | Control |
| --- | --- |
| Zoom | Wheel, pinch or focused plot +/− |
| Pan | Drag or focused plot arrow keys |
| Reset to −10..10 | Reset view or Home |
| Center near selected function at x=0 | Go to function |
| Expand / return | Expand graph / Back or Escape |

Go to function leaves the camera unchanged if the value at zero is undefined or
non-real. Short mobile landscape view has a **Controls** toggle. Zoom stops
before coordinates lose usable pixel precision; reset/zoom out remain available.

## Prepared ranges

Cached geometry gives immediate movement previews; missing detail arrives after
the gesture. Old requests cannot replace newer views. Color/hide/show retain
caches; hidden rows do no sampling. Caches are bounded and tab-local. A
multi-function update can take longer than one request because each function
has its own budget. [Cache and scheduling policy](../reference/SCREEN_GRAPH_HTTP_API.md#browser-refinement).
