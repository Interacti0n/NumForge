# Guarded narrow graph features — 15 October 2026

The release audit's Lorentzian `1/(1+((x-0.1234)/0.04)^2)` has a maximum of one,
but its displayed value near the maximum was approximately 0.09. Three small
sample values passed midpoint tolerance, and the old stationary search excluded
variable-denominator expressions. This fix adds bounded derivative-guided
discovery during the existing fast C refinement.

## Algorithm and scope

- For non-globally-smooth ASTs with screen bounds, a real sampled derivative
  sign change prevents immediate interval acceptance. A significant bend in the
  derivative also explores intervals wider than one horizontal pixel, helping
  locate a narrow feature whose first slope signs are masked by a linear trend.
- Guarded endpoint slope brackets use at most 32 bisection probes. Real/finite
  values, captured phases and domain/denominator guards must pass. An uncertain
  candidate uses the existing exact classifier and its reserve; optional search
  does not chase candidates outside the visible y camera plus its margin.
- An actual candidate sample splits the parent. It is generally not its
  midpoint, so neither neighbouring edge is accepted until its own midpoint is
  checked. This matters especially when a maximum lies close to a seed: using
  the near-endpoint root as a midpoint error test would accept a long bad chord.
- A private anchor flag avoids repeatedly searching the same numerically
  located turn. Values and derivatives remain those actually evaluated. No
  synthetic height, zeroed derivative or public point/HTTP field is added.
- The shared five-second scope, 8192 refined points, depth 16, 128 exact probes,
  allocation/response limits and optional-work reserves stay unchanged. Legacy
  calls without a vertical screen span and globally smooth ASTs retain their
  existing discovery behavior.

This is sampled screen geometry, not a proof that every extremum exists or has
been found. A feature with no useful sampled value/slope signal can still escape
finite sampling. Exp tail scale rejection, step discontinuities, dense mixtures
and the remaining release gates are separate work.

## Correctness and failure regressions

The initial API and actual-SVG tests fail against the previous sampler. Current
tests cover 32 peak/valley combinations: four centres, four widths from 0.2 to
0.0004 and both signs. Peak-height error must stay below 0.01 plot pixel,
accepted neighbouring linear chords below 1.5 pixels, all sampled values agree
with independent formulae, and these unimodal cases must not exhaust refinement.
Exact decimal digits of a stationary coordinate are not a graph guarantee.

Additional cases combine an offset, a linear trend, two separate peaks and a
maximum close to the x boundary. Actual SVG is tested at the original bad
coordinate. Native tests require both edges around an anchor to be accepted
after refinement, exercise 20 stratified allocation failures and interrupt an
inherited budget during work while preserving the previous output pointer.

## Reproduction

This step compares GCC builds of the previous `a572c0f` and the corrected source
on the same PC. The previous GCC executable was preserved before rebuilding;
its SHA256 is `7d4443afa8f4fa982ffd7cae1597e37a909dd72f161097f251d9893994b4921b`.
Earlier reports use MSVC timings; do not compare their absolute timings with
this GCC run as an algorithm-only comparison.
The corrected GCC executable SHA256 is
`4e13f14c14ee482b12e3e9a35527d5a694d89b9c57e2a35519197cec4fce97f7`.

Run timing suites serially, after tests/builds finish:

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
node scripts/benchmark_graph_audit.cjs --before build/server-a572c0f-gcc/numforge_web.exe --server build/release-gcc-ninja22/numforge_web.exe --suite narrow --repeats 3 --output build/graph-narrow-features-2026-10-15.json --csv build/graph-narrow-features.csv
node scripts/benchmark_graph_audit.cjs --before build/server-a572c0f-gcc/numforge_web.exe --server build/release-gcc-ninja22/numforge_web.exe --repeats 3 --output build/graph-narrow-controls-2026-10-15.json --csv build/graph-narrow-controls.csv
```

`--suite narrow` adds 64 simple cases over desktop/small canvas dimensions,
four composed/edge cases and six controls, for 74 cases. With three repetitions
per version that is 444 requests; the unchanged default 52 cases add 312.
References include 4097 x coordinates and explicit peak/shoulder anchors. The
default suite remains unchanged; schema values, slopes, bands and connections
are assessed using the existing independent screen diagnostic. Timing includes
evaluation, serialization and loopback transfer, not just arithmetic. Raw
payloads and images remain under ignored `build/`.

## Measurements and validation

Committed measurements: [74-case narrow suite](graph_narrow_features_2026_10_15.csv)
and [52-case control suite](graph_narrow_controls_2026_10_15.csv). Times are
medians of three loopback requests; a few milliseconds of variation are noise,
so these results do not establish a general speedup.

| Case | Before → current HTTP ms | Before → current maximum sampled render error, pixels | Before → current points |
| --- | ---: | ---: | ---: |
| Original Lorentzian, default control suite | 3.44 → 4.20 | 16.929 → 0.541 | 65 → 79 |
| Width 0.0004, centre 0.1234, desktop | 3.62 → 2.72 | 18.600 → 0.421 | 65 → 103 |
| Narrow peak with linear trend | 3.12 → 2.89 | 37.156 → 0.226 | 65 → 93 |
| Two narrow peaks | 2.78 → 3.55 | 18.580 → 1.239 | 65 → 109 |
| Peak at x = 9.999 | 2.46 → 2.92 | 2.718 → 0.382 | 77 → 81 |
| sin(1/x), default control suite | 19.98 → 25.20 | 0.148 → 0.109 | 3173 → 3317 |

All 68 narrow/composed cases retain complete sampled visible coverage, remain
unlimited and use at most 109 points. Their largest measured interpolation
error is 1.239 pixels. The unchanged default corpus shows no new rejected case,
invalid/pole connection, sampled coverage loss or render-error increase above
0.1 pixel. Derivative exploration adds work on some oscillatory expressions:
`sin(1/x)` costs about five additional milliseconds in this run.

The actual Chromium SVG at the original Lorentzian maximum draws
0.999997867 rather than approximately 0.09, an error of 0.000040 plot pixel.
The independent Decimal oracle checks 5580 visible point values in 64 simple
narrow cases: maximum scaled point error 2.41e-13. The original seven oracle
controls add 541 checks with maximum scaled error 1.29e-14. References computed
at 100 and 140 decimal digits agree within 1.76e-97 across both sets. These are
checks at sampled coordinates, not continuity or global error proofs.

Validation completed on the corrected source:

- MSVC: seven selected native/web CTest targets pass.
- GCC: five selected native/application/HTTP CTest targets pass.
- All 133 graph/guide Playwright tests pass, including API and actual-SVG peak
  regressions, lifecycle/cache checks and the composed peak cases.
- 756 serial benchmark HTTP requests complete across both binaries.
- Actual-SVG diagnostics and UI interaction smoke checks run separately.

The very narrow Gaussian scale error and floor jump connections still reproduce
in both versions. The width-0.04 Gaussian still takes about 1.2 seconds in these
GCC builds; this fast-sampler change does not optimize its precision fallback.
The next fix should address exp tail scale rejection, followed by step
discontinuities and the remaining release gates from the original audit.
