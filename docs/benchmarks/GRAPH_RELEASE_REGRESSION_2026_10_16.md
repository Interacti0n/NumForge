# Graph regression review — 16 October 2026

Snapshot under test: `3c049c7`, Windows MSVC Release executable. This follow-up
adds tests and documentation; it does not alter the evaluator, sampling, browser
behavior, release version or deployed executable.

## Browser coverage

The complete existing Chromium graph corpus ran 209 cases. Initially 203 passed
and six failed due to stale test expectations/instrumentation:

- One camera test still expected reload to discard the saved camera. The
  corrected test checks both retained same-tab state and origin defaults in a
  fresh browser context, without dropping either assertion.
- Five reconciliation checks could not install their diagnostic hook because
  their exact source anchor predated retained-session cleanup. The hook now
  inserts before the existing save operation. Plateau continuity, cache bounds,
  cancellation, failed repair and terminal-state assertions remain intact.

The corrected six checks passed in the final selected run. Together with the
unchanged passing checks, all 209 existing Chromium cases are accounted for.

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

Full [52-row summary](GRAPH_RELEASE_REGRESSION_HTTP_2026_10_16.csv); raw responses
remain in ignored `build/graph-release-audit-2026-10-16.json`. This curve corpus
does not benchmark C marker verification or discrete modes; their timings are
in the [discrete/verified report](GRAPH_DISCRETE_VERIFIED_2026_10_16.md).

## Conclusions and next work

No new evaluator/rendering defect was identified by these browser scenarios.
Finite adaptive sampling, dense-range approximations and limited numerical
intersection confirmation remain documented limits; a warning is not a promise
that every feature is present. The historical
[15 October findings](GRAPH_RELEASE_AUDIT_2026_10_15.md) must be read with their
linked fixes, rather than treated as current failures.

The next performance target is bounded reuse of actual verification samples
within a C batch, preserving rational subtraction, cancellation, residual checks
and limits. Measure hit rate and confirmed markers before increasing any budget.
On narrow mobile views, many markers can visually crowd oscillating curves;
consider a screen-dependent marker density or showing more only for the active
function. That would be a separate UX change.

Physical-phone touch review and CI remain before final release sign-off.
No version bump or release publication is part of this review.

Reproduce the complete browser corpus from `tests/browser` with the built server
selected through `NUMFORGE_WEB_EXECUTABLE`:

```powershell
node node_modules/@playwright/test/cli.js test 'graph.*\.spec\.js$' fast-graph.spec.js
node node_modules/@playwright/test/cli.js test graph-release-regression.spec.js graph-camera.spec.js graph-reconcile.spec.js
node node_modules/@playwright/test/cli.js test graph-release-regression.spec.js graph-reconcile.spec.js --config playwright.cross-browser.config.js
```

From the repository root, the independent corpus can be rerun with:

```powershell
node scripts/benchmark_graph_audit.cjs --server build/server-3c049c7/numforge_web.exe --repeats 3 --output build/graph-release-audit-2026-10-16.json --csv build/graph-release-audit-2026-10-16.csv
```

Raw outputs and screenshots stay under ignored `build/graph-release-*2026-10-16`.
