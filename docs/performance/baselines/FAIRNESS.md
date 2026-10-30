# Bounded literal arithmetic under compute saturation

30 October 2026, Windows x64, Ryzen 7 7435HS, MSVC 19.51 Release
(`/O2 /Ob2 /DNDEBUG`), optional allocation statistics off and test hooks on.
Baseline checkout `8a039de` already includes native limb multiplication
`04ac1e1`; optimized scheduling is `2c66800`. This comparison isolates the later scheduling change; it must not be
combined with older server results using slower arithmetic.

## Scheduling policy

The configured general workers keep their FIFO queue and concurrent calculation
capacity. One additional worker may take the first eligible literal-arithmetic
job from later in the queue. Eligibility is a bounded, allocation-free hint,
not another evaluator: the ordinary handler still parses and executes once.

Eligible requests use POST `/api/evaluate`, including legacy caching and live
preview/commit, with Cartesian form and 0–100 output places. The body has at most
192 ASCII bytes, 96 digits and 32 `+ - * /` operators, with literal decimals,
parentheses and whitespace. Exponent notation, powers, factorials, functions,
variables/ans, assignments, units, random calls and other settings retain normal
general-worker service. Invalid expressions in the small alphabet still return
their normal parser/domain errors. These are scheduling bounds, not API limits.

