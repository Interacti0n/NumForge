# Restricted dense-oscillation envelopes — 2026-10-14

The browser now displays supported dense single sin/cos carriers as a colored
translucent range, then refines back to ordinary accepted strokes on zoom.
For `sin(1000x)` over −10..10 at width 800, measured loopback HTTP time drops
from 141.1 to 5.6 ms and JSON from 427.0 to 60.0 KiB. This is an approximate
pixel-range representation, not a claim to draw or certify every hidden period.

## C/application and HTTP contract

The existing C parser prepares the expression once. The private bytecode model
recognizes `(a*x+b)*sin/cos(k*x+c)+d*x+e` with a single nonzero carrier amplitude,
constant/affine coefficients, and nonzero constant division. It never rewrites
input text or evaluates functions in production JavaScript. Multiple carriers,
variable denominators, tangent, nonlinear phase/amplitude and domain-sensitive
outer operations keep their existing bounded point/precision paths.

`dense=band` is opt-in on the existing screen endpoint, requires `mode=fast` and
both y bounds, and returns optional schema 3 `dense_representation` / `bands`.
Legacy queries and schema 2 fallback keep their original results. Each column
needs a full affine phase turn and endpoints within the existing ±1e6-radian fast
range. Amplitude/trend endpoint bounds produce the approximate ordinate range.
Variable amplitude/trend may overestimate actual attained values; floating
coefficients and outward endpoint nudges are not certified interval arithmetic.
See the [HTTP contract](../reference/SCREEN_GRAPH_HTTP_API.md#optional-dense-oscillation-envelopes).

Band-covered refinement intervals need no extra midpoints. They never become
accepted point chords. Complete default dense examples retain 33 seed points,
all point connection flags false, plus at most the requested width in bands.
Other intervals preserve normal refinement, domain/pole/phase guards and limits.
The single five-second / 64 MiB cumulative / 512 KiB single-allocation and JSON
budgets remain. A fully completed envelope needs no unused 300 ms midpoint
reserve; deadline/resource failures still leave outputs unchanged.

The web requests bands automatically, uses the function color, and identifies
ranges separately from unresolved detail. Zoom refines them into strokes;
band cache hits require at least the current horizontal density. A subpixel
height paints a clipped one-pixel trace without changing numeric bounds.
Hidden/off-screen bands do not count as visible functions; inspection does not
invent one y value for an entire range. Existing color, visibility, legend,
expanded canvas, pan/zoom and per-function cache behavior remain.

Cache bounds now count point-or-band records: 12 ranges, 12288 records and
estimated 8 MiB per function. Weight includes twice JSON length, 256 bytes per
point and 128 bytes per band. This is conservative accounting, not heap usage.

## Isolated HTTP comparison

Before: MSVC Release server from `80df461`. After: final envelope candidate,
same build/compiler. Eleven cases with three repeats in each version give
66 loopback requests. Timing includes evaluation, serialization, transfer and
text receipt; it is not isolated arithmetic or remote-tunnel latency. Envelope
values are checked against 10,773 independent JavaScript sin/cos evaluations.

| Case | HTTP ms before → after | JSON KiB before → after | Points before → after | New bands |
|---|---:|---:|---:|---:|
| `sin(1000x)`, width 800 | 141.147 → 5.615 | 427.0 → 60.0 | 8192 → 33 | 800 |
| `sin(1000x)`, width 320 | 139.865 → 3.976 | 427.0 → 18.6 | 8192 → 33 | 320 |
| `2-3*cos(20000x+0.3)` | 156.010 → 6.311 | 411.3 → 60.0 | 8192 → 33 | 800 |
| `x*sin(1000x)+2` | 141.678 → 5.566 | 418.1 → 59.7 | 8192 → 33 | 800 |
| `sin(1000x)+x` | 139.902 → 5.259 | 419.3 → 59.9 | 8192 → 33 | 800 |
| `sin(33x)`, −1000..1000 | 141.084 → 5.091 | 426.3 → 42.2 | 8192 → 33 | 800 |

All six supported cases change from limited, disconnected dense sampling to
complete envelope coverage. Ordinary `sin(33x)` remains 2259 points / 115005
bytes (14.174 → 14.050 ms); resolved `sin(1000x)` over −0.01..0.01 remains 135
points / 8424 bytes (3.167 → 3.357 ms). Tangent, variable-denominator and
multiple-carrier controls retain the same point counts, limited flags and JSON
byte sizes. Their timing variation is not evidence of an algorithmic improvement.

Raw local data: `build/graph-dense-http-2026-10-14.json`.
Tracked medians: [HTTP CSV](graph_dense_http_2026_10_14.csv).

## Four-function browser comparison

The existing benchmark now counts stroke and band layers together and records
visible dense ranges separately from unresolved warnings. Three fresh-page
repeats per version/layout/case give 24 runs in headless Chromium at 1280×900
and 390×844 on this PC. The mobile viewport is not physical mobile hardware.
It plots, expands, drags, sends six wheel zoom events, then checks a candidate
warm visibility toggle. Settling includes 350 ms quiet; interaction timings
include automation overhead. This final benchmark followed the browser suite.

Mixed: `sin(33x)`, `cos(17x)`, `exp(-x*x/4)`, `x^3-3x`.
Dense/poles: `sin(1000x)`, `tan(3x)`, `1/(x-0.1)^2`,
`sqrt(abs(x))*sin(33x)`.

| Layout / case | First frame ms before → after | Initial shown functions before → after | Initial dense ranges before → after | Initial unresolved before → after | Initial KiB before → after |
|---|---:|---:|---:|---:|---:|
| desktop / mixed | 129.1 → 124.3 | 4 → 4 | 0 → 0 | 0 → 0 | 1146.3 → 1146.3 |
| desktop / dense-and-poles | 414.8 → 222.8 | 3 → 4 | 0 → 1 | 2 → 1 | 1501.4 → 1385.8 |
| mobile-layout / mixed | 96.0 → 97.2 | 4 → 4 | 0 → 0 | 0 → 0 | 705.1 → 705.1 |
| mobile-layout / dense-and-poles | 345.2 → 212.5 | 3 → 4 | 0 → 1 | 2 → 1 | 1483.8 → 1165.8 |

The dense four-function first frame improves about 46% on desktop and 38% in the
mobile layout. Cumulative initial requests rise from five to seven: the resolved
dense carrier now prepares two optional ranges instead of stopping at its cap.
Its smaller responses still reduce cumulative initial bytes by about 8% / 21%.
Dense total bytes across plot/expansion/pan/zoom fall from 4929.2 to 4129.2 KiB
on desktop and 4381.2 to 3231.7 KiB on mobile. More initial background requests
are a deliberate cache preparation cost, not eliminated work.

Candidate frame p95 remains about 16.7–16.8 ms. Pan input-to-frame p95 is
17.4–31.6 ms, zoom 27.1–31.5 ms, with zero ≥50 ms long tasks. Desktop dense pan
input p95 increases from 21.9 to 31.6 ms in these runs: drawing a real band has
some client cost even though evaluation/transfer improve. Candidate visibility
toggles make zero graph requests. The uncontrolled whole-page heap snapshot
(without forced GC, candidate including an extra toggle) is not a leak or
retained-cache measurement; medians span 12.9–31.3 MiB before and 14.7–26.7 MiB
after. Longer interaction/memory sessions remain a separate stage.

Mixed mobile pan varied between measurement runs: the final main comparison
shows zero versus ten requests, while an additional one-repeat diagnostic with
camera/request logging gives identical −12.580479844674556..7.419520155325445
bounds at 336×554 and ten requests / 580176 bytes in both versions. Do not infer
a universal ordinary-pan traffic change from that phase. The diagnostic records
request ranges and camera sizes for future comparisons; it does not replace the
main three-repeat timing table. Raw files are local under `build/`.

Main raw: `build/graph-dense-browser-2026-10-14.json`.
Camera diagnostic: `build/graph-dense-camera-diagnostic-2026-10-14.json`.
Tracked medians: [browser CSV](graph_dense_browser_2026_10_14.csv).

## Verification and next work

- 115 graph/guide browser tests passed; final dense/priority regressions were also
  rerun after the short-budget reserve check. New tests cover values, RAD/DEG,
  legacy behavior, precision fallback, zoom, locales/layouts, colors, visibility,
  no invented coordinates, subpixel visibility and malformed metadata.
- MSVC and GCC warnings-as-errors builds and graph/application-expression/session/
  application/web-API checks passed. Graph tests include every allocation failure,
  zero/short inherited deadlines, output atomicity and excluded domains.
- Pole, domain, seam, stationary-extremum, inspector, touch, cache and guide checks
  remain green. SDK numerical headers and calculator arithmetic are unchanged.

This is the first restricted envelope stage. Multiple carriers and nonlinear
phase/amplitude/domain models need separate designs and independent references;
`sin(1/x)` cannot have all oscillations finitely resolved near zero. Next:
longer pan/zoom/memory diagnostics, physical-phone checks, then clearer graph
messages and saved/exported configurations according to the project roadmap.

```powershell
node scripts/benchmark_graph_dense.cjs --before build/server-80df461/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-dense-http-2026-10-14.json
node scripts/benchmark_graph_multi.cjs --before build/server-80df461/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-dense-browser-2026-10-14.json
```
