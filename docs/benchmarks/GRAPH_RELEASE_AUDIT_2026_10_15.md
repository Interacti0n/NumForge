# Graph release audit — 15 October 2026

**Decision: useful and responsive, but not ready to sign off as a reliable graph
release yet.** Existing regressions pass; the wider independent corpus exposes
incorrect interpolation, missed peaks, false jump connections and an avoidable
whole-plot error. Fix those before adding more mathematical graph features.

Follow-up: the [precision camera fix](GRAPH_PRECISION_CAMERA_2026_10_15.md)
addresses the first finding. The subsequent [narrow-feature fix](GRAPH_NARROW_FEATURES_2026_10_15.md)
addresses the missed fast Lorentzian peak. The results below describe the audited
`8e004db` build. The [exponential tail fix](GRAPH_EXP_TAILS_2026_10_15.md)
subsequently addresses the default-screen Gaussian scale failure. Other release
findings remain open. The subsequent [step guards](GRAPH_STEP_GUARDS_2026_10_15.md)
address observed floor/ceil/trunc/round jump connections while retaining
bounded refinement.
The [view reconciliation fix](GRAPH_VIEW_RECONCILIATION_2026_10_15.md) subsequently
addresses the stuck loading state and missing central cache tile after zooming.
The [dense-sum follow-up](GRAPH_DENSE_SUMS_2026_10_15.md) adds bounded affine
sin/cos sums; nonlinear phases such as the wide chirp remain separate work.

This audit changes diagnostic scripts and documentation only. It does not change
the evaluator, sampler, web UI, version or deployed server.

## Builds, environment and method

- Current: `8e004db`, previous: `cb619e3`, MSVC Release executable snapshots.
  Current SHA256: `a40c4ee9adcfc84b8c57051ca32bfad1c0582f52b18007a75f72066f45fe0d9b`;
  previous: `71264cd913f633b20c33ab7958b7f3fcdb06e2158795c8e1fc0d1d9ed4d158bd`.
- Windows 11 Pro 10.0.26300, Ryzen 7 7435HS, 8 cores / 16 logical processors,
  approximately 31.8 GiB visible RAM; Node 24.19.0, Playwright 1.63.0.
- Each benchmark owns a separate loopback server and stops only that process.
  Tests, timing suites, compilation and browser timing do not run simultaneously.
  Results are local HTTP wall times, not arithmetic-only or public-tunnel times.
- HTTP matrix: **52 cases × 3 repetitions × 2 builds = 312 requests**. Default
  camera x/y −10..10, width 800, height 440, Hermite interpolation and dense bands.
  Exceptions include wide/shifted/tiny cameras and deep zoom. All cases and camera
  bounds are defined in [the harness](../../scripts/benchmark_graph_audit.cjs).
- Independently inspect 4097 uniformly spaced reference coordinates per case,
  with explicit anchors for narrow peaks. Compare visible values and the
  browser's limited-Hermite/linear interpolation using its 714×372 plot area.
  Count missing connections, accepted pole/jump crossings and band coverage.
  Endpoints count when attached to an accepted edge. An isolated valid sample is
  not counted as a visible curve.
- This is a sampled corpus, not a continuity proof or a bound on all functions.
  Coverage alone is insufficient: a wrong chord can cover every reference x.
  Error applies only where a connected curve exists; a missing curve must not
  be described as zero error. Dense bands are assessed as ranges, not exact y.
- Python standard-library Decimal references at 100 and 140 digits agree within
  1e−90 scaled error on seven selected cases. The 407 visible-point checks show
  at most approximately 1.285e−14 scaled point error. In steep sinh/cosh cases
  there is only one point inside the camera; the problem is the long adjacent
  chord, not that point. Independent Decimal values also check the coordinates
  with the largest reported interpolation error.
- Actual SVG `getPointAtLength` checks and screenshots reproduce selected errors
  after foreground completion and after background work settles. Minor differences
  from the HTTP interpolant reflect SVG path-length/projection precision.
  These diagnostic tools record known failures; exit zero is not a release pass.

Full HTTP results: [104-row CSV](graph_release_audit_http_2026_10_15.csv).
The [previous domain report](GRAPH_DOMAIN_VIEWS_2026_10_15.md) remains applicable.

## Correctness findings and proposed fixes

