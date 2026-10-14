# Graph cache and preparation efficiency — 2026-10-14

Accepted cached curves now supply the preview for covered, resolved views, so
zoom refinement skips a duplicate coarse calculation. Final measurements show
17–27% fewer requests and 13–20% fewer response bytes after six zoom events.
Initial preparation is unchanged; this pass improves subsequent interaction.

## Implementation

- Preserve useful refined resolution levels of the same interval. Prune dominated
  levels when a refined, non-limited entry of the same evaluation kind covers
  that exact interval with at least their horizontal and vertical density.
  Coarse results cannot discard refined levels. Existing bounded eviction still applies.
- Select adequately refined cached tiles before narrower but insufficient ones.
  Keep independent accepted-edge, classification and unresolved flags; no new
  connections or mathematical evaluations are produced by these cache rules.
- When cached coverage is complete, contains a visible accepted stroke and has no
  visible unresolved detail, request refinement directly. Missing or visibly
  unresolved views retain coarse-then-refined sampling and cancellation behavior.
- Check prepared coverage across multiple tiles. Sample one missing preparation
  tail alone; two missing neighbors can share one bounded buffer request.
- Keep normal next-zoom preparation across the current x range at up to twice
  the width and half the y span, preserving preparation for canvas expansion.
  In the expanded canvas, prepare only the central half of x at the current width
  and half the y span. A completed batch is not restarted by an adequately cached
  resize. Attempt keys include requested density; at most two background requests
  run per batch with foreground priority.

An initial experiment prepared only the next central view in all layouts. It
reduced initial bytes but introduced extra work on expansion; it was replaced
before the final comparison. That diagnostic is not the released behavior.

## Method

Before: MSVC Release executable from commit `77d618a`. After: final candidate
from this cache/preparation pass, same compiler/build settings. The existing
`scripts/benchmark_graph_multi.cjs` runs real headless Chromium at 1280×900 and
390×844 on this PC. The mobile layout is not a physical phone. Three fresh-page
repeats per version/layout/case give 24 runs. Earlier diagnostics ran alongside
regression tests; the final browser benchmark ran after those tests completed.

Mixed functions: `sin(33x)`, `cos(17x)`, `exp(-x*x/4)`, `x^3-3x`.
Dense/pole functions: `sin(1000x)`, `tan(3x)`, `1/(x-0.1)^2`,
`sqrt(abs(x))*sin(33x)`.

Each run plots, expands, drags, sends six wheel zoom events, then (candidate only)
hides/shows one warmed function. Settling includes 350 ms network quiet. Timings
include automation/event overhead; each C request retains its own five-second
budget. Transfer counts include speculative work. Medians are diagnostics,
not universal timing promises or CI performance thresholds.

## Final medians

| Layout / case | Zoom requests before → after | Zoom KiB before → after | Pan requests before → after | Initial KiB before → after | Total KiB before → after |
|---|---:|---:|---:|---:|---:|
| desktop / mixed | 15 → 11 | 685.7 → 552.0 | 0 → 0 | 1146.3 → 1146.3 | 1832.0 → 1698.3 |
| desktop / dense-and-poles | 11 → 9 | 2457.0 → 2081.6 | 0 → 0 | 1501.4 → 1501.4 | 5435.8 → 4929.2 |
| mobile-layout / mixed | 15 → 11 | 547.3 → 436.0 | 14 → 10 | 705.1 → 705.1 | 2053.4 → 1707.7 |
| mobile-layout / dense-and-poles | 12 → 10 | 1984.8 → 1733.5 | 0 → 0 | 1483.8 → 1483.8 | 4667.7 → 4381.2 |

Total covers plot, expansion, pan and zoom; zero-byte candidate toggles do not
change it. Median totals are computed per whole run rather than summing separate
phase medians. Overall transfer drops about 6–17%. Mixed mobile pan bytes fall
from 801.0 to 566.6 KiB. Cached mixed expansion remains zero requests in both
layouts. Dense expansion drops from six to five requests in both layouts.

First-frame medians before → after: desktop mixed 130.1 → 132.5 ms, desktop dense
395.9 → 397.7 ms, mobile mixed 96.7 → 96.4 ms, mobile dense 382.2 → 387.4 ms.
This pass does not demonstrate a first-frame improvement. Candidate frame p95 is
about 16.7–16.8 ms; input-to-frame p95 is 17.6–22.3 ms for pan and 29.1–34.9 ms
for zoom. Desktop mixed zoom input p95 rises from 29.8 to 34.9 ms in these three
runs; other cases vary in both directions. No ≥50 ms long tasks were recorded
in either final version. These results support reduced work, not a guarantee
that every individual interaction becomes faster.

The raw whole-page JavaScript heap snapshot has no forced GC and includes the
candidate's extra toggle phase. It is not a retained-cache/leak measurement:
medians range from 15.3–27.1 MiB before and 15.6–29.7 MiB after. Cache caps remain
12 ranges / 12288 points / estimated 8 MiB per function; eviction regressions pass.
Longer memory sessions and physical-phone checks remain planned separately.

## Correctness and remaining work

- All 107 graph/guide browser tests passed, including four new efficiency tests.
- Prepared three-level zoom/reset revisits for `x*x` make no further graph requests
  in both layouts. Missing coverage still paints coarse data while refinement is
  held; covered cached previews skip coarse work and reject obsolete responses.
- Expanded preparation, cached expansion, real numeric references, poles, domain
  gaps, seams, inspector, touch/pinch, hidden curves and bounded eviction pass.
- MSVC Release warnings-as-errors build and web API/remote/UI CTest checks passed.
- Documentation and the SK/EN web guide describe the final policy. C sampling,
  mathematics, HTTP schemas, connection checks and resource limits are unchanged.

Dense initial views still show three curves, later four after zoom, with the same
unresolved counts before/after. `sin(1000x)` at the default scale still has no
accepted initial stroke. A deliberate dense-oscillation representation is the
next stage; this cache change does not solve subpixel periods or continuity.

```powershell
node scripts/benchmark_graph_multi.cjs --before build/server-77d618a/numforge_web.exe --after build/release-audit-2026-10-12/tests/Release/numforge_web.exe --repeats 3 --output build/graph-cache-efficiency-final-2026-10-14.json
```

Local raw data: `build/graph-cache-efficiency-final-2026-10-14.json`.
Tracked medians: [CSV](graph_cache_efficiency_2026_10_14.csv).
Previous baseline: [visible-first audit](GRAPH_VISIBLE_PRIORITY_2026_10_14.md).
