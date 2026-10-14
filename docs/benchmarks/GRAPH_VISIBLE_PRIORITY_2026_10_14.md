# Visible-first graph sampling — 2026-10-14

The browser now samples the visible interval before preparing off-screen data.
All visible functions finish foreground work first. The final comparison showed
13–20% lower median first-frame times on this PC, complete initial coverage for
the dense sqrt/abs/sine case, and fewer requests after a six-event zoom burst.
Background preparation increases initial cumulative traffic for ordinary views;
this remains a concrete target for the next efficiency pass.

## Implementation

- First requests use the actual camera interval, −10..10 by default, rather than
  spending the foreground point budget on −30..30. Existing C APIs, point caps,
  acceptance checks, time and allocation limits are unchanged.
- After a quick resolved view, inexpensive ranges may prepare one combined
  three-times-wide buffer. More than 2730 foreground points skips that buffer.
- Next-zoom detail remains available; optional batches have at most two sequential
  requests per function, retain cache/memory bounds and foreground queue priority.
- Cached geometry redraws through one animation-frame callback, including initial
  clearing and successive foreground results. States without accepted visible
  strokes skip path construction. Coarse previews still paint before refinement.
- The multi-function count uses accepted connected visible edges rather than
  treating every real sample set as a rendered function.

## Method

Baseline is the deployed MSVC Release executable from commit `5ac4aba`. Candidate
is the final visible-first/coalesced-rendering change, also MSVC Release. Headless
Chromium ran at 1280×900 and 390×844 on this Windows PC. A mobile viewport is not
a physical phone measurement. Three fresh-page repeats per version/layout/case
give 24 runs. A preceding diagnostic run exposed a dense desktop slowdown;
coalescing drawing and skipping empty strokes addressed it before this final run.

Cases are the same as the [previous four-function audit](../project/GRAPH_MULTI_BENCHMARK_2026_10_13.md):

- Mixed: `sin(33x)`, `cos(17x)`, `exp(-x*x/4)`, `x^3-3x`.
- Dense/poles: `sin(1000x)`, `tan(3x)`, `1/(x-0.1)^2`,
  `sqrt(abs(x))*sin(33x)`.

Each run plots, expands the canvas, drags, sends six wheel zoom events and then
hides/shows a cached function. First-frame times include test click/scroll
overhead and wait for all initial foreground results; they do not imply every
curve is resolved. Full settling includes a 350 ms network-quiet interval.
Foreground traffic is the first four requests in this fixed four-function
workload; cumulative initial traffic also includes optional preparation.

## Final median results

| Layout / case | First frame before → after (ms) | Foreground KiB before → after | Cumulative initial KiB before → after | Initial requests before → after | Zoom requests before → after |
|---|---:|---:|---:|---:|---:|
| desktop / mixed | 131.9 → 114.6 | 554.8 → 240.6 | 905.8 → 1146.3 | 8 → 12 | 20 → 15 |
| desktop / dense-and-poles | 328.6 → 263.0 | 1419.9 → 1294.2 | 1419.9 → 1501.4 | 4 → 5 | 12 → 11 |
| mobile-layout / mixed | 113.8 → 97.7 | 330.3 → 132.4 | 572.8 → 705.1 | 8 → 12 | 20 → 15 |
| mobile-layout / dense-and-poles | 314.5 → 265.2 | 1411.7 → 1276.6 | 1411.7 → 1483.8 | 4 → 5 | 17 → 11 |

The common mixed views transfer roughly 57–60% less data before the first result,
but foreground-plus-preparation traffic grows about 23–27%. Dense scenarios grow
about 5–6% in cumulative initial traffic while dropping one unresolved-function
warning on desktop. Warm visibility toggles make zero requests in all after runs.
No ≥50 ms long tasks were recorded in the final candidate runs. Median p95
input-to-frame delays were about 17–20 ms for drag and 30–31 ms for zoom.

Three repeats are diagnostic evidence, not a guarantee for arbitrary functions,
mobile hardware or a remote tunnel. Full request latency still includes transport
and each range independently retains its five-second server budget.

## Coverage and safety

A direct controlled screen-sampler comparison for `sqrt(abs(x))*sin(33x)` at
800×440 gives:

| Sampling range | Points | Limited | Accepted coverage inside −10..10 |
|---|---:|---|---:|
| Old foreground −30..30, width 2048 | 8192 | true | about 99.85% |
| New foreground −10..10, width 800 | 4097 | false | 100% |

The browser regression also checks every emitted edge is accepted and independently
checks 64 representative ordinate values against `sqrt(abs(x))*sin(33x)`. Pole
regressions ensure no accepted SVG segment crosses the reciprocal discontinuity.

`sin(1000x)` still fills 8192 points without an accepted initial stroke. The dense
benchmark therefore initially has three layers in both versions; zoom later gives
four. Visible-first allocation improves useful coverage but cannot solve every
subpixel oscillation. The sampler continues to leave uncertainty unconnected.

## Verification and next work

- 103 graph/guide browser tests passed on the final coalesced implementation.
- MSVC Release warnings-as-errors build and web API/remote/UI checks passed.
- Priority tests prove all foreground rows finish before buffers start; holding a
  buffer does not delay or replace the displayed curve. Existing cache, touch,
  inspector, seam, extrema, cancellation and gap regressions pass.

Next: reduce cumulative preparation traffic, reuse prepared resolution levels
during repeated zoom, and introduce a deliberate dense-oscillation representation
with strict domain/pole handling. Increasing point caps alone would worsen
decoding/drawing/transfer costs and does not make all cycles fit into pixels.

```powershell
node scripts/benchmark_graph_multi.cjs --before build/server-5ac4aba/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-visible-priority-final-2026-10-14.json
```

Local raw observations: `build/graph-visible-priority-final-2026-10-14.json`.
Tracked medians: [CSV](graph_visible_priority_2026_10_14.csv).
