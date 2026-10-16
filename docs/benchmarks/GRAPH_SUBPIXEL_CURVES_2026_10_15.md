# Subpixel graph curves — 15–16 October 2026

The mixed-range follow-up fixed wide views but left gaps between dense bands
and resolved curves. On ±2 with 800 requested columns,
`sin(x*x)+cos(1000x)` still exhausted 8192 points with zero checked curve
coverage. On ±1 both tested sizes nearly completed but still exhausted points.
Strict linear refinement and the browser's one-pixel cubic threshold spent work
on straight chords even where a checked smooth arc could render the curve.

## Implementation and compatibility

New explicit `interpolation=hermite-subpixel` requests can use controlled cubic
acceptance for the existing restricted quadratic sin/cos models with camera
bounds and `dense=band`. Each proposed pair of half-arcs uses the browser's
monotone tangent limiter. C compares their cubic midpoints with actual
quarter-point evaluations. Phase/domain guards remain; unavailable or uncertain
control samples defer acceptance rather than invent values.

Derivative brackets discover actual stationary anchors during refinement, with
bounded searches and original value/derivative evaluations. Each child is then
checked at its own midpoint. Redundant later phase/stationary searches are skipped
for results using checked cubic arcs. Existing discovery paths remain for
legacy and unsupported models. This preserves visible extrema while reducing
point use instead of increasing the 8192-point limit.

`subpixel_hermite:true` responses require slopes and cubic interpolation even
below a horizontal pixel. Drawing and coordinate inspection use the same rule;
the policy survives cached tiles. Missing/malformed required data is rejected.
If slopes cannot fit the 512-KiB response, the server resamples with the legacy
path within the same inherited operation budget. A high-resolution/scaled-input
regression exercises that bounded fallback. Existing `linear`/`hermite` requests
and explicit precision retain their original sampling policy.

The UI selects the new mode automatically. On narrow screens, these checked
curves use a thinner stroke to reduce merging of closely spaced waves. There
are no extra settings or controls. SDK arithmetic, five-second/allocation limits,
response/cache limits and domain/connection protections remain unchanged.

These checks are approximate screen error control, not certified bounds for
arbitrary functions or proof of continuity between all finite samples.

## Reproduction and method

Compare preserved GCC RelWithDebInfo `c383a75` with the candidate built from the
same configuration. Before SHA256:
`2090ec2ee883d8acdeabcf9ec63a09d9488ec8588c92c6735e48a76f62d731b6`.
Candidate SHA256:
`f286bcb2c5f930f0b3317447b72ba53c379186dba1fe4cb1e65b899daaa8d94d`.
Before requests use legacy `hermite`; candidate requests explicitly use
`hermite-subpixel`. The new `transition` suite has 43 unique cases: two viewport
sizes, seven two-axis zoom scales, phase offsets, scaled coordinates, signed
amplitude/multiple-carrier mixtures, fixed-y reproductions and six controls.

Independent references now include the original 4097 uniform positions **and**
4096 offset positions, plus feature anchors. The offset grid prevents nearly
completed dyadic samples from hiding between-sample interpolation errors.
Historical reports retain their original measurements; new comparisons use
the enlarged reference set for both binaries.

```powershell
node scripts/benchmark_graph_audit.cjs --suite transition --server build/release-gcc-ninja22/numforge_web.exe --before build/server-c383a75-gcc/numforge_web.exe --repeats 3 --output build/graph-subpixel-2026-10-15.json --csv docs/benchmarks/GRAPH_SUBPIXEL_CURVES_HTTP_2026_10_15.csv
```

Actual SVG tests evaluate emitted M/L/C segments at independent parameters,
rather than relying on approximate `getPointAtLength` flattening for very steep
curves. They check sequential zoom, visible geometry, reset and the inspector
at 390/1280 widths. Generated screenshots are retained under build/.

## Results and validation

The final run completed 258 HTTP requests (43 cases × two binaries × three
repetitions), with no HTTP errors or detected pole/jump crossings. All 37 target
cases achieved full checked coverage without exhausting the point budget. Their
largest checked connected-curve error was **0.437 pixels**. The six controls
retain their previous geometry/work policy, including intentional limited
results around difficult poles.

Selected local HTTP medians, including serialization, request and client parsing:

| Case | Before → after (ms) | Points before → after | Checked coverage before → after | Response bytes before → after |
| --- | ---: | ---: | ---: | ---: |
| Desktop, ±2 | 147.96 → 87.46 | 8192 → 6673 | 0% → 100% | 433877 → 375590 |
| Desktop, ±2.5 | 143.52 → 113.37 | 8192 → 7313 | 0% → 100% | 450857 → 420927 |
| Small viewport, ±0.625 | 124.74 → 25.09 | 8192 → 2365 | 93.35% → 100% | 468498 → 137842 |
| Desktop, signed mixture | 143.03 → 38.97 | 8192 → 3659 | 0% → 100% | 460301 → 212830 |
| Desktop, four carriers | 152.03 → 42.56 | 8192 → 3645 | 0% → 100% | 458383 → 210622 |

Full measurements are in [the CSV](GRAPH_SUBPIXEL_CURVES_HTTP_2026_10_15.csv).
These machine-local medians describe the tested workload, not a universal frame
rate or guaranteed error bound for arbitrary expressions.

Validation completed:

- Seven native CTest groups passed on both MSVC Release and GCC RelWithDebInfo:
  graph, application expression, application, session, web API, web remote and
  web UI. After final protocol/UI adjustments, the four directly affected
  graph/web groups were rebuilt and passed again on both compilers.
- The full 158-test Chromium graph/guide run passed. The final 33-test targeted
  run passed after those adjustments, covering subpixel geometry, smooth curves,
  multiple functions and coordinate inspection.
- Native tests exercise allocation failures, shared deadlines, unchanged output
  on failure and the explicit opt-in. Browser tests cover legacy interpolation,
  explicit precision, invalid slope policy, bounded oversized-response fallback,
  sequential zoom, reset and actual emitted SVG geometry.
- Desktop and mobile screenshots were visually inspected, including the thinner
  mobile stroke. Existing point, time, response and cache limits were retained.

## Remaining scope

General unsupported phases/compositions and extreme resolutions can still
exhaust finite budgets; the tested oversized-response fallback intentionally
retains the legacy limited result. Conservative dense envelopes can overestimate
correlated/beat ranges. Firefox/WebKit/physical-device checks remain release
gates. No universal instant-rendering, mathematical certification, release
sign-off, deployment, push or version bump is implied by these results.

The subsequent [Firefox/WebKit browser check](../testing/GRAPH_BROWSER_CHECK_2026_10_16.md)
records engine coverage and test portability fixes; physical-device validation
is tracked separately there.