Both kinds of active owner share the original connection bound: configured
general workers plus queue reserve, default 36 and maximum 80. There is no extra
admission slot. Queue expiry, receive/send deadlines, revision checks, session
leases, retention and shutdown ownership remain in force. A busy session can
still consume up to 250 ms of lease waiting; slow response readers and arbitrary
expensive expressions are not universally isolated. See
[hosting](../../guides/HOSTING.md#capacity-and-state) and
[concurrency ownership](../../design/ARCHITECTURE.md#concurrent-server-prerequisites).

## Fixed arrivals

Each fresh server receives four uncached `246000!` requests. Starting 100 ms
later, independent light requests are scheduled every 25 ms, without waiting
for responses or retrying failures. Each factorial's scientific result and every
light result are verified. Legacy runs use 80 separate IDs; live runs start 60
distinct sessions before measurement and issue one preview/commit at revision 2
per session. Post-load checks verify revisions, empty preview history, committed
answers and idempotent replay without another history entry. Setup, state/replay
checks and memory sampling are outside the timed arrival window.

Five immediate before/after pairs per main profile, reversed order on even
pairs; three pairs per control. Four general workers and queue reserve 64 unless
specified. Both versions run BelowNormal. No own concurrent build/test/other
benchmark, affinity or frequency lock; unrelated host activity is uncontrolled.
Driver scheduling lag is retained separately. All 5,126 timed requests across
the selected main/control runs succeed and pass their mathematical checks.
Numbers below are ranges of per-run nearest-rank p95, not a pooled percentile.

| Light profile | Verified light requests per version | Before p95, ms | After p95, ms |
| --- | ---: | ---: | ---: |
| Legacy `2+2` | 400/400 | 607.16–679.31 | 1.86–2.37 |
| Live preview `2+2` | 300/300 | 628.07–662.81 | 1.88–2.26 |
| Live commit `2+2` | 300/300 | 600.07–657.26 | 1.84–2.28 |
| Default queue reserve 32, legacy | 240/240 | 588.29–621.24 | 1.97–2.25 |
| No heavy work, legacy | 240/240 | 1.95–2.22 | 1.92–2.47 |
| Function `sin(0)` | 240/240 | 611.79–656.70 | 680.15–699.58 |
| Power `2^2` | 240/240 | 590.82–651.36 | 609.60–676.72 |
| `2+2`, output precision 101 | 240/240 | 600.37–656.12 | 640.81–644.63 |

The function/power/precision controls demonstrate the conservative boundary;
they are not accelerated by the reserve. Their apparent slowdowns and all
driver/resource observations remain in the recordings. This is an improvement
to selected short-request latency, not a general arithmetic-throughput result.

### Cost and equal-thread control

The default comparison adds one OS thread and its stack/storage. Median initial
legacy working set is 8.781 / 8.809 MiB; private bytes are 11.441 / 11.488 MiB.
These process observations do not establish a portable per-thread memory cost.
Legacy lifetime peak working-set ranges are 20.74–21.41 / 21.42–34.49 MiB.
Live-preview peaks are 28.92–29.84 / 29.31–30.27 MiB. Transient peaks vary;
this change does not claim lower peak RAM.

Large calculations do not become faster. Across the five main runs, medians of
the last factorial completion are 858 / 848 ms for legacy, 836 / 913 ms for
preview and 827 / 893 ms for commit. The latter two are about 9% and 8% slower;
their ranges overlap, and host frequency/load were not controlled. Retain that
cost when evaluating this policy rather than presenting short-request gains as
a universal speedup.

A separate three-pair control holds total threads and admission equal. Baseline
uses five general workers and queue reserve 63; the new policy uses four general
plus one literal worker and reserve 64. Both admit at most 68 connections and
include one receiver. Five heavy clients occupy all baseline general workers.
All 255 requests per version pass. Light p95 is 635.42–668.07 / 1.87–2.06 ms,
while the final factorial completes in 873.95–907.24 / 1572.81–1597.74 ms. The
reserved worker stays available for eligible arithmetic; the fifth heavy job
waits for a general worker. This is an explicit latency/throughput tradeoff,
not extra unrestricted heavy-computation capacity.

## Other load and correctness checks

Three complete alternating mixed-corpus pairs contain nine fresh-server runs
per client count. At 60 clients each version verifies all 3,240 mixed requests
and 540 session reads. Mixed p95 ranges are 65.81–141.62 / 55.06–80.16 ms;
session-read p95 is 34.14–71.61 / 30.41–37.44 ms. Ranges overlap, so no general
mixed-load speedup is inferred from these controls.

Four incomplete peers plus 80 fixed-arrival additions pass in all three runs
per version. Light p95 remains 17.23–17.81 / 17.24–17.44 ms: the receiver's
Windows poll/scheduling floor remains. Fully occupying all 68 admission slots
still rejects all 180 overload probes per version with transport resets, then
recovers correctly. Those rejections are preserved as failures, not successful
calculation capacity.

Deterministic worker tests cover eligibility boundaries, invalid arithmetic,
queue wraparound and bypass, all 16 general workers occupied, shared admission
with both kinds of active owner, original deadlines, queue expiry, fragmented
inputs, 80 pending sockets and shutdown joining both worker kinds. Linux Release
builds successfully; all 13 worker tests also pass against production libraries
built with AddressSanitizer/UndefinedBehaviorSanitizer, including leak detection,
inside an isolated container. All 106 selected Chromium calculator/session/
variable/graph-navigation tests pass. These are not physical-device, WAN or
full C/Django/PostgreSQL stack capacity measurements.

The initial MSVC full-suite run passed 84/85: the remote lifecycle check failed
at a PID-only stopped-process assertion. An isolated unchanged repeat passed.
The check now compares executable path/start time and excludes exited processes,
as the production ownership helper does; PID reuse alone cannot identify an
old owner. Initial diagnostic and retry logs are retained rather than reporting
that first run as green. The GCC ordinary full suite passes all 85 tests; all
84 other MSVC tests pass, and the corrected remote lifecycle test passes in
both builds. This covers all 85 test cases in each final build without hiding
the initial failure. Documentation links/navigation and all six documentation
tests pass as well.

## Reproduction and records

Use an ordinary Release server. For example in PowerShell:

```powershell
$env:NUMFORGE_QUEUE_BENCH_HEAVY_CLIENTS='4'
$env:NUMFORGE_QUEUE_BENCH_CAPACITY='64'
$env:NUMFORGE_QUEUE_BENCH_ACTION='preview'
node benchmarks/application/http_queue_benchmark.js build/dev/Release/numforge_web.exe --arrival-load
```

`NUMFORGE_QUEUE_BENCH_ACTION` accepts `legacy` (default), `preview` or `commit`.
`NUMFORGE_QUEUE_BENCH_LIGHT_KIND` accepts `literal` (default), `power`, `function`
or `precision`. Live profiles default to 60 distinct sessions and enforce the
64-session bound; legacy defaults to 80 requests. Existing arrival-count/rate,
heavy-client, worker and queue environment settings still apply. `--quick`
checks the driver with eight light calls and small factorials; it does not
simulate sustained saturation. Run `--load` and `--slow-clients` separately.

Raw ignored records: `build/performance-fairness/selected-metadata.json`,
`selected-summary.json`, `report-summary.json`, `selected-*.jsonl`,
`control-*.jsonl`, separate stderr, build/native/browser/Linux-sanitizer logs.
Metadata records both binary hashes, relevant source/driver hashes, compilation
settings and pair order. Exploratory `before-*` and `quick-*` runs are not the
selected comparison. No running pilot was replaced by these isolated tests.
