# Absolute-value graph valleys — 16 October 2026

The reported `abs(x*sin(x))` view lost its zero valleys after zooming out.
The same issue affected cosine, shifted/translated absolute expressions and
`sqrt((x*sin(x))^2)`. Point values were individually correct, but screen-floor
acceptance and incomplete corner discovery allowed visibly inaccurate strokes.

## Change

- Record inner arguments of `abs` in the existing C probes. Locate actual sign
  brackets during refinement, evaluate the original expression at the corner and
  check both child intervals before accepting edges. Do not assign synthetic
  zeros: `1+abs(sin(x)+0.2)` keeps its corner at y=1. Corner slopes are unavailable.
- Retain vertical midpoint checks below a horizontal pixel for absolute-value
  expressions. Adding a corner after accepting a parent chord was insufficient:
  the original midpoint error could have hidden curvature on a child interval.
- Avoid repeatedly seeding the same root due to floating-point sign residues.
  Ordinary exact uncertainty recovery retains its 128-probe cap; optional abs
  corner/rectified phase recovery can extend the shared total to 512. The same
  five-second/allocation/point budgets and reserves still apply. The measured
  target cases needed at most 254 exact probes.
- A final `abs` around a recognized carrier model maps its conservative bounds
  through absolute value. A single carrier without an additive trend needs a
  full monotone π phase span per column, including numerical margins. General
  sums retain the full 2π carrier requirement. Bands are approximate ranges,
  not fabricated point connections or exact envelopes.
- Normalize real screen `sqrt(u^2)`/postfix-square bytecode to the same handling.
  Inner domain guards remain, and non-real/uncertain samples are classified
  through the unchanged original AST. Calculator evaluation is unchanged.

The library also now contains an editor for the selected complete expression.
Typing, selecting and inserting functions update that row while the library
stays open. Enter/Plot closes it and draws; other rows remain unchanged.

## Independent measurements

The new `absolute` suite compares 22 target cases at desktop/mobile profiles and
six existing controls, using 8193 uniform/offset reference positions plus known
analytic zero/feature coordinates. Three repetitions across two binaries give
**168 HTTP requests, zero HTTP errors and zero detected pole/jump crossings**.
All 22 target cases have full checked coverage and `limited:false`; the largest
connected-curve error is **0.642 pixels**, and the largest point count is 6345.
Controls retain their previous geometry, including intentionally limited poles.

Selected local HTTP medians include request, serialization and parsing:

| Desktop case | Before → after (ms) | Checked connected error before → after (px) | Points / bands after |
| --- | ---: | ---: | ---: |
| `abs(x*sin(x))`, ±100 | 23.12 → 19.42 | 13.904 → 0.559 | 1797 / 0 |
| Screenshot-like −340..3890 | 63.66 → 5.57 | 193.332 → range representation | 33 / 930 |
| Shifted affine amplitude/phase | 34.01 → 75.79 | 50.208 → 0.642 | 5053 / 0 |
| `sqrt((x*sin(x))^2)`, ±10 | 6.52 → 5.27 | 7.701 → 0.255 | 229 / 0 |

The screenshot-like response shrank from 384579 to 57625 bytes. Its ranges all
have lower bound zero; their upper bounds enclose independent evaluated values.
Some resolved cases now spend more work to draw accurately. Medians are local
measurements, not universal latency promises. Full data: [CSV](GRAPH_ABSOLUTE_HTTP_2026_10_16.csv).

Both binaries use the same MSVC Release configuration. Before `b0c38cc` SHA256:
`4409dca70cb28f311ad6a6368d6842ad54f8d0a791f1a7ea451690052a21ab91`.
Candidate SHA256:
`62fcf19c831bfc0973f1c8fbeb77b49e27edc0043d92703f012646dd82f52c8c`.

```powershell
node scripts/benchmark_graph_audit.cjs --suite absolute --server build/release-audit-2026-10-12/tests/Release/numforge_web.exe --before build/server-b0c38cc/numforge_web.exe --repeats 3 --output build/graph-absolute-audit-2026-10-16.json --csv docs/benchmarks/GRAPH_ABSOLUTE_HTTP_2026_10_16.csv
```

## Validation and limits

Five native CTest groups passed on both MSVC and GCC: graph, application
expression, web API, web UI and web remote. Tests cover actual cusp values,
domain exclusions, complex-intermediate classification, dense bounds and shared
deadline/output atomicity, alongside existing allocation-failure checks.
The final browser runs passed 40 Chromium tests and 34 Firefox/WebKit tests,
covering resolved cusps, dense ranges, complex guards, library editing/selection,
viewport fit, multiple curves and existing smooth/subpixel behavior. Mobile
library-editor and wide absolute-range screenshots were visually inspected.

Finite probes do not prove arbitrary continuity or discover every possible
oscillation. Unsupported compositions retain ordinary/precision sampling and
finite limits. Dense ranges deliberately do not provide invented point values
for coordinate inspection. Physical-device interaction remains a separate check.
