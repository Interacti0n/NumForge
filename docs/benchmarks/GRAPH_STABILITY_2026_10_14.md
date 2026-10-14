# Sustained graph interactions and cache lifetime — 2026-10-14

Inverse wheel zooms could generate a microscopic missing cache interval and
replace a valid curve with a coordinate-range error. Coverage and resolution
comparisons now account for floating-point roundoff. The sustained diagnostic
checks cache replacement, cancellation and collection after navigation in
addition to the existing mathematical graph regressions.

## Reproduced bug and fix

With four functions, move the initial x camera eight times to the right, then
apply three wheel zoom-ins and three inverse zoom-outs. The resulting x bounds
can be `22 .. 42.00000000000001`, although the prepared tile ends at 42. Exact
cache comparisons requested the remaining approximately 7e-15 interval with
at least 320 samples. These cannot all have distinguishable browser coordinates,
so projection validation rejected the response and removed a displayed curve.

Before: deployed MSVC Release `a68b0cd`, failure in the first sustained desktop
cycle. The new desktop wheel regression also fails against this executable.
After: the corrected candidate passes the same regression in desktop and mobile
layouts. Coverage epsilon is the larger of 1e-12 of the view span and eight
times the coordinate magnitude times machine epsilon. Resolution comparisons
allow sixteen arithmetic epsilons.
Both are far below a visible pixel within the permitted zoom range. Tiny
partition tails are omitted from sampling; genuinely missing coverage still
uses coarse/refined requests. Accepted C connection flags and actual boundary
samples continue to control joins. No numerical evaluator or API schema changes.

## Workload and measurements

The script [benchmark_graph_stress.cjs](../../scripts/benchmark_graph_stress.cjs)
uses its own loopback server, Chromium and a test-only routed copy of graph.js
that exposes private cache/queue counters. It does not ship a production debug
API or evaluate graph expressions in JavaScript.

Three layouts: desktop 1280×900, touch portrait 390×844, touch landscape 844×390.
Two four-function cases:

- Mixed: `sin(33x)`, `sin(1000x)`, `cos(17x)`, `x/5`.
- Poles/domains: `tan(3x)`, `1/(x-0.1)^2`, `sin(1000x)`, `sqrt(abs(x))*sin(33x)`.

Each case repeats sixteen reset/pan/inverse-zoom/warm-hide-show cycles. Groups
of four visit x ranges 22..42, 34..54, 46..66 and 58..78 before optional pinch.
Every fourth cycle expands/collapses, deliberately holds a response, changes
the expression and cancels that response before restoring the original function.
Touch layouts also pinch. Six graph→Units→graph SPA round trips follow each case.
The full matrix gives 96 interaction cycles, 576 wheel events, 24 held-response
cancellations, 16 native touch pinches and 36 navigation round trips.

Assertions check no graph/page errors or nonfinite SVG projection, settled
transport/queues, zero requests for settled visibility toggles, and per-function
bounds of 12 cached ranges, 12288 point-or-band records, estimated 8 MiB and
48 preparation attempts. Retained cache plus active geometry is checked against
20480 unique point-or-band objects per function. The latter permits a current
8192-record response in addition to the bounded cache; it is not a public API
limit or byte-accurate heap bound.

Frame intervals/long tasks exclude cache-stat sampling and forced GC, but include
headless automation and idle frames, so they are diagnostic measurements rather
than isolated interaction latency. Heap is the whole page after forced GC.
Inspector console roots are cleared first: an initial apparent retained graph
was traced in a heap snapshot to a DevTools console root, not an application
reference. WeakRef checks then verify collection of old graph roots. DOM and
listener counts and the owned C process working set are recorded separately.

## Results

Results are recorded in [the six-case summary](graph_stability_2026_10_14.csv).

| Layout / case | HTTP requests through cycle 16 | Peak cached records / function | Initial → peak → final JS heap, MiB | Frame p95 / max, ms |
| --- | ---: | ---: | ---: | ---: |
| Desktop mixed | 106 | 11982 | 5.836 → 7.135 → 6.984 | 16.8 / 33.4 |
| Desktop poles | 136 | 11976 | 6.104 → 8.454 → 7.890 | 16.8 / 33.4 |
| Portrait mixed | 108 | 12255 | 4.460 → 6.713 → 6.248 | 16.7 / 33.3 |
| Portrait poles | 158 | 11845 | 5.931 → 8.073 → 6.434 | 16.7 / 33.4 |
| Landscape mixed | 149 | 12227 | 5.851 → 7.415 → 6.940 | 16.8 / 33.4 |
| Landscape poles | 178 | 12227 | 6.127 → 8.451 → 6.713 | 16.8 / 33.4 |

All 96 cycles and 36 navigation round trips pass. Each case records four
deliberately aborted requests. Settled hide/show pairs make no sampling requests.
The injected native pinch must actually reduce both camera spans to 0.625 of
their previous width; off-screen gesture injection is not counted as a pinch.
There are no recorded ≥50 ms long tasks in the measured interaction windows.
These windows contain idle frames too, so the p95 is not a direct input-latency
benchmark. Occasional 33 ms frames remain.

Cache reaches the 12-range bound and 12255 of 12288 permitted records; estimated
weight peaks at 4.250 MiB per function, below 8 MiB. The greatest observed
retained cache/active geometry is 12255 unique records per function. Heap peaks
at 8.454 MiB across the entire page. Each Units visit has exactly 878 DOM nodes
on desktop or 869 in touch layouts, and 73 event listeners. No prior graph root
survives GC across any of the 36 visits. Units heap ranges from 2.942 to 3.149
MiB over all runs, with small within-run warming; the test does not establish
constant total browser memory. Owned-server working sets finish at 9.211–9.809
MiB, compared with 8.848–9.527 MiB initially.

The C server and sampler limits are unchanged. Repeated pole views make more
requests than warm simple views because detailed tiles compete for the bounded
cache; the diagnostic does not claim every navigation or zoom is request-free.
Heap, SVG/native DOM storage and OS working set are different measurements.

Raw local artifacts are ignored: `build/graph-stress-initial-2026-10-14.json`
for the original failure, successful desktop rows in
`build/graph-stress-2026-10-14.json`, verified touch rows in
`build/graph-stress-touch-portrait-2026-10-14.json` and
`build/graph-stress-touch-landscape-2026-10-14.json`, combined as
`build/graph-stress-final-2026-10-14.json`. Early mobile harness trials with
off-screen pinches are excluded. All accepted runs use the same corrected MSVC
Release executable, on this PC without simultaneous compiler/browser workloads.

## Validation and remaining work

119 graph/guide browser tests pass, including four new wheel-roundtrip and
queued-navigation regressions. MSVC and GCC build the embedded web assets;
GCC graph/application/expression/session/web API tests and MSVC remote/UI asset
checks pass. Existing deep-zoom, zero ticks, pole/domain and actual tile-boundary
tests remain unchanged and pass.

These are PC Chromium measurements with emulated mobile layouts. A physical
phone, real browser backgrounding/suspension, much longer sessions and other
browsers still need testing. Neither bounded cache accounting nor a finite
forced-GC run proves absence of every memory leak. Sampling/envelopes keep their
existing approximation and continuity limits. Next: physical-phone validation,
clearer per-function partial-result messages, then saved/exported configurations.
