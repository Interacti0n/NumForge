# Smooth graph extrema and prepared zoom, 2026-10-13

Compare the Release MSVC server at `f2633bf` with internal forward derivatives,
stationary anchors and shape-preserving cubic drawing. Windows localhost HTTP;
one warm-up and five serial measured requests per expression. Elapsed time ends
after response bytes arrive, excluding JSON parsing. See [raw results](graph_smooth_zoom_2026_10_13.csv).
Transport/scheduler variation is substantial (the CSV includes min/max); these
are local diagnostics, not portable speed promises or isolated C arithmetic costs.

Request: `mode=fast&min=-30&max=30&ymin=-10&ymax=10&width=2048&height=440`;
the new server also receives `interpolation=hermite`. Visible x/y camera −10..10,
800×440 SVG with a 714×372 plot area. Both retain the same caps and shared budgets.

Drawing errors compare five fractional positions in each connected edge with
independent Python `math` references, restricted to visible x and reference y.
Before uses straight interpolation; after uses the browser's limited Hermite
representation when spacing exceeds one horizontal pixel, otherwise straight
lines. This samples error, not its continuous supremum or a certified bound.
Blank entries have no analytic drawing-error probe.

| Expression | Points before / after | HTTP median ms before / after | Sampled visible error px before / after | After limited |
| --- | ---: | ---: | ---: | --- |
| `sin(x)` | 277 / 277 | 14.69 / 2.075 | 0.126285 / 0.00144 | False |
| `x^3-3x` | 1709 / 1711 | 8.093 / 9.735 | 0.392877 / 0.0 | False |
| `exp(-(x-0.37)^2)` | 131 / 132 | 9.65 / 9.464 | 0.611323 / 0.042563 | True |
| `sin(3x)+cos(2x)` | 1101 / 1139 | 13.072 / 31.002 | 0.103325 / 0.019965 | False |
| `sin(100x)` | 6007 / 6007 | 30.197 / 37.435 | 3.540659 / 3.540659 | False |
| `exp(x)` | 1805 / 1805 | 7.786 / 9.16 | 0.464061 / 0.003644 | False |
| `1/(x-0.1)^2` | 2215 / 2215 | 27.382 / 11.594 | — / — | True |
| `sin(1000x)` | 8192 / 8192 | 125.166 / 136.579 | — / — | True |

Derivatives add modest work and response metadata, rather than making every
calculation faster. `sin(100x)` keeps its subpixel straight segments and unchanged
sampled error at this scale; slopes become useful as those segments enlarge.
`sin(1000x)` retains unresolved periods and gaps. The Gaussian's extreme buffer
tails remain limited, while the visible curve has no disconnected real edges;
the default −10..10 C accuracy test is fully resolved. The squared pole retains
domain checks; unresolved edges outside the y camera are not counted as visible.

The biggest measured benefit is shape accuracy for ordinary curves: sine and
exp errors shrink by roughly two orders of magnitude without increasing point
count. The polynomial gains two actual stationary anchors; the Gaussian gains
one. Added slopes increase response bytes, so tunnel latency still matters.

## Interaction and checks

The browser uses C slopes for monotone cubic controls, clips each accepted tile
and retains gaps. Missing/null slopes and subpixel spacing use linear fallback.
Stationary searches keep sign brackets, actual values, at most 24 probes, and
normal point/time/allocation limits. AD is internal drawing support, not a
symbolic differentiator or new installed numeric SDK operation.

After 180 ms idle, prepare the entire current x range for the next zoom level,
up to twice the width and half the y span. Active view data is retained ahead
of speculative data. Settled missing detail waits 90 ms, then coarse/refined
responses; old curves remain visible. An unprepared function/range or a busy
remote server still requires calculation.

Browser regressions check rendered SVG geometry with independent analytic
sin, shifted Gaussian and cubic references at widths 390 and 1280: sampled
visible deviation stays below 0.15 pixel in those default-camera checks.
A separate test waits for prepared next-zoom data, zooms once and verifies no
foreground request for that view. Its local event-to-animation-frame observation
was 10.2 ms; this observes callback timing, not physical display latency.
Twenty cached dense sin(33x) wheel frames retained the curve, with a median
interval 16.7 ms and maximum 33.4 ms. Some frames span two refresh
intervals; this is not a guarantee of 60 fps on every device or view.

C tests cover analytic derivatives, degree factors, unavailable slopes with
valid values, stationary roots of x³−3x and mixed functions, shared deadlines,
atomic outputs and existing allocation-failure checks. HTTP/browser tests cover
point-row compatibility, slope lengths, dense response bounds, linear/precision
fallback, malformed metadata, poles, hidden periods, stale work and cache limits.

## Reproduce

```powershell
python scripts/benchmark_graph_smooth.py --before path/to/old/numforge_web.exe --after path/to/new/numforge_web.exe --repeats 5 --output build/graph-smooth-bench.csv
```

The script starts private local servers on ports 19489/19490 and terminates only
its own children. Keep those ports free, compiler/build configuration and machine
consistent. `tests/browser/graph-smooth.spec.js` and the existing graph suites
exercise geometry, preparation and cancellation without hardware timing gates.
Further render work can target cached SVG transforms for dense camera motion;
second derivatives/error estimates remain separate future work.
