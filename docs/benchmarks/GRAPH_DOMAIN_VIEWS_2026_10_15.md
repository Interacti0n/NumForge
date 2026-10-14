# Domain-sensitive graph views — 2026-10-15

`1/ln(ln(sqrt(x)))^2` wasted most of its sampling work around an off-screen pole
and could draw the visible branch with coarse precision-fallback chords. The
camera sampler now keeps confirmed non-real domains local and defers guarded
off-screen refinement without accepting connections. Visible parts receive
more useful detail; y-only movement refines previously deferred regions.

Under the conventional real-function interpretation, x must exceed 1 and
x=e²≈7.389056 is a pole. There are two positive branches, tending to infinity
at the pole. At the default −10..10 axes the right branch is above y=10, so
seeing only the left branch is expected. Complex-to-real compositions retain
the precision evaluator; this change does not redefine complex arithmetic.

## Changes and constraints

- Exact probes still classify non-real/domain samples. Those confirmed cases
  no longer count as generic numerical uncertainty for whole-expression fallback.
  A known non-real intermediate that produces a real final sample requests
  precision fallback, including `abs(sqrt(x))` on negative x.
- For non-globally-smooth ASTs with explicit y bounds, a guarded parent up to
  eight horizontal pixels wide can defer refinement if all three real samples
  are beyond the same y edge with a margin and have finite slopes of the same
  sign. No connection is accepted. Sampled turns, guard failures, non-real
  samples and possible pole crossings continue refining.
- Optional schema 3 `screen_clipped:true` describes this deferral. It is separate
  from `limited` and domain gaps. The browser stores requested y coverage,
  retains useful vertical cache levels and recomputes if clipped regions enter
  a new view. Fully sampled siblings remain cached. Legacy requests without
  full camera bounds keep their old refinement criterion.
- Restricted globally smooth ASTs retain connected screen-floor chords and
  stationary/phase anchors. Point/depth/exact-probe, five-second, allocation,
  response and browser cache limits remain unchanged. These are approximate
  screen heuristics, not continuity or correctly-rounded-value proofs.

The UI distinguishes off-screen curves, absence of sampled real points, domain
gaps, unresolved detail, unit-valued output and numeric projection limits. Notes
are local to each curve; the shared status points to those notes when necessary.
Errors remain separate. Dense envelopes keep their existing zoom hint.

## Controlled HTTP comparison

Before: MSVC Release `cb619e3`. After: corrected candidate, same compiler/PC.
Nine cases with three repetitions per version give 54 owned-loopback requests.
Both versions use the same camera/interpolation/dense options. Wall times include
computation, serialization and text transfer; they are not arithmetic-only or
remote-tunnel measurements. No concurrent test/compiler workload ran.

| Case | HTTP median ms before → after | Points before → after | JSON bytes before → after |
| --- | ---: | ---: | ---: |
| Nested log, x −10..10 / y −10..10 | 361.505 → 43.048 | 1023 → 183 | 116003 → 8201 |
| Nested log, x 1.001..20 / y −10..100 | 87.110 → 3.350 | 8192 → 151 | 471051 → 8955 |
| Nested log, x 2..30 / y −10..10 | 85.978 → 3.072 | 8192 → 159 | 460629 → 7953 |
| Nested log, x 7..8 / y −10..1000 | 88.098 → 5.497 | 8192 → 223 | 473755 → 11302 |
| tan(x), default camera | 29.921 → 3.832 | 4777 → 517 | 268322 → 26293 |
| 1/(x−0.1)², default camera | 25.590 → 2.815 | 3791 → 137 | 218027 → 7124 |
| x³−3x control | 4.226 → 3.780 | 371 → 371 | 17415 → 17415 |
| sin(33x) control | 13.436 → 13.842 | 2259 → 2259 | 115005 → 115005 |
| sin(1000x) envelope control | 4.340 → 4.363 | 33 + 800 bands → same | 61459 → 61459 |

The default nested-log path changes from precision fallback to approximate double
screen evaluation with local exact domain probes. Its domain probes still cost
about 43 ms here; positive-only views need about 3–6 ms. Remaining `limited:true`
near poles reflects unresolved mathematical/numeric boundaries, not a claim that
every interval was completed. Entirely off-screen gaps do not trigger a visible
warning. Control counts/bytes are unchanged; small timing variations are noise.

Visible values and accepted connections are compared with independent formulae.
100-digit Python Decimal sqrt/ln arithmetic checks 1005 visible nested-log values
from retained original JSON; maximum relative error is approximately 1.285e-14
after, 6.681e-15 before. This is a measured corpus, not a general precision bound.
Browser regressions probe accepted chords at quarter/mid/three-quarter positions
within two pixels, reject connections across x=1 or x=e² and require both branches
where they lie in the camera. Binary libm is deliberately not used as an oracle
for enormous invisible values arbitrarily close to the pole.

Summary: [CSV](graph_domain_views_2026_10_15.csv). Raw local JSON:
`build/graph-domains-2026-10-15.json`, with original payloads;
`build/graph-domain-oracle-2026-10-15.json` for Decimal checks.

```powershell
node scripts/benchmark_graph_dense.cjs --suite domains --before build/server-cb619e3/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-domains.json
```

## Verification

125 graph/guide browser tests pass. New tests cover both branches, domain and pole
flags, visible chord error, complex recovery, English/Slovak notes, vertical-only
resampling with an unchanged sibling, typed non-error notes and malformed clipping
metadata. Native graph tests cover values, bounds, allocation failures and deadline
atomicity; relevant graph/application/expression/session/web API tests pass with
MSVC and GCC warnings-as-errors builds. Physical phones and other browsers still
require separate validation.

A follow-up run of the sustained diagnostic with four cycles per case passes
24 interactions and 36 SPA round trips across desktop/portrait/landscape.
Native pinch checks, cancellation, warm toggles and cache bounds pass, with no
retained old graph roots. Whole-page forced-GC heap peaks at 7.089 MiB; cached
records peak at 12255 per function. No ≥50 ms interaction long tasks were
recorded; maximum frame interval is 33.4 ms. This shorter verification is not
a direct heap comparison with the previous sixteen-cycle stress workload.
Raw artifact: `build/graph-domain-stress-2026-10-15.json`.
