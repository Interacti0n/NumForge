# Precision graph camera refinement — 15 October 2026

This implements the first correctness fix from the
[release audit](GRAPH_RELEASE_AUDIT_2026_10_15.md): precision-fallback curves must
follow the visible camera, rather than accepting visible chords with a tolerance
dominated by enormous off-screen seed values. The C evaluator remains the source
of values; no function evaluation or derivative implementation is added in JS.

## Implementation and contracts

The internal C application layer adds `application_graph_adaptive_camera`.
Existing uniform/adaptive/point-limit entry points retain their legacy behavior.
The HTTP screen adapter uses the camera entry point when it falls back to
precision arithmetic and both y bounds are supplied, including non-default
requested precision. Bounds and all sampling decisions stay BigDecimal/
BigRational values; output remains schema 2 with typed points.

- Camera tolerance is one vertical plot pixel, using `height−68` plot pixels.
  Larger intervals retain the finer of this and the original seed criterion,
  preserving tiny seed signals that can reveal a narrow feature.
- A supported smooth parent no wider than one horizontal plot pixel may use
  the camera tolerance only when its measured midpoint error also passes.
  Width alone cannot accept a high-error edge. This avoids spending the whole
  budget on detail below the screen scale after locating a small feature.
- For three real, ordered probes beyond one camera edge by two pixel tolerances,
  refinement may be deferred once the parent is at most eight horizontal plot
  pixels wide and strict captured phase checks pass. Deferred edges stay false;
  the sampler never joins them as a performance shortcut.
- Camera-aware schema 2 adds width/height, `quality:"refined"` and optional
  `screen_clipped:true`. A coarse request still uses full precision refinement;
  clients must not mistake it for the double sampler's coarse algorithm.
  Slopes and dense envelopes remain absent in precision responses.
- The browser validates schema 2 precision deferral metadata and reuses its
  existing y-coverage cache checks. Revealing a deferred y region resamples the
  affected function; a fully sampled sibling still reprojects without a request.

The five-second shared request scope, 2048-point/depth-12 precision limit,
allocation and response bounds, session isolation and atomic failure outputs are
unchanged. Deferral and midpoint decisions are sampled display heuristics, not
proofs that all extrema or discontinuities were found.

## Regressions and validation

The initial API and actual SVG regressions fail against `8e004db`, with roughly
63 pixels of error at the API's chosen interpolation probes and approximately
68 pixels in the actual sinh stroke. The tests then require less than 1.5 pixels
of error at independent visible coordinates for sinh/cosh in both the default
y −10..10 view and a shifted y 90..110 view, at 800×440 and 320×300 canvas sizes.

An additional narrow-Gaussian regression preserves the small seeded signal at
`exp(-((x-0.1234)/0.04)^2)` and checks its actual connected peak. This protects
against simply replacing a fine seeded tolerance with a coarser screen tolerance,
and against exhausting the budget by refining that tiny tolerance indefinitely.
These failures informed the combined width-and-error rule above.

Native tests cover clipping and atomic output over 20 stratified allocation
failures, an inherited expired deadline, invalid dimensions/y bounds, explicit
precision 35, coarse-request metadata and unchanged legacy response fields.
Browser tests cover actual SVG geometry, vertical cache refinement with a
complete sibling, and malformed precision deferral metadata.

## Measurements

Same PC, MSVC Release, before `8e004db` versus this change. Measured candidate
executable SHA256:
`3a0ae513cb9d9e5785a0bfcd210bfef3a3e28f01b2a3336a017ebb5f1777601a`.
The 52-case matrix uses three requests per version, **312 requests** total. HTTP
wall time includes evaluation, serialization and loopback receipt. No concurrent
compiler or other test/benchmark workload runs during timing.
Full results: [before/after CSV](graph_precision_camera_2026_10_15.csv).

| Default camera case | Visible render error px before → after | HTTP median ms before → after | Points before → after |
| --- | ---: | ---: | ---: |
| sinh(x) | 2.210 → 0.302 | 29.17 → 50.76 | 121 → 221 |
| sinh(10x) | 67.784 → 0.221 | 38.67 → 75.05 | 149 → 293 |
| cosh(10x) | 72.668 → 0.219 | 48.08 → 81.38 | 189 → 297 |
| Gaussian, width 0.04 | 2.890 → 0.216 | 438.40 → 706.76 | 79 → 109 |

Correct shape requires more evaluations than the previous inaccurate chords.
The selected steep functions remain below 0.1 seconds here; the expensive
Gaussian remains below one second. This is a correctness fix, not a blanket
speedup. Unchanged fast sine/pole controls retain their point counts/geometry;
atan/asin/tanh and complex-recovery controls retain their previous fine accuracy.
The reference matrix has no new invalid edges, known pole crossings, whole-plot
failures or visible coverage regression. Existing missed fast peaks, scale
rejection and dense-limit cases are still recorded as failures/limitations.
Independent Decimal references at 100 and 140 digits check 521 visible sampled
values, with maximum scaled point error approximately 1.285e−14 in the selected
corpus. This is a value check, separate from screen interpolation accuracy.

Final validation: 130 graph/guide browser tests pass, as do seven selected MSVC
and five GCC native/adapter test executables, including allocation/deadline tests.

The 12-run four-function browser matrix compares both builds with three
repetitions per desktop/mobile layout. All four foreground curves render without
errors. Median frame after all four foreground jobs completes: **264.7 → 332.1 ms
desktop**, **263.8 → 229.0 ms mobile layout**. Warm hide/show makes zero sampling
requests. Current zoom input-to-frame p95 medians are 29.7 / 31.5 ms, with maximum
recorded frame intervals 16.8 ms and no ≥50 ms long tasks in measured windows.
This is local headless Chromium, not a physical-phone or WAN guarantee. The
desktop pays for finer geometry; smaller mobile cameras can defer more invisible
work. See [the phase CSV](graph_precision_camera_browser_2026_10_15.csv).

Actual settled SVG checks at the original bad coordinates show approximately
0.00029 px sinh error and 0.0764 px cosh error, consistent with the independent
API geometry checks. These are two selected coordinates, not global maxima.

## Reproduction

Run serially with an old and a newly built Release server:

```powershell
node scripts/benchmark_graph_audit.cjs --before build/server-8e004db/numforge_web.exe --server build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-precision-camera-2026-10-15.json --csv build/graph-precision-camera.csv
python scripts/check_graph_audit_decimal.py build/graph-precision-camera-2026-10-15.json --output build/graph-precision-decimal-2026-10-15.json
node scripts/benchmark_graph_multi.cjs --before build/server-8e004db/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --suite precision --repeats 3 --output build/graph-precision-multi-2026-10-15.json
```

The `precision` browser suite uses sinh(10x), cosh(10x), tanh(10x) and atan(100x),
desktop 1280×900 and mobile layout 390×844. Its warm visibility check uses atan,
which remains in the zoomed camera; an off-screen cosh curve is correctly absent
and must not be mistaken for a failed toggle. Raw recordings are ignored.

The fast Lorentzian missed-peak case, exp tail scale rejection, step jumps and
unsupported dense combinations remain separate follow-up work. This is not an
overall graph release sign-off.
