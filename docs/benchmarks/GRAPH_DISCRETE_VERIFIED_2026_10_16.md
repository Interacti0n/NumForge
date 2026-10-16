# Discrete graphs and verified intersections — 16 October 2026

Local Windows Release measurements, MSVC, Chromium. These measurements concern
the implementation following `bde0f27`; they are not latency guarantees.

## Scope

Integer-domain graphs use actual C evaluations and isolated dots. Auto detects
direct integer arguments such as `x!` and `ncr(x;3)`. Explicit Points samples
integer x; arbitrary discrete domains and half-integer lattices are not inferred.
Sampling is bounded by screen width and at most 256 points per request.

Curve geometry supplies intersection candidates. C then evaluates the original
expressions, rejects unresolved candidates and refines eligible brackets.
Only confirmed markers are displayed. This is numerical verification, not a
symbolic proof or an enumeration of all roots. Selection is spatially distributed
and round-robin across axis/curve/pair groups, with at most 128 markers.

## Cold plot and verification

The browser harness measures from submitting the expressions until the plot and
marker verification settle. Requests include normal refinement/prefetch work.
Each scene below was measured once; timings are diagnostic, not percentiles.

| Scene | Settled ms | Plot POSTs | Verify POSTs | Longest HTTP ms | Markers | State |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `x!` | 83.9 | 1 | 0 | 4.5 | 1 | ready |
| `ncr(x;3)` | 77.0 | 1 | 0 | 8.9 | 0 | ready |
| `sin(33*x), cos(31*x), sin(17*x), cos(13*x)` | 1045.5 | 12 | 10 | 56.8 | 92 | limited |
| `1/ln(ln(sqrt(x)))^2` | 118.3 | 1 | 0 | 37.9 | 0 | ready |

The dense four-curve case reaches bounded partial verification. The warning is
intentional: unchecked candidates are omitted and the graph remains usable.
The 1.05 s settling time includes debounce, scheduling and multiple requests;
it is not a single calculation exceeding the request budget. Factorial renders
four visible dots; combinations renders three in the initial camera.

## Cached redraw comparison

Same machine, serial baseline/candidate runs. Each scene uses 5 warmups followed
by 20 measured redraws. The timer covers drawing work; frame time also includes
waiting for the browser. Marker counts differ because candidate geometry alone
is no longer sufficient to display a marker.

| Scene | Baseline median / p95 ms | New median / p95 ms | New max ms | New frame median ms |
| --- | ---: | ---: | ---: | ---: |
| `sin(x)` | 1.1 / 1.4 | 1.2 / 1.4 | 1.5 | 17.2 |
| `sin(33*x)` | 2.5 / 3.2 | 2.2 / 2.9 | 2.9 | 18.9 |
| `x, 2-x, sin(3*x), cos(5*x)` | 2.7 / 3.6 | 2.5 / 2.9 | 3.3 | 18.9 |
| `abs(x*sin(x)), sin(33*x), cos(17*x), x*x` | 3.5 / 4.6 | 2.8 / 3.4 | 3.4 | 17.9 |

Verification results are cached with the retained graph session. Color changes
and returning from the calculator reuse confirmed results. Camera/expression
changes invalidate affected geometry; stale responses cannot add old markers.

## Validation and conclusions

- Five native test groups passed with MSVC and GCC; GCC uses strict warnings.
- Chromium covered discrete graphs, marker verification, inspection, layout,
  multiple curves, retained sessions, smooth zoom, subpixel curves and guides.
  All five control tests passed after updating the old no-select expectation.
- Firefox/WebKit: 90 passed and four explicitly Chromium-only checks skipped.
- Additional Chromium checks cover verification cache reuse; all eight final
  cold/warm performance cases passed.
- Native regressions cover poles, floor jumps, shifted tangent roots, a positive
  nonzero minimum, mixed parameter names, allocation failures and cancellation.

The main remaining cost is cold verification of many oscillatory curves, not
cached drawing. A future optimization should share actual evaluations across
candidates in a batch, retaining cancellation and residual checks. Increasing
budgets or joining unverified geometry would weaken responsiveness or correctness.
Do not interpret a limited marker set as evidence that other intersections do
not exist. See the [protocol and numerical limits](../reference/GRAPH_ANALYSIS_HTTP_API.md).

Reproduce with `graph-analysis-performance.spec.js` and
`graph-features-performance.spec.js` in `tests/browser`. Raw local output lives
under ignored `build/graph-analysis-performance-final` and
`build/graph-analysis-baseline-performance`.