| Priority | Reproduction and observed result | Proposed implementation and regression |
| --- | --- | --- |
| P1 | `sinh(10x)` and `cosh(10x)`, default camera: connected drawing differs from the reference by **67.8 / 72.7 pixels**. At x=−0.1953125, sinh should be −3.4544282 but the HTTP chord gives −7.0987369. Actual SVG confirms about 67.75 px error, persisting after background work. `limited:false` gives no warning. | Give precision fallback the actual camera and a screen-space error criterion. Its current tolerance comes from seed-wide y extrema, including huge invisible values. Refine visible steep branches and defer invisible work under the same guards. Adding supported sinh/cosh/tanh bytecodes with derivatives can improve speed, but cannot replace a correct fallback. Assert geometry between samples, not only computed sample values. |
| P1 | `1/(1+((x-0.1234)/0.04)^2)`: true maximum 1 at x=0.1234, displayed value approximately **0.089857**; **16.93 px** error, 65 points, `limited:false`. The peak is missed in both builds and in settled SVG. | Strengthen initial feature detection with bounded extra probes and derivative/curvature information. Prioritize suspect intervals by visible error. Revalidate/refine edges around inserted stationary anchors. Add shifted narrow peaks of several widths/positions to prevent grid-alignment luck; increasing a global cap alone does not find an unobserved feature. |
| P1 | `exp(-((x-0.1234)/0.005)^2)`: whole plot fails with **value too large**, although the real function is finite everywhere and lies in (0,1]. Width 0.04 succeeds through precision fallback but takes about **438 ms** and still has **2.89 px** interpolation error. | Handle exp tail underflow/scale limits in the screen evaluator without aborting an otherwise valid curve. Keep numeric uncertainty and mathematical domain errors distinct. Prefer a bounded screen approximation with an error bound, or locally unresolved samples, over an unbounded exact tail computation. Do not globally redefine tiny exact values as mathematical zero. Test foreground, buffered ranges, zoom back into the peak, shared budgets and calculator semantics. |
| P1 | `floor(x)`, HTTP x −100..100 / y −10..10: **192 accepted edges cross integer jumps**, with up to **17.44 px** visible error. Values at sampled points are correct. Ordinary x −10..10 does not expose this aliasing. | Carry jump/discontinuity metadata through floor/ceil/round and supported compositions. Split at known boundaries, never accept a chord solely because endpoint/midpoint integer values happen to align. Cover shifted/scaled steps, negative inputs, poles and cache seams in regression tests. The 192 count includes crossings outside the y camera; the visible-error figure is measured inside it. |
| P2 | `sin(1000x)+sin(1001x)`: 8192 points and about **435 kB**, yet no accepted visible curve in the reference grid. `sin(x*x)` on −100..100 uses about **438 kB**, covers only **30.7%** of reference x values and reaches 4.16 px error on connected portions. Both warn about unresolved detail. | Extend conservative dense ranges to selected composed expressions and nonlinear phases, with domain/phase guards and explicit approximate-range notices. Otherwise retain truthful gaps and improve budget prioritization. This is a documented supported-scope limitation, not permission to connect arbitrary missing points. |
| P2 | After plotting `floor(x)` and pressing − three times, x/y become −80..80; **Sampling… remains after transport settles and aria-busy is absent**. Reproduced in Chromium, Chrome and Edge. | Reconcile final pending-view/message state after the last job, including precision fallback/coarse tiles and failed refinement. End in ready, limited, numeric or error state; test settled messages as well as visible paths. |

No accepted connection crosses the explicitly listed poles in the tested cases,
including tan, paired poles, reciprocal powers and the nested logarithm.
`abs(sqrt(x))`, `re(sqrt(x))` and `im(sqrt(x))` retain their expected real-valued
branches in the precision path. Tiny/large coordinates, zoomed sine extrema and
zero labels are covered by the corpus and existing browser regressions.

The narrow-feature and fallback defects predate `8e004db`; the before/after
comparison does not attribute them to the most recent domain optimization.

## Performance: useful improvements and remaining costs

Medians of three complete loopback requests, previous → current:

| Case | HTTP ms | Points / representation now | Current JSON bytes |
| --- | ---: | ---: | ---: |
| sin(x) | 3.83 → 3.03 | 135 | 6714 |
| sin(33x) | 14.24 → 14.94 | 2259 | 115005 |
| sin(1000x) | 4.61 → 5.40 | 33 + 800 bands | 61459 |
| 1/ln(ln(sqrt(x)))², default | 354.01 → 42.37 | 183 | 8201 |
| Same, x 1.001..20 / y −10..100 | 85.32 → 4.37 | 151 | 8955 |
| Narrow Gaussian, width 0.04 | 441.35 → 438.29 | 79, precision | 8341 |
| Two dense sine carriers | 157.06 → 158.51 | 8192, unresolved | 435002 |
| Wide quadratic-phase sine | 123.61 → 124.80 | 8192, partly unresolved | 438485 |

