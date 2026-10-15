# Discrete graph guards — 15 October 2026

The release audit found 192 accepted connections crossing integer jumps for
`floor(x)` on x −100..100. A straight diagonal could pass the midpoint error
test because different integer levels happened to align. Sample values were
correct, but the displayed interpolation was not (up to 17.44 plot pixels).

## Implementation

The prepared C expression's existing graph metadata now captures the evaluated
result of every floor/ceil/trunc/round call as well as forward trig arguments.
Step results must compare exactly equal at both endpoints and the midpoint
before either chord can connect. Calls inside compositions retain their own
guards, independently of final output amplitude or the vertical tolerance.
Round's optional precision argument uses the same ordinary C evaluator.

The combined metadata cap remains 64 call sites. Overflow or unsupported
metadata remains explicitly unknown and cannot authorize a connection. Owned
BigDecimal captures share existing allocation/time limits and cleanup. This
adds no HTTP fields and changes no calculator rounding, uniform sampling,
fast bytecode support, public SDK or per-operation resource limits.

Changed steps forbid connections but do not block the existing camera deferral
when trig metadata is safe. Small ordered triples entirely beyond a camera
edge remain disconnected and set screen_clipped. This avoids using all points
on invisible steps while visible plateaus are still unfinished. Revealing a
deferred y region uses the existing cache resampling contract.

This is conservative sampled geometry. Cancelling discrete calls may still
create gaps. Nonmonotone arguments can hide an excursion between all sampled
values; no completeness or global continuity proof is claimed. Precision
sampling still has a 2048-point/depth-12 limit. Dense steps can remain limited
and incomplete; there is no shortcut that draws diagonals to hide those gaps.

## Regressions and method

Browser API regressions cover floor, ceil, trunc and round at default/explicit
precision, negative/shifted/scaled/reversed inputs, a continuous trend, tiny
amplitude, multiple steps, a periodic argument, and legacy/screen sampling.
Independent interior probes inspect every accepted chord. Camera regressions
require over 98% coverage of visible shifted plateaus at both canvas widths,
with correct values and fewer than 2048 points. Native tests check captured
step ownership, all allocation failures of a mixed metadata sample, metadata
overflow, 20 stratified graph allocation failures and inherited interruption
while preserving the original output pointer.

The timing comparison uses GCC RelWithDebInfo snapshots on the same PC:

- Before: `441e614`, executable SHA256
  `4d1aa4afb9a3cc30c2f6c9ed925991ddba16e6c7b9e304fd88b7c723ec9bdba3`.
- Corrected executable SHA256:
  `f32b1abe2e55e8ec9a37763cdfbf72f343bc9189b46de6676c32db329cefcbce`.

Run timing suites serially after compilation/tests finish:

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
node scripts/benchmark_graph_audit.cjs --before build/server-441e614-gcc/numforge_web.exe --server build/release-gcc-ninja22/numforge_web.exe --suite steps --repeats 3 --output build/graph-step-guards-2026-10-15.json --csv build/graph-step-guards.csv
node scripts/benchmark_graph_audit.cjs --before build/server-441e614-gcc/numforge_web.exe --server build/release-gcc-ninja22/numforge_web.exe --repeats 3 --output build/graph-step-controls-2026-10-15.json --csv build/graph-step-controls.csv
```

The step suite contains 16 primitive/camera profiles, nine composed/nonlinear/
decimal/sparse cases and six controls: 31 cases / 186 requests. The unchanged
52-case controls add 312 requests, for 498 total across both versions. Three
repetitions provide median/min/max loopback HTTP wall times including response
serialization and transfer. References inspect 4097 x coordinates and known
jump boundaries. Step boundaries can specify left/right closure, including
ceil, negative trunc and half-even round ties. Diagnostic errors describe
covered visible points; always read coverage alongside them. Raw payloads and
actual-SVG images stay under ignored build/.

Validation on the corrected source: all 139 graph/guide Playwright tests pass,
seven selected MSVC CTest targets pass and five GCC CTest targets pass. The
metadata and graph fault tests also verify cleanup and atomic output behavior.

## Measurements and conclusions

Committed summaries: [31-case step suite](graph_step_guards_2026_10_15.csv) and
[52-case controls](graph_step_controls_2026_10_15.csv). Read render error as
conditional on accepted coverage, not a claim that every interval is drawn.

| Step-suite case | Before → current HTTP median ms | Before → current known jump crossings | Before → current render error, pixels | Current points |
| --- | ---: | ---: | ---: | ---: |
| floor(x), x ±100 | 5.64 → 24.10 | 192 → 0 | 17.438 → 0 | 639 |
| floor(3*x+0.37), x ±100 | 10.53 → 37.47 | 484 → 0 | 16.856 → 0 | 591 |
| floor(x)+x/10 | 7.99 → 33.65 | 192 → 0 | 17.438 → 0 | 603 |
| round(x;1), x ±2 | 23.18 → 48.14 | 16 → 0 | 1.831 → 0 | 929 |
| floor(sin(3*x)), x ±10 | 90.48 → 155.10 | not enumerated | 0 → 0 | 599 |
| floor(x/100), x ±10000 | 6.88 → 32.54 | 192 → 0 | 17.438 → 0 | 639 |

All 31 cases succeed with zero accepted enumerated jump crossings and zero
invalid edges. The primitive/shifted/trend/sparse wide cases above retain full
sampled visible coverage. Across all 25 step cases the minimum coverage is
0.99170, in the tiny-amplitude case which reaches the existing 2048-point cap.
Other step cases can still report limited depth near their discontinuities.
Render error on covered step coordinates is zero apart from floating reference
noise below 3e-22 pixels. The periodic case retains trig phase guards; its
boundaries/isolated values are not exhaustively enumerated by this diagnostic.

The fix intentionally costs more work than accepting incorrect diagonals.
Separating step acceptance from safe off-screen deferral reduced the first
correctness-only prototype from 2048 points / about 131 ms / 52% sampled visible
coverage on the shifted case to 591 points / 37 ms / full sampled coverage.
Those prototype figures are exploratory, not the committed final comparison.

The 52-case controls show no new rejected case, invalid edge, accepted known
pole crossing, coverage loss or interpolation error increase above 0.1 pixel.
Non-step geometry remains unchanged in the measured corpus; small time
differences on those cases are noise. The independent default floor-wide case
changes 5.93 → 23.55 ms and 17.438 → 0 pixels, confirming the step-suite result.
All 498 timing requests complete within their existing operation scopes.

Actual Chromium SVG shows 464 horizontal segments / 20 subpaths initially and
3152 segments / 140 subpaths after three zoom-outs, with **zero nonhorizontal
step segments** in both views. Multi-function, expansion/Escape, keyboard pan,
reset and syntax recovery smoke checks pass. The wide zoomed view still shows
the pre-existing `Sampling…` status and a missing central cached region after
transport settles. This is a separate remaining client lifecycle/cache defect;
the SVG diagnostic records it explicitly and does not certify full coverage.

Next: repair final pending-view/cache reconciliation, then revisit dense mixed
expressions and cross-engine/device release checks. This is not release sign-off.

Follow-up: [view reconciliation](GRAPH_VIEW_RECONCILIATION_2026_10_15.md)
addresses the separately recorded client loading/central-cache defect.
