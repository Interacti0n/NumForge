# Graph intercept and intersection checks — 2026-10-16

The graph marks approximate axis intercepts and pairwise intersections of its
accepted displayed strokes. These are screen geometry, not a new exact root API.
The Find function control separately uses an actual C sample at x = 0 to center
the selected visible expression; Reset view keeps the original camera contract.

## Rendering comparison

Windows, MSVC Release, local loopback server and headless Chromium. Baseline:
`7847cc6`; candidate: the intersection-marker change based on that commit.
Runs were serial, without simultaneous builds or browser suites. Default camera
and canvas; 25 color-triggered cached redraws per scene, first five discarded.

The browser diagnostic times synchronous redraw from removal of the old drawing
through the completed mutation batch. Animation-frame waiting is recorded
separately. These measurements concern warm geometry: camera/sample changes
invalidate the marker cache and require fresh geometry analysis. They do not
measure C evaluation, network latency, arbitrary zooming or worst-case inputs.

| Expressions | Baseline median / ms | Candidate median / ms | Candidate p95 / ms | Markers |
| --- | ---: | ---: | ---: | ---: |
| `sin(x)` | 1.0 | 1.0 | 1.3 | 8 |
| `sin(33*x)` | 1.6 | 2.4 | 2.8 | 128 |
| `x`, `2-x`, `sin(3*x)`, `cos(5*x)` | 1.7 | 2.9 | 3.1 | 99 |
| `abs(x*sin(x))`, `sin(33*x)`, `cos(17*x)`, `x*x` | 2.5 | 3.1 | 3.8 | 128 |

The highest individual candidate warm redraw was 5.4 ms. Median frame-inclusive
latencies were 17.7–19.9 ms. Keeping only the latest marker geometry avoids
re-solving unchanged intersections on color/status updates. Work remains bounded
by 65,536 edge/overlap visits and 128 visible markers; dense views can omit points.
Pointer inspection is frame-coalesced, reuses the markers and makes no requests.

Reproduce with `tests/browser/graph-features-performance.spec.js`, setting
`NUMFORGE_WEB_EXECUTABLE` to the selected built server. Each test attaches a JSON
diagnostic including expressions, draw/frame timings and marker count.

## Correctness and limits

- Independent analytic checks cover line intercepts, sine zeros, polynomial roots,
  shifted squared and absolute contacts, square-root boundaries, crossing lines,
  tangent pairs and sine/cosine intersections.
- `1/x` has no false zero at its pole. Positive minima, including an absolute
  minimum shifted upward by 1e-12, do not become zeros. Coincident stretches and
  dense bands do not receive isolated intersection markers.
- Mouse/touch snapping, expanded views, hidden/edited curves, localized labels,
  y-intercept lookup with inferred variables, missing real values and cancellation
  after input edits have browser regressions. Existing graph/layout/session and
  inspector checks also exercise the integration.
- A preexisting session test could reload before its preceding SPA navigation
  completed. Its helper now awaits the destination URL; the full seven-test file
  passed twice after this synchronization fix.
- C corner search previously overwrote an almost exact secant probe with a later,
  worse bisection probe. It now retains the best actually evaluated residual,
  using the same iteration/resource limits. Native tests check an absolute cusp
  position and height within 1e-14 and preserve its positive 1e-12 translation.
- MSVC and GCC graph/expression/web-API/web-UI/remote groups passed, including the
  existing allocation-failure and deadline checks. Browser feature coverage runs
  in Chromium, Firefox and WebKit; Chromium-only native multi-touch injection
  checks remain explicitly skipped in the other engines.

These tests do not prove all intersections are found. Markers inherit sampling
and interpolation error, and a represented contact may disappear at another
resolution. Domain gaps and unresolved ranges are deliberately excluded.