The slowest current request observed in this matrix was 447.08 ms. This does not
prove arbitrary expressions fit in five seconds: the budget belongs to each
request, while queue wait and network transfer add time. A fast response with an
incorrect or missing curve is not a successful optimization. Small timing changes
in unchanged controls are noise; do not tune thresholds from these differences.

Four-function browser comparison uses desktop 1280×900 and mobile layout 390×844,
two workloads, three repetitions per build: **24 runs**. Foreground frame means
the frame after all four foreground jobs finish, not time to the first curve.

| Layout / workload | Foreground frame median ms before → current |
| --- | ---: |
| Desktop, mixed | 118.2 → 123.4 |
| Desktop, dense/poles | 230.1 → 137.9 |
| Mobile layout, mixed | 96.3 → 113.4 |
| Mobile layout, dense/poles | 211.8 → 130.0 |

All four curves render, no page errors occur, and warm visibility toggles make
zero graph requests. Current zoom input-to-frame p95 medians are approximately
29–32 ms; maximum frame intervals are 33.3 ms. No ≥50 ms long tasks were recorded
in these windows. This is headless local testing, not physical-phone/WAN latency.
See [the phase CSV](graph_release_audit_browser_2026_10_15.csv).

## Sustained interactions, cache and memory

Fresh current-build run: three layouts × two four-curve workloads × 24 cycles
= **144 cycles**, 864 inverse-wheel events, 36 deliberately held/aborted requests,
24 native emulated touch pinches, and 36 graph↔Units navigation round trips.
See [the six-case CSV](graph_release_audit_stress_2026_10_15.csv).

| Layout / case | Requests through cycle 24 | Peak JS heap MiB | Peak cache records / function |
| --- | ---: | ---: | ---: |
| Desktop mixed | 135 | 7.262 | 11982 |
| Desktop poles | 119 | 6.407 | 11976 |
| Portrait mixed | 146 | 7.081 | 12255 |
| Portrait poles | 182 | 6.598 | 11933 |
| Landscape mixed | 202 | 7.447 | 12227 |
| Landscape poles | 190 | 6.815 | 12227 |

All assertions pass: no stale graph/page errors, settled queues, no recalculation
for warm toggles, bounded cache ranges/records/estimated bytes and collected old
graph roots after navigation. Cache reaches 12255/12288 records and approximately
4.250/8 MiB estimated weight per function. This accounting weight differs from
actual browser heap. Heap is whole-page JS after forced GC, peaking at 7.447 MiB;
it excludes native SVG storage and the C process. No ≥50 ms long tasks; measured
frame p95 approximately 16.7–16.8 ms, max 33.4 ms. Windows include idle frames and
exclude diagnostic/GC pauses, so p95 is not an isolated input-latency guarantee.
These finite runs reveal no retained old graph roots; they are not a general
proof that no memory leak exists. Prior 16-cycle data is in the
[stability report](GRAPH_STABILITY_2026_10_14.md); counts differ in workload length.

## Resource rejection and queue behavior

An additional bounded workload, `sum(atan(x); …)` with 120 terms, returns
`value too large` in a median **273.90 ms** over three runs. The API does not
identify which internal size/resource condition caused this status; do not call
it a time-limit result. A simple graph sent 100 ms later waits **164.42 ms**,
versus an idle median **5.07 ms**. Following recovery calls take 2.57–3.70 ms.
Aborting the client after approximately 100 ms still leaves the next graph
waiting approximately 153 ms. The server remains usable, but client abort is
not immediate server-side preemption.

Existing native tests check shared deadlines, allocation failures and atomic
failure outputs. This audit did not create a real five-second timeout; it must
not be reported as measuring every possible worst-case deadline overshoot.
For a multi-user service, bound queue wait and add cooperative cancellation or
isolated workers with safe session/budget ownership. Four serialized five-second
jobs can take much longer than five seconds as a group. This hosting concern
should not be confused with a slow SVG redraw.

## Validation and release gates

- Existing Chromium graph/guide suite: **125/125 passed**.
- MSVC Release: **7/7** selected graph/application/expression/session/web API/
  remote/UI asset executables passed. GCC: **5/5** graph/application/expression/
  session/web API executables passed. These counts are executable suites, not
  individual assertions.
