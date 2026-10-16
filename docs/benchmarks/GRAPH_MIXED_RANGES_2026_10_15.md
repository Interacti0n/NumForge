# Mixed graph ranges — 15 October 2026

The quadratic-phase follow-up still exhausted 8192 points for
`sin(x*x)+cos(1000x)` on ±100. Checked reference coverage was 87.51% desktop
and 94.97% small: the slow quadratic phase prevented a band near its vertex,
while the other carrier remained too dense to sample economically.

The later [subpixel-curve follow-up](GRAPH_SUBPIXEL_CURVES_2026_10_15.md)
addresses the intermediate-scale sampling gap. Measurements below describe
the preceding `c383a75` implementation.

## Change and scope

The C sampler now requires **at least one** full monotone phase period per pixel
column. Every other supported carrier contributes a bounded range:

- Phase bounds include endpoints, an interior quadratic vertex where present,
  and approximate coefficient/roundoff margins.
- Sin/cos bounds include endpoint values and all interior trig extrema. A full
  phase interval uses −1..1. Nominal phases retain the existing ±1e6-radian
  limit; non-dense phase ranges must also fit within that limit after margins.
- Signed affine amplitude bounds multiply trig bounds using all four endpoint
  products. Term ranges add to the affine trend, with outward rounding.
- A band is emitted only if a carrier actually qualifies as dense and every term
  has usable finite bounds. Other columns retain ordinary sampling. Zooming to
  a resolvable scale removes bands and restores sampled curves.

This remains the existing restricted model: up to four sin/cos occurrences,
affine/quadratic phases, affine amplitudes and affine trend. Products of carriers,
variable denominators, tangent, higher/non-polynomial phases and nonlinear outer
transformations retain their existing paths. No point connection or domain guard
is relaxed. Precision fallback and explicit precision remain in place.

These are approximate possible-value ranges, not certified interval arithmetic,
exact local extrema or a curve through invented values. Treating amplitudes and
phases independently can overestimate correlation/cancellation and beat envelopes.
Coordinate inspection continues to omit band-only regions. Five-second,
allocation, point, response and cache limits are unchanged.

## Reproduction

Windows x64 GCC RelWithDebInfo, preserved `571ac3b` executable SHA256
`4c6f018d94e2c1125bd8924439f765701bb96b8fe635595bc13363d7bb0ed4a5`;
new executable SHA256
`2090ec2ee883d8acdeabcf9ec63a09d9488ec8588c92c6735e48a76f62d731b6`.
The `mixed` suite has 34 mixed cases at default/resolved/wide views across two
requested viewport sizes, four intermediate-scale diagnostics, and eight
controls/known limitations: 46 unique cases. Independent
checks use 4097 coordinates plus explicit phase/amplitude/trig feature anchors.

```powershell
node scripts/benchmark_graph_audit.cjs --suite mixed --server build/release-gcc-ninja22/numforge_web.exe --before build/server-571ac3b-gcc/numforge_web.exe --repeats 3 --output build/graph-mixed-ranges-2026-10-15.json --csv docs/benchmarks/GRAPH_MIXED_RANGES_HTTP_2026_10_15.csv
```

## Measurements and validation

All 276 requests succeed, with zero tested pole/jump crossings. Measurements
are three serial repetitions per case/version after builds and tests finish.
They include loopback HTTP and serialization; they are not universal latency
or frame-rate guarantees.

| Case | Median HTTP ms before → after | Response bytes before → after | Points before → after | Bands after | Checked coverage before → after |
| --- | ---: | ---: | ---: | ---: | ---: |
| sin(x*x)+cos(1000x), default, 800 columns | 152.27 → 6.03 | 391340 → 67115 | 8192 → 34 | 800 | 0.05% → 100% |
| cos(1000x)+sin(x), default, 800 columns | 143.69 → 5.52 | 391544 → 67286 | 8192 → 33 | 800 | 0% → 100% |
| sin(x*x)+cos(1000x), ±100, 800 columns | 150.54 → 5.31 | 395469 → 43236 | 8192 → 33 | 800 | 87.51% → 100% |
| sin(x*x)+cos(1000x), ±100, 320 columns | 147.62 → 3.62 | 385828 → 18994 | 8192 → 33 | 320 | 94.97% → 100% |

All 34 default/resolved/wide mixed cases have full checked coverage and
`limited:false`. The 16 default cases take median 3.65–6.95 ms after the change.
All 16 resolved views have no bands and maximum checked connected error below
0.000043 px. For the slow-sine mixture at 800 columns, native/web tests also
require band width below 2.026; actual slow-term variation is retained instead
of simply replacing every term by ±amplitude.

Full band coverage is not an exact curve or tight-envelope accuracy claim.
The [CSV](GRAPH_MIXED_RANGES_HTTP_2026_10_15.csv) retains all work counts,
coverage, errors, timing ranges and diagnostic failures. Raw first responses
remain in the ignored build JSON.

- Seven selected graph/expression/application/session/web CTest targets pass
  with MSVC and GCC. Added independent value checks through signed-amplitude and
  quadratic turns, full allocation-failure sweeps, shared-deadline interruption
  and unchanged caller output on failure.
- All 154 Chromium graph/guide tests pass. New cases cover interior trig extrema,
  degrees, supported mixtures, narrow ordinary curves, explicit precision,
  viewport dimensions and band-to-curve/reset transitions. Unsupported
  expressions retain their domain/pole guards.
- Actual SVG range/zoom/reset tests pass at 390 and 1280 widths; both generated
  screenshots were visually inspected. Band-only regions keep their approximate
  notice, with no fabricated point connections.

## Remaining scope

Intermediate scales expose a pre-existing sampler limit. For the same mixed
function on ±2, the 800-column result still has 8192 points, no bands and zero
checked curve coverage (before and after); the 320-column result now has a full
band and full checked coverage. On ±1, both sizes still reach 8192 points and
99.976% checked coverage, with `limited:true`. No full period fits a column in
these remaining views, so range emission must not hide this limitation.

Next: improve acceptance/refinement of resolvable subpixel oscillatory curves
in that transition region, comparing rendered interpolation against actual
control evaluations while retaining phase/domain guards. Avoid increasing point
limits or relaxing checks without independent geometry evidence.

General unsupported phases and compositions still have finite sampling limits.
Range overestimation for correlated carriers/beats remains a potential refinement.
Firefox/WebKit and physical-device checks remain release gates; these changes
alone are not a release sign-off. No push, deployment or version change is part
of this follow-up.
