# Graph view reconciliation — 15 October 2026

After plotting floor(x) and zooming out three times, the release audit found
`Sampling…` after all transport had settled. The step-guard follow-up confirmed
a missing central cached region, despite correct horizontal step strokes.
This change repairs the client cache/lifecycle behavior; C numerical sampling,
HTTP schemas, point/time limits and step/domain acceptance are unchanged.

## Cause and repair

A precision response to a coarse preview request is already refined. The
foreground loop remembered that preview, promoted it to its final result, then
remembered the same object again. Limited results cannot dominate other useful
resolution levels, so both references occupied slots and counted twice against
the bounded cache. Repeated zooms evicted the original central tile. Planned
partial jobs did not include the newly missing centre, leaving pendingView true
after busy had become false. The draw path deliberately skips describing a
pending view, so the loading text never ended.

- Remembering an existing response object is now idempotent. Different useful
  resolution levels keep their existing retention rules.
- After the planned partial jobs, an incomplete cached view can trigger one
  full-view foreground repair. This is bounded, uses the same generation/signal
  and camera, and retains the 12-range/12288-record/8-MiB per-function limits.
- Success clears busy before refreshing the description and scheduling the
  drawing. Failure clears pendingView and preserves the explicit error. A
  failed repair never creates an automatic retry loop.
- Existing generation checks and cancellation also cover the repair. An old
  response after editing/hiding/navigation cannot publish stale data or status.

The repair requests fresh C samples; it does not invent connections, interpolate
across missing tiles or hide unresolved numeric detail.

## Regression and diagnostics

The two original real-server regressions fail on eee0e2f at desktop and mobile
widths with the stuck Sampling message. They then require terminal status,
actual central floor plateaus after every zoom/reset, unique cache entries and
bounded retained records. Three further regressions force partial-tile eviction
using large routed fixtures: one full-view repair succeeds or fails explicitly,
and an edited expression replaces a held repair without stale geometry.

Private cache counters are injected only into routed test assets. Production
graph.js contains no diagnostics endpoint or cache test hook. The sustained
stress harness now includes four precision step functions, verifies unique
entry identities and rejects a settled view that is still pending. Existing
forced-GC/lifecycle and per-function bounds remain checked.

For reproducible browser diagnostics, compare GCC RelWithDebInfo executables
on this PC. Before eee0e2f SHA256:
`f32b1abe2e55e8ec9a37763cdfbf72f343bc9189b46de6676c32db329cefcbce`.
Corrected SHA256:
`4843c347eb93810a42ce739a785a0dcbc3b76b3624f807fe944517c716d5ca9c`.

Run after tests/builds finish, serially:

```powershell
$env:PATH='C:/msys64/ucrt64/bin;'+$env:PATH
node scripts/benchmark_graph_multi.cjs --before build/server-eee0e2f-gcc/numforge_web.exe --after build/release-gcc-ninja22/numforge_web.exe --repeats 1 --output build/graph-reconcile-multi-2026-10-15.json
node scripts/benchmark_graph_stress.cjs --server build/release-gcc-ninja22/numforge_web.exe --case steps --cycles 4 --output build/graph-reconcile-stress-2026-10-15.json
node scripts/benchmark_graph_audit_browser.cjs --server build/release-gcc-ninja22/numforge_web.exe --output build/graph-reconcile-svg-2026-10-15.json
```

Headless Chromium mobile profiles emulate layout/touch; they are not physical
phones. Quiet intervals, instrumentation and forced GC are diagnostic overhead,
not arithmetic timing. One comparison repetition is a smoke measurement, not
statistical evidence of a general performance improvement. Raw JSON and SVG
images stay locally under ignored build/.

## Results

- All **144 graph/guide Playwright tests pass** on the corrected MSVC build.
- API/remote/UI CTest checks pass 3/3 in both MSVC and GCC packaging builds.
  The arithmetic and C graph implementation are unchanged in this step.
- The five new regressions cover the reproduced failure at two widths, repair
  success/failure under forced eviction, and an edit while repair is held.

The separate actual-SVG check now ends at x/y ±80 with the ordinary limited
detail notice, not Sampling. It restores the central region: **160 step
subpaths / 3616 horizontal segments**, versus 140 / 3152 in the prior audit.
Both views contain zero diagonal step segments. At the prior missing central
coordinate x = −0.048828125, the drawn y is −0.9999986874 (about 0.0000031 plot
pixel from −1). This preserves the existing numerical discontinuity gaps rather
than inventing vertical or diagonal bridges. Expansion/Escape, keyboard pan,
reset and syntax recovery also pass; no page errors were recorded.

The before/after four-function smoke measurement retains identical zoom request
counts. Cold first-frame numbers include browser and loopback overhead:

| Layout / scenario | Before → after first visible frame, ms | Before → after zoom requests |
| --- | ---: | ---: |
| Desktop / mixed | 131.8 → 131.1 | 11 → 11 |
| Desktop / dense and poles | 141.9 → 162.8 | 7 → 7 |
| Portrait / mixed | 97.3 → 96.0 | 11 → 11 |
| Portrait / dense and poles | 147.4 → 146.2 | 8 → 8 |

All four functions render in every run without errors. Corrected warm visibility
toggles make zero requests. Zoom input-to-frame p95 is 29.3–33.0 ms including
automation overhead (before: 26.4–31.1 ms). One repetition does not distinguish
noise from small timing effects; this is not a general speedup or 60-fps claim.

Four precision step functions pass four sustained interaction cycles in each
profile, followed by six navigation roundtrips per profile:

| Profile | Maximum ranges per function | Maximum records per function | Maximum estimated cache MiB |
| --- | ---: | ---: | ---: |
| Desktop | 6 | 8280 | 3.52 |
| Portrait touch layout | 12 | 8361 | 3.54 |
| Landscape touch layout | 9 | 9557 | 4.08 |

Across **12 cycles and 18 navigation roundtrips**, all snapshots have unique
cache identities and no settled pendingView. Warm toggles do not recalculate.
Zoom/pan/reset, expansion, touch pinch and cancellation checks pass. Forced-GC
heap rises from roughly 3 MiB initially to 4.5–4.9 MiB with warmed caches;
the configured cache bounds remain intact. After every tested navigation,
zero old graph DOM roots survive the forced-GC/WeakRef check. These finite
checks are useful lifecycle evidence, not a universal memory-leak proof.

Remaining release work is the dense mixed-function policy/limits and
Firefox/WebKit/physical-device validation from the original audit. Sampled
graphs still cannot certify every hidden discontinuity or arbitrarily narrow
feature. No server deployment, push, version bump or release is part of this fix.
