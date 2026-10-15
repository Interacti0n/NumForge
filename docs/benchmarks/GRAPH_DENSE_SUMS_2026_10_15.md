# Dense graph sums — 15 October 2026

The release audit found no accepted visible strokes for
`sin(1000x)+sin(1001x)` across −10..10: the ordinary sampler consumed all 8192
points. The existing opt-in pixel envelope recognized only one affine carrier.
This follow-up extends that bounded representation to sums/differences of up
to four sin/cos occurrences with affine phases, affine amplitudes and an affine
trend. It does not solve arbitrary nonlinear phases or certify continuity.

The later [quadratic-phase follow-up](GRAPH_QUADRATIC_PHASES_2026_10_15.md)
addresses the wide chirp. Measurements and scope below describe the affine-sum
change at `427654d`.

## Implementation and limits

- The existing C bytecode produces a fixed-size model; no JavaScript expression
  evaluation, text rewriting or additional HTTP schema is introduced. Identical
  phase/kind terms combine in metadata. The four-occurrence admission limit
  applies before combining them.
- Each remaining carrier must span a full period in each represented pixel
  column, with the existing numerical margin and ±1e6-radian phase limit.
  Per-column absolute amplitude bounds are summed and added to the affine
  trend range. Finite checks and outward `nextafter` rounding remain approximate
  screen arithmetic, not a certified interval API.
- These are conservative possible-value ranges. Different carriers can cancel;
  in particular, the band for nearby frequencies may overestimate the changing
  beat envelope. Full reference coverage does **not** imply an accurate envelope
  boundary or that every ordinate inside a band is attained.
- A band never supplies a point, a connection or an inspector y value. Zooming
  to a resolvable scale restores ordinary sampled/interpolated curves. Existing
  colors, visibility controls and per-function notices apply.
- Products of carriers, more than four occurrences, nonlinear arguments or
  amplitudes, variable denominators and domain-sensitive transformations retain
  ordinary sampling. A mixture with a slow carrier also retains sampling when
  every carrier cannot complete a period per column. Numerical uncertainty
  still retains precision fallback, including some cancelling intermediates.
- Existing five-second, allocation, point, phase, domain and pole guards remain.
  Model extraction uses a bounded stack; band storage remains at most one record
  per requested column, maximum 2048.

## Reproduction and measurements

Windows x64 GCC RelWithDebInfo, same configured build, serial loopback requests,
three repetitions per case/version after tests/builds finished. Before is the
preserved `5f450b8` executable, SHA256
`4843c347eb93810a42ce739a785a0dcbc3b76b3624f807fe944517c716d5ca9c`.
After executable SHA256:
`64be20cab128982a5174da878e5578f8959d54f058d0569b290d1cbfdaf0a064`.
The suite has 28 unique cases, 168 total requests: five sum forms at wide and
resolved scales in two viewport sizes, plus eight controls/known limitations.

```powershell
node scripts/benchmark_graph_audit.cjs --suite dense --server build/release-gcc-ninja22/numforge_web.exe --before build/server-5f450b8-gcc/numforge_web.exe --repeats 3 --output build/graph-dense-sums-2026-10-15.json --csv docs/benchmarks/GRAPH_DENSE_SUMS_HTTP_2026_10_15.csv
```

Summary of median whole HTTP latency and response bytes:

| Wide case | Latency ms before → after | Bytes before → after | Sample points before → after | Bands after |
| --- | ---: | ---: | ---: | ---: |
| Two nearby sines, 800 columns | 166.00 → 5.24 | 435002 → 61447 | 8192 → 33 | 800 |
| Two nearby sines, 320 columns | 157.26 → 5.20 | 435002 → 19063 | 8192 → 33 | 320 |
| Four carriers, 800 columns | 195.95 → 5.90 | 432296 → 61442 | 8192 → 33 | 800 |
| Four carriers, 320 columns | 194.98 → 4.11 | 432296 → 19058 | 8192 → 33 | 320 |

Across all ten wide supported cases, median latency is 3.67–5.90 ms after the
change. All have `limited:false`, no exact probes, and all independently checked
visible reference values lie within a band; before, accepted reference coverage
was zero. These are range displays, so a zero chord-error metric is inapplicable
to their shape, not proof of exact rendering. All ten resolved cases have no
bands, full checked curve coverage and maximum rendered error below 0.000152 px
on the deliberately narrow ±0.001 views. Local timing samples are diagnostic;
they are not a universal latency or frame-rate guarantee.

The [CSV](GRAPH_DENSE_SUMS_HTTP_2026_10_15.csv) preserves medians, ranges, bytes,
coverage, errors and work counts. Raw first-response payloads remain under build/.

## Validation and remaining work

- All seven selected graph/expression/application/session/web CTest targets pass
  with MSVC and GCC. Added model/failure tests and full allocation-failure/deadline
  sweeps for a four-carrier graph preserve the previous caller result.
- All 146 Chromium graph/guide tests pass. After the final guide and fallback
  assertions, all 18 dense/guide tests pass again. Independent sum values are
  checked across columns; tests cover signs, division, trend, affine amplitude,
  repeated terms, four carriers, unsupported products/larger sums, precision
  fallback, resolved zoom and simultaneous band/curve rendering.
- Known nonlinear-phase cases remain open: `sin(x*x)` on ±100 still reaches
  8192 points, covers only 30.73% of the checked reference positions and has
  maximum connected error 4.157 px. `sin(1/x)` remains limited near its pole,
  with 99.951% checked coverage. No tested pole is crossed.

Next: handle bounded nonlinear phase ranges/turns, beginning with the quadratic
chirp, and tighten beat bounds if needed. Keep a visible limitation for unsupported
or unresolvable cases. Firefox/WebKit and physical-device release checks remain.
No release sign-off, version bump, push or server deployment is part of this change.
