# Quadratic graph phases — 15 October 2026

The release audit found that `sin(x*x)` on −100..100 exhausted 8192 points,
covered only 30.73% of independently checked reference positions, and had a
4.157 px maximum error in accepted strokes. Affine-carrier ranges alone could
not represent this changing frequency. This change adds bounded quadratic
phase metadata and combines sampled central curves with dense outer ranges.

## Implementation

The C bytecode model now accepts affine/quadratic arguments formed by addition,
subtraction, multiplication, constant division, square/postfix square or power
two. Amplitudes and the outer trend remain affine; the maximum is still four
sin/cos occurrences. Higher/non-polynomial phases, variable denominators,
products of carriers and nonlinear outer transformations retain sampling.

A represented pixel column must contain a full phase period for **each**
remaining carrier. Endpoint phase derivatives must have the same sign with
roundoff margins; columns containing a quadratic vertex are excluded. Phase
endpoints keep the ±1e6-radian limit. Expanded-coefficient cancellation and
roundoff estimates can defer a column to ordinary sampling. These estimates and
band bounds are approximate screen arithmetic, not certified interval analysis.

Quadratic phase vertices add actual evaluations of the original expression as
discovery seeds. They do not invent function extrema, set derivatives to zero,
or accept adjacent edges. In recognized quadratic models, the half-pixel smooth
shortcut is disabled and the stricter phase and midpoint-error checks remain in
resolved parts. This matters at transitions from dense ranges to sampled curves:
simply adding bands repaired coverage but left a 3.217 px error in a pilot.

Band-covered intervals never become accepted chords or inspector y values.
Resolved parts keep the existing interpolation and guards; zooming sufficiently
restores ordinary curves without bands. API schema, per-request five-second and
allocation/point limits, SDK arithmetic and explicit-precision behavior remain.

## Reproduction

Windows x64 GCC RelWithDebInfo. Compare the preserved `427654d` executable
(SHA256 `64be20cab128982a5174da878e5578f8959d54f058d0569b290d1cbfdaf0a064`)
with the new executable
(SHA256 `4c6f018d94e2c1125bd8924439f765701bb96b8fe635595bc13363d7bb0ed4a5`).
The `quadratic` suite contains 40 quadratic cases across desktop/small requested
dimensions, plus six unchanged controls. It includes centred/shifted sin/cos,
positive/negative quadratic coefficients, resolved zooms, affine amplitudes,
two distinct phase turns, the ±1e6 phase boundary and a known fast/slow mixture.
The independent harness checks 4097 coordinates plus specified vertices,
actual sample values and browser-equivalent limited interpolation.

```powershell
node scripts/benchmark_graph_audit.cjs --suite quadratic --server build/release-gcc-ninja22/numforge_web.exe --before build/server-427654d-gcc/numforge_web.exe --repeats 3 --output build/graph-quadratic-2026-10-15.json --csv docs/benchmarks/GRAPH_QUADRATIC_PHASES_HTTP_2026_10_15.csv
```

## Results and validation

All 276 serial loopback requests succeed. Timing uses three repetitions per
case/version after builds and tests finish. These local medians include HTTP
and serialization, and are not universal latency or frame-rate guarantees.

| Case | Median HTTP ms before → after | Points before → after | Bands after | Checked coverage before → after | Maximum connected error px before → after |
| --- | ---: | ---: | ---: | ---: | ---: |
| sin(x*x), ±100, 800 columns | 130.42 → 20.37 | 8192 → 2077 | 700 | 30.75% → 100% | 4.157 → 0.177 |
| sin(x*x), ±100, 320 columns | 136.41 → 6.95 | 8192 → 341 | 304 | 30.75% → 100% | 2.888 → 0.111 |
| Shifted cos((x−0.1234)²), ±100, 800 columns | 126.20 → 18.43 | 8192 → 2132 | 699 | 30.77% → 100% | 4.553 → 0.206 |
| Affine amplitude/trend, ±100, 800 columns | 171.53 → 10.27 | 8192 → 715 | 767 | 10.27% → 100% | 7.691 → 0.727 |
| sin(x*x), ±1000, 800 columns | 175.53 → 8.06 | 8192 → 131 | 798 | 0.34% → 100% | No comparable resolved curve |

The original desktop chirp response shrinks from 438472 to 149257 bytes; the
small case shrinks from 438623 to 34572 bytes. All 38 positive quadratic cases
have `limited:false` and full checked coverage, with maximum connected error
0.728 px across the corpus. All 16 resolved cases have no bands. The two
deliberately difficult mixed cases remain limited: coverage is 87.51% desktop
and 94.97% small, from zero before, with 8192 points still exhausted.

Coverage includes values enclosed by bands; it is not curve accuracy in those
columns. Connected error only describes independently checked accepted strokes.
A zero error for an almost wholly banded case must not be read as an exact
envelope. There are zero tested pole/jump crossings. The control geometries and
work counts remain unchanged. The [CSV](GRAPH_QUADRATIC_PHASES_HTTP_2026_10_15.csv)
contains all timing ranges, bytes, work and quality metrics. First-response raw
payloads remain in the ignored build JSON. Vertex anchors add a few reference
checks compared with the original release audit, hence minor coverage rounding
differences from its 30.73% baseline.

- All seven selected graph/expression/application/session/web CTest targets pass
  with both MSVC and GCC. Tests cover polynomial forms, rejected higher degrees
  and amplitudes, correct evaluated vertices, independent band values, complete
  allocation-failure sweeps, shared-deadline interruption and caller-output
  preservation.
- All 150 Chromium graph/guide tests pass. New tests check centred/shifted
  sin/cos, signs, degrees, postfix square, phase bounds, explicit precision,
  coefficient cancellation fallback, resolved zoom and independent geometry.
- Actual SVG zoom/range/reset checks pass at widths 390 and 1280; screenshots
  under build/ were visually inspected. Both sampled curves and range paths
  remain present during the tested wide view, and reset restores the resolved
  curve with less than one pixel checked error.

## Remaining scope

A fast carrier mixed with a slowly varying or turning carrier can still exhaust
the point budget in columns where the all-carriers-full-period condition fails.
`sin(x*x)+cos(1000x)` is deliberately retained in the corpus to expose that
limitation. Conservative sum ranges can also overestimate beating/cancellation.
Neither an envelope nor full sampled-reference coverage proves continuity or
arbitrary between-sample accuracy. General nonlinear/non-polynomial phases and
Firefox/WebKit/physical-device release checks remain separate work.

Next: bounded ranges for slow terms inside a column containing another dense
carrier, with explicit domain/turn checks. Do not simply relax period checks or
join rejected edges. No release sign-off, version bump, push or deployment is
part of this change.