- Installed Chrome and Edge smoke checks: two functions, expansion/Escape,
  horizontal/vertical keyboard movement, reset, syntax error and recovery pass.
  Both reproduce the selected rendering defects. Chromium, Chrome and Edge all
  use Chromium; this is not three independent engine implementations.
- Firefox, WebKit/Safari and physical Android/iPhone were not available in this
  audit. Cross-engine and real-touch checks remain release gates.

Recommended order:

1. Add the P1 reproductions as correctness regressions, then fix camera-aware
   fallback, narrow features, exp-tail handling and jump metadata in separate
   tested changes. Inspect accepted edges and independent reference coordinates.
2. Resolve the stuck sampling state and clarify approximate versus unresolved
   results. Reconcile the guide's older blanket “vertical panning without a
   calculation” paragraph with the later clipped-region exception.
3. Agree the supported first-release scope: up to four explicit real y=f(x)
   curves, radians, finite screen/numeric limits and truthful gaps/bands. Choose
   whether wider dense compositions are required now or documented as later.
4. Repeat this corpus, browser suite, cache stress and phase measurements after
   fixes; require no silent large geometry errors or accepted known jump/pole
   crossings in the agreed corpus. A practical target is ≤1 px normal error and
   ≤2 px exceptional steep/endpoint detail, stated as a tested screen target,
   not a universal theorem. A missed visible narrow peak is a failure regardless
   of low average error or high coverage.
5. Complete Firefox/WebKit/physical-phone and realistic-latency checks, including
   offline/retry, orientation change, keyboard/focus, color contrast, SVG/inspector
   accessible naming and multiple-user queue behavior appropriate to hosting.
   Then run the full release CI on the final fixes and close changelog/version.

## Feature priorities after correctness

Already present: four adjustable colors, per-curve visibility/errors, common
library, empty axes, pan/zoom/pinch, keyboard controls, adaptive zero/ticks,
expanded graph, cached coordinate inspector, prepared ranges and bounded cache.

Useful next additions, without crowding the default view:

- Share/save graph definitions and camera; export SVG/PNG. Existing per-tab
  expression/color persistence is not a portable saved graph.
- Optional equal axis scale and independent axis scaling, plus fit-to-curves.
  Keep default −10..10 and the current simple controls; fitting near poles must
  use robust visible bounds rather than unbounded extrema.
- Computed roots, intersections and local extrema as an optional analysis tool,
  using C operations, budgets and approximate-result metadata. The current
  inspector interpolates cached geometry; it is not a root solver.
- Named parameters/sliders and subsequently piecewise, parametric and implicit
  curves. Specify domains and API contracts before implementing each new type.

These are expansion features, not prerequisites for a reliable first explicit
function plotter. Unrestricted arbitrary functions over arbitrary intervals,
symbolic differentiation, integrals and 3D are separate projects.

## Reproduction

From the repository root; substitute equivalent freshly built executables when
the ignored snapshot paths are unavailable:

```powershell
node scripts/benchmark_graph_audit.cjs --server build/server-8e004db/numforge_web.exe --before build/server-cb619e3/numforge_web.exe --repeats 3 --output build/graph-audit-2026-10-15.json --csv build/graph-audit-http.csv
python scripts/check_graph_audit_decimal.py build/graph-audit-2026-10-15.json --output build/graph-release-decimal-2026-10-15.json
node scripts/benchmark_graph_multi.cjs --before build/server-cb619e3/numforge_web.exe --after build/server-8e004db/numforge_web.exe --repeats 3 --output build/graph-release-multi-2026-10-15.json
node scripts/benchmark_graph_stress.cjs --server build/server-8e004db/numforge_web.exe --cycles 24 --output build/graph-release-stress-2026-10-15.json
node scripts/benchmark_graph_audit_browser.cjs --server build/server-8e004db/numforge_web.exe --channel chromium --output build/graph-release-svg-chromium-2026-10-15.json
node scripts/benchmark_graph_budget.cjs --server build/server-8e004db/numforge_web.exe --output build/graph-release-budget-2026-10-15.json
```

Run these suites serially. `--channel chrome` and `--channel msedge` use installed
browsers for the SVG/smoke diagnostic. Raw payloads, screenshots, Decimal and
browser telemetry stay under ignored `build/`; the small summaries and this
report are tracked. Ports 19505–19508 belong to the new diagnostics, which refuse
occupied ports. Production server/tunnel are untouched.
