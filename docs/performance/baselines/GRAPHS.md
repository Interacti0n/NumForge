# Graph regression review — 16 October 2026

Snapshot: `3c049c7`, Windows MSVC Release.

## Browser coverage

The existing Chromium graph corpus had 203/209 passing cases initially. Six
stale camera/instrumentation expectations were corrected and passed in the final
selected run; all 209 scenarios and their continuity/cache assertions remain.

Eight new cases exercise four-function scenes at widths 390 and 1280:

| Scene | Expressions |
| --- | --- |
| Oscillations/corners | `sin(33x)`, `cos(17x)`, `abs(x*sin(x))`, `x*x/5` |
| Poles/domains | `1/(x-0.37)`, `sqrt(x-0.2)`, `ln(x+2)`, `1/ln(ln(sqrt(x)))^2` |
| Peaks/tangencies | shifted Gaussian, absolute cusp, squared contact, Lorentzian |
| Discrete/steps | `x!`, `ncr(x;3)`, `x`, `floor(x)` |

Each scene checks initial drawing, zoom plus horizontal/vertical movement,
widening plus reverse movement, and reset: 32 camera checkpoints per engine.
Tests wait past camera/marker debounce and graph-request quiescence, then require
a terminal marker state. Every displayed axis/pair marker is independently
compared with JavaScript references; every integer dot is checked at its actual
integer coordinate. Relative/absolute marker tolerance is 1e-7 scaled by ordinate.
This does not assert that all mathematical intersections have been discovered.

Other checks include finite SVG paths, bounded marker count, exact zero tick
labels, reset bounds, HTTP success and absence of uncaught page errors. Saved
screenshots permit visual review. This is desktop browser mobile emulation,
not testing on a physical Android/iOS phone.

Final Chromium selected run: 19 passed (six camera, five reconciliation and eight
new cases). Firefox/WebKit first ran 28 camera/new cases successfully; the final
26-case run covers corrected reconciliation and the strengthened new scenarios.
The combined unique coverage is 217 Chromium cases and 19 cases per other engine.

## Independent HTTP corpus

After browser tests stopped, the existing audit harness ran 52 cases three times
on a separate owned loopback server: **156 successful responses**, zero invalid
edges, zero accepted crossings of the listed poles and zero listed step-jump
crossings. All observed requests finished within 51.50 ms. No compilation or
other benchmark ran concurrently. These are local wall times including HTTP,
not public-tunnel times or a guarantee for arbitrary expressions.

The harness checks 8193 uniform/offset reference coordinates plus feature anchors
per case, reconstructing accepted interpolation or conservative dense bands.
Its independent numerical references use binary JavaScript math, with the usual
limitations near singularities and very large arguments. Finite grids are not
proofs that an arbitrarily narrow feature is absent.

| Case | Median HTTP ms | Maximum connected error px | Representation |
| --- | ---: | ---: | --- |
| `sin(x)` | 3.11 | 0.0015 | curve |
| `sin(33x)` | 10.59 | 0.238 | curve |
| Narrow Gaussian, width 0.005 | 2.82 | 0.503 | curve |
| `sinh(10x)` | 50.33 | 0.222 | precision curve |
| Nested logarithm | 28.37 | 0.0072 | curve, bounded gaps |
| `sin(1000x)+sin(1001x)` | 5.93 | — | 800 conservative bands |
| `sin(x*x)` on −100..100 | 5.11 | 0.151 | sampled center + 700 bands |
| `sin(x)` on −1000..1000 | 10.44 | 1.918 | curve |

The widest sine has the corpus maximum connected error, about 1.9171 px.
Cube root is about 1.53 px and square root about 1.30 px near their steep turns;
thus this is not a subpixel accuracy guarantee for every curve. Band-only error
is deliberately not reported as zero curve error. Those cases instead check
enclosure of independently evaluated values.

Six cases have sampled visible coverage just below 100%: fractional power,
`sin(1/x)`, floor, wide floor, ceil and `abs(sqrt(x))`. The lowest measured
coverage is 99.8837% for wide floor; discontinuities and unresolved boundaries
retain gaps. These results do not justify connecting unchecked intervals.

Full [52-row summary](../data/GRAPH_RELEASE_REGRESSION_HTTP_2026_10_16.csv). Raw responses
are disposable local output. This curve corpus
does not benchmark C marker verification or discrete modes; their timings are
in the later [refinement comparison](GRAPH_REFINEMENT.md).

## Conclusions and next work

No new evaluator/rendering defect was identified by these browser scenarios.
Finite adaptive sampling, dense-range approximations and limited numerical
intersection confirmation remain documented limits; a warning is not a promise
that every feature is present. Earlier 15 October failures are superseded historical findings; the corpus
above records the later tested behavior, with the stated remaining limits.

Request-local verification reuse and Brent refinement were implemented after
this snapshot; see [refinement comparisons](GRAPH_REFINEMENT.md). Current
sampling/connection/cache behavior is described by the
[screen graph contract](../../reference/SCREEN_GRAPH_HTTP_API.md). The older
point-cap, dense-envelope, phase, domain, narrow-feature and camera tuning logs
are superseded by those contracts and this checked corpus. Their original
measurements remain in Git history, rather than a parallel set of instructions.

This snapshot predates final release sign-off; later publication verification
is recorded in [release history](../../project/RELEASES.md). Physical-phone
touch review remains separate from browser emulation.

Reproduce the complete browser corpus from `tests/browser` with the built server
selected through `NUMFORGE_WEB_EXECUTABLE`:

```powershell
node node_modules/@playwright/test/cli.js test 'graph.*\.spec\.js$' fast-graph.spec.js
node node_modules/@playwright/test/cli.js test graph-release-regression.spec.js graph-camera.spec.js graph-reconcile.spec.js
node node_modules/@playwright/test/cli.js test graph-release-regression.spec.js graph-reconcile.spec.js --config playwright.cross-browser.config.js
```

From the repository root, the independent corpus can be rerun with:

```powershell
node benchmarks/application/benchmark_graph_audit.cjs --server build/server-3c049c7/numforge_web.exe --repeats 3 --output build/graph-release-audit-2026-10-16.json --csv build/graph-release-audit-2026-10-16.csv
```

Raw outputs and screenshots stay under ignored `build/graph-release-*2026-10-16`.
