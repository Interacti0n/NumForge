# Exponential screen tails — 15 October 2026

The release audit's `exp(-((x-0.1234)/0.005)^2)` failed as a whole with
`value too large`. Its tails underflowed binary double, causing exact fallback
to compute huge negative exponentials. Their decimal scale exceeded the
precision evaluator's supported range. The function itself is bounded by one.

## Implementation and limits

The internal screen probe opts into an absolute error allowance of vertical
span / height / 64. An underflowed exp with a finite argument whose upper error
estimate is below log(DBL_MIN) may become display zero with error DBL_MIN.
Supported arithmetic carries that error, never allowing the bound itself to
underflow to zero. The final bound must fit the screen allowance. Denominators
containing projected tails, roots, logarithms, powers and other nonlinear
magnification retain uncertainty/exact fallback. Small propagated roundoff at a
well-conditioned exp can also be resolved within the screen allowance; this
avoids exact evaluation of an entire sum of Gaussians at a cancelled vertex.
Product and square estimates include the product of input errors.

Exponent argument derivatives remain available in otherwise flat tails. Before
refinement, sampled sign brackets use at most 40 bisection probes per exp slot
and seed interval. Turns exactly on a seed are included. Actual function
evaluations at those candidates and curvature-estimated shoulders seed normal
refinement. This prevents a narrow peak from disappearing and keeps a very wide
view from needing excessive refinement depth merely to reach its shoulders.
Every resulting edge still goes through domain/phase and midpoint checks.
When exp turns were found, the midpoint test also remains active below a pixel;
ordinary monotone exp retains the previous smooth screen-floor behavior.

This is an approximate finite display algorithm, not certified interval
arithmetic or a completeness proof. Arbitrary unseen features remain possible.
Ordinary numeric probes retain underflow uncertainty. Calculator evaluation,
explicit precision fallback, public schemas and the five-second/point/depth/
allocation/response limits are unchanged. Unsupported compositions and precision requests can still encounter
the original extreme scale limit; the SDK exp algorithm is separate work.

## Regression scope

- 72 combinations: x cameras ±10 and ±1000, desktop/small canvases, centres 0,
  0.1234 and −0.43127, widths 0.04/0.005/0.0004 and peaks/valleys. Independent
  formulae check point values, accepted peak/shoulder chords, bounded counts
  and complete refinement.
- Offset/trend, two peaks, a near-boundary peak and a defined-domain gap in
  exp(-1/x²), plus the actual Chromium SVG maximum.
- Native tests retain ordinary underflow uncertainty, reject nonlinear and
  large amplified tail reuse, check insufficient screen tolerance and invalid
  inputs, and exercise 20 stratified allocation failures and inherited deadline
  interruption with atomic output preservation.
- The earlier precision-camera Gaussian test now explicitly requests precision
  35 so it continues protecting that independent fallback path.

## Reproduction

Compare GCC RelWithDebInfo builds on the same PC, serially after tests/builds.
The preserved `188e77f` executable SHA256 is
`4e13f14c14ee482b12e3e9a35527d5a694d89b9c57e2a35519197cec4fce97f7`.
The corrected GCC executable SHA256 is
`4d1aa4afb9a3cc30c2f6c9ed925991ddba16e6c7b9e304fd88b7c723ec9bdba3`.
Do not compare these absolute GCC times against previous MSVC reports.

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
node scripts/benchmark_graph_audit.cjs --before build/server-188e77f-gcc/numforge_web.exe --server build/release-gcc-ninja22/numforge_web.exe --suite exponential --repeats 3 --output build/graph-exp-tails-2026-10-15.json --csv build/graph-exp-tails.csv
node scripts/benchmark_graph_audit.cjs --before build/server-188e77f-gcc/numforge_web.exe --server build/release-gcc-ninja22/numforge_web.exe --repeats 3 --output build/graph-exp-controls-2026-10-15.json --csv build/graph-exp-controls.csv
```

The exponential suite has 72 simple cases, four composed/domain cases and six
controls, for 82 cases / 492 requests across both versions and three repetitions.
The unchanged default suite adds 312 requests. The diagnostic checks 4097
independent coordinates plus explicit feature/shoulder anchors. Errors refer to
the displayed interpolation, not only point values. Raw payloads and images
remain under ignored build/; CSVs retain timing ranges and geometry summaries.

Validation: all 136 graph/guide Playwright tests pass on the corrected MSVC
build. Seven selected MSVC CTest targets and five GCC CTest targets pass,
including native fault/deadline regressions and the ordinary evaluator controls.

## Results

Committed data: [82-case exponential suite](graph_exp_tails_2026_10_15.csv) and
[unchanged 52-case controls](graph_exp_controls_2026_10_15.csv). Times are
medians of three serial loopback HTTP requests, including evaluation,
serialization and transfer. Small millisecond differences are noisy.

| Case | Before → current HTTP ms | Before → current maximum sampled render error, pixels | Current points |
| --- | ---: | ---: | ---: |
| Gaussian width 0.04, default control suite | 1212.63 → 2.41 | 0.216 → 0.503 | 79 |
| Gaussian width 0.005, default control suite | rejected → 2.53 | unavailable → 0.502 | 79 |
| Width 0.0004, centre 0.1234, x ±1000 | rejected → 2.44 | unavailable → 0.329 | 80 |
| Valley with offset and linear trend | rejected → 2.74 | unavailable → 0.622 | 89 |
| Two Gaussian peaks | rejected → 3.67 | unavailable → 0.502 | 93 |
| exp(x), default control suite | 4.94 → 4.35 | 0.000739 → 0.000739 | 561 |
| sin(1/x), default control suite | 23.25 → 26.23 | 0.109 → 0.109 | 3317 |

All 82 cases succeed in the corrected exponential suite; 63 were rejected
before. All 75 simple/composed peak cases have complete sampled visible
coverage, no refinement-limit flag and at most 93 points. Their worst sampled
interpolation error is 0.6221 pixels. The separate exp(-1/x²) domain case still
reports limited refinement near its hole, uses 133 points and does not connect
across zero. Sampled coverage improves from 0.99658 to 1.

The default corpus adds no rejected case, invalid edge, accepted known-pole
crossing or sampled coverage loss. The successful width-0.04 Gaussian trades
its previous 0.216-pixel precision interpolation for 0.503-pixel fast
interpolation, still below one pixel, while avoiding precision fallback.
Unaffected controls retain their geometry; the sin(1/x) timing is about three
milliseconds higher in this run, so this is not a universal speedup claim.

The independent Decimal oracle checks 5512 visible point values in all 72
simple Gaussian cases with maximum scaled error 8.59e-14. Seven original oracle
controls add 511 checks, maximum scaled error 1.29e-14. Independent 100/140-digit
references agree within 1.76e-97 across both recordings. This checks sampled
values, not a global interpolation bound.

The actual Chromium SVG at the very narrow Gaussian maximum draws
0.9999806394 (0.000361 plot pixel from one), both initially and after background
preparation. Expansion/Escape, two functions, horizontal/vertical keyboard
movement, reset and syntax-error recovery also pass the separate SVG smoke.
All 804 benchmark requests complete; raw payloads remain locally available.

Next release blockers: false floor/ceil jump connections, the persistent
Sampling state on some wide step views, dense mixed-expression limits and
cross-engine/real-device validation. This change is not release sign-off.
