# Four-function browser graph benchmark — 2026-10-13

Visibility toggles are implemented and regression-tested. In this measurement,
the added controls did not materially change the existing initial-render or camera
timings. Warm toggles made zero graph HTTP requests in all twelve measured after
runs. The dense scenario reveals a pre-existing sample-budget limitation:
`sin(1000x)` has no accepted stroke in the initial view, then reappears after zoom.

## Method

- Baseline: local commit `8ee7e5a`, deployed MSVC Release executable.
- Candidate: the visibility change built with MSVC Release, warnings as errors.
- Headless Chromium on this Windows PC, desktop 1280×900 and mobile layout
  390×844. These are PC measurements, not physical phone measurements.
- Three fresh-page repeats per version/layout/scenario: 24 runs total.
- Both axes initially −10..10; initial sampling also prepares a larger buffer.
- Each run plots four functions, expands the canvas, drags horizontally in eight
  steps, sends six zoom-in wheel events and waits for work to settle.
- The candidate then hides/shows one completed function through the legend.
- A first-frame value includes Playwright click/scroll overhead and waits until
  initial foreground sampling completes. It does not imply every segment is
  resolved. Input-to-frame latency uses browser event timestamps and rAF.
- Settled times include a fixed 350 ms quiet interval and optional background work.
- No intentional CPU throttling, garbage collection or concurrent test load.

Mixed functions: `sin(33x)`, `cos(17x)`, `exp(-x*x/4)`, `x^3-3x`.

Dense/domain-sensitive functions: `sin(1000x)`, `tan(3x)`, `1/(x-0.1)^2`,
`sqrt(abs(x))*sin(33x)`.

## Initial render and interaction

Values below are medians of three runs. The interaction columns are medians of
each run’s 95th-percentile event-to-frame delay for the candidate.

| Layout / scenario | First frame before (ms) | First frame after (ms) | Pan event → frame (ms) | Zoom event → frame (ms) | Initial received data (KiB) |
|---|---:|---:|---:|---:|---:|
| desktop / mixed | 171.8 | 168.1 | 17.6 | 30.0 | 905.8 |
| desktop / dense-and-poles | 412.6 | 412.4 | 18.2 | 30.4 | 1419.9 |
| mobile-layout / mixed | 129.6 | 129.3 | 17.5 | 29.4 | 572.8 |
| mobile-layout / dense-and-poles | 412.9 | 397.2 | 19.0 | 30.1 | 1411.7 |

Differences of a few milliseconds across three runs are insufficient evidence
of an arithmetic speedup. The implementation changes UI visibility and scheduling.

## Requests and efficiency

- Initial mixed views used eight graph requests: four foreground ranges plus
  next-zoom preparation. Dense views used four and stopped further preparation
  where refinement remained limited. Counts were the same before and after.
- Expanded views used 7–11 requests; the six-event zoom burst used 12–20, including
  refinement and preparation. Only one graph HTTP request was active at a time.
- Cached drag used zero requests in all dense/mobile runs. Desktop mixed drag
  needed ten requests in two baseline and all three candidate runs; one baseline
  run used zero. Boundary/density/cache scheduling makes this a variable path.
- Warm hide/show used zero requests in all twelve candidate runs, preserved
  geometry and colors, and repainted in roughly 28–33 ms from each button event.
  The two-click end-to-end toggle measure is 111–121 ms and includes test overhead.
- No ≥50 ms browser long tasks were recorded in the 24 runs. The largest sampled
  animation-frame interval was about 33.4 ms; this run did not demonstrate stalls
  approaching the five-second request limit.

The sampled JavaScript heap includes the entire page and unreclaimed temporary
objects; no GC was forced. Candidate median samples by layout/scenario were:
- desktop / mixed: 7.60 MiB.
- desktop / dense-and-poles: 11.34 MiB.
- mobile-layout / mixed: 14.71 MiB.
- mobile-layout / dense-and-poles: 28.43 MiB.

This is separate from the cache’s existing 8 MiB conservative weight per function
(32 MiB total). Received/estimated response weights across a run are cumulative
traffic and cannot be interpreted as live cache occupancy.

## Remaining limitation and concrete next work

The dense initial view produces three SVG curve layers rather than four. There
are no HTTP/evaluation errors; the sampler conservatively leaves unresolved edges
unconnected. `sin(1000x)` reaches 8192 samples and has no accepted initial stroke.
The same behavior occurs in the baseline. Both layouts have four layers after
zoom, with one function still reporting unresolved visible detail.

This is a sampling/resolution issue rather than a five-second computation timeout.
Raising the global point cap would increase traffic, decoding, memory and drawing
work without making thousands of cycles fit faithfully into a few hundred pixels.

Suggested follow-up, in order:

1. Give the visible interval its own refinement priority and budget before the
   surrounding buffer. Prepare distant/next-view tiles with less work when several
   functions compete for cache capacity.
2. Add an explicit dense-oscillation representation/indicator at pixel resolution,
   while retaining pole/domain breaks. Do not connect uncertain points blindly.
3. Investigate the 12–20 requests after six wheel events: reuse prepared resolution
   levels, postpone additional neighbor preparation while the view keeps changing
   and measure foreground versus speculative traffic separately.
4. Make the status count distinguish sampled functions from functions with accepted
   strokes; valid numeric samples alone do not guarantee a displayed curve.
5. Repeat interactive benchmarks on a physical Android/iOS device and over the
   public tunnel; localhost on a desktop PC excludes mobile CPU and WAN latency.

## Verification and reproduction

- 87 graph/guide browser tests passed on the final implementation.
- Web API, remote-start and Node UI CTest checks passed; MSVC Release build passed.
- Visibility regressions cover both languages/layouts, same-view zero-request
  toggles, cached camera movement, hidden edits, cancellation, persistence,
  keyboard focus and expanded-graph legend controls.

```powershell
node scripts/benchmark_graph_multi.cjs --before build/server-8ee7e5a/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-multi-benchmark-2026-10-13.json
```

Raw local observations: `build/graph-multi-benchmark-2026-10-13.json`. The executable
paths and generated build data are local artifacts. The script is tracked.
