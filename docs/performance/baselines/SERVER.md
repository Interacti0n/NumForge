# Server load validation

These are local Windows observations, not a deployment-capacity guarantee.
The [protocol](../protocols/APPLICATION.md#multi-client-load) defines fixtures,
verification, latency/memory scope and limitations.

## Serial foundation — 21 October 2026

Recorded 21 October 2026 at clean source
`2a977fad48bd90e118ed6680e1e8f7baea21919d`, using the
[multi-client protocol](../protocols/APPLICATION.md#multi-client-load).
Normal Windows x64 MSVC 19.51.36247.0 Release, Node 24.19.0, Ryzen 7 7435HS
(16 logical CPUs), approximately 32 GiB RAM. Driver and server shared the host;
no other build/test ran during recording. This is a local initial baseline,
not Linux/deployment capacity or a speedup claim.

```sh
node benchmarks/application/http_queue_benchmark.js build/apps/Release/numforge_web.exe --load
```

Three independent fresh-server repetitions per row; six mixed requests per client:

| Clients | Verified mixed requests per repetition | Connection refusals | Mixed p95 range, ms | Sessions retained after serial setup |
| --- | --- | --- | --- | --- |
| 10 | 59/60, 59/60, 59/60 | 1, 1, 1 | 13.21–18.93 | 8/10 |
| 30 | 64/180, 65/180, 66/180 | 116, 115, 114 | 36.90–44.00 | 8/30 |
| 60 | 64/360, 63/360, 65/360 | 296, 297, 295 | 73.16–90.95 | 8/60 |

All returned successes passed their mathematical/value checks; the mixed failures
were ECONNREFUSED. Timed concurrent session reads separately recorded expiry and
transport outcomes; serial prechecks confirmed exactly eight retained values in
all nine scenarios. Recovery succeeded. Low failure-inclusive latency must not be
read as successful capacity: refused connections can fail quickly. These are
fresh-connection bursts without think time, not a model of ordinary browsing.

Fresh-process peak resident ranges through recovery were 6.69–6.73 MiB for mixed
work and 7.67–7.70 MiB for sessions. These brief runs do not establish sustained
memory behavior. The unchanged head-of-line CSV mode separately measured idle
2+2 at 1.55–2.20 ms and queued 2+2 at 435.03–444.58 ms behind 100000!;
all factorial outputs were verified. No timing threshold is a pass criterion.

Raw ignored recordings: `build/http-load-baseline.jsonl` and
`build/http-queue-baseline.csv`. JSONL includes the executable SHA-256 and host
metadata. Preserve the same protocol for worker/queue comparisons; the
[shared-state audit](../../design/ARCHITECTURE.md#concurrent-server-prerequisites)
defines prerequisites before adding concurrency.

## Worker pool — 22 October 2026

Normal Windows x64 MSVC Release, same CPU/Node configuration as above. Server
implementation `596d6d7`; clean driver source `8c49bc5` for the final 64-slot
recording. Executable SHA-256:
`b781a85d6bca96d5bd560121eda49912fd7b2726f07c37f4f3420cc23e400a4e`.
No build/test ran alongside measurements. Four workers, three fresh-server
repetitions of the original six-turn mixed workload:

| Queue slots | Clients | Verified mixed requests per repetition | Mixed p95 range, ms | Verified concurrent session reads per repetition |
| --- | --- | --- | --- | --- |
| 32 (default) | 10 | 60/60, 60/60, 60/60 | 13.23–18.15 | 10/10 each |
| 32 | 30 | 180/180, 180/180, 180/180 | 39.59–49.59 | 30/30 each |
| 32 | 60 | 256/360, 252/360, 250/360 | 89.37–108.15 | 36/60 each |
| 64 | 10 | 60/60 each | 13.45–18.34 | 10/10 each |
| 64 | 30 | 180/180 each | 38.39–42.55 | 30/30 each |
| 64 | 60 | 360/360 each | 79.27–100.44 | 60/60 each |

Every serial precheck retained all requested sessions, including 60/60 with
either queue size. The default queue's 60-client burst failures were connection
resets, not lost session values or incorrect mathematics. All scenarios recovered.
The larger queue removed those failures in these runs; it does not increase
worker count or computation budgets. Keep the conservative default, and use
`--workers 4 --queue-capacity 64` when validating this burst profile on a host.

In the separate three-sample head-of-line diagnostic, fast 2+2 behind 100000!
took 2.20–2.49 ms (idle 1.46–3.16 ms); all factorial results were correct.
This demonstrates independent serving in that fixture, not faster factorials.

Raw ignored recordings: `build/http-load-workers.jsonl` (clean `596d6d7`, default
queue), `build/http-load-workers-queue64-final.jsonl` and
`build/http-queue-workers.csv`. They retain the different settings and failures.

### Sustained state and recovery

Clean `8c49bc5`, identical executable, four workers and 64 queue slots. Each
client count ran for three minutes with a 50-ms pause after each response,
alternating commits, isolated reads and mathematical fixtures; every tenth
commit was replayed. All final values/revisions/history counts and recovery
checks passed.

| Clients | Verified / completed requests | p95 range across full windows, ms | Resident memory during final minute, MiB |
| --- | --- | --- | --- |
| 10 | 27,180 / 27,180 | 9.88–11.97 | 10.277–10.293 |
| 30 | 67,768 / 67,768 | 26.56–28.78 | 13.180–13.227 |
| 60 | 114,710 / 114,710 | 43.18–47.83 | 17.508–17.547 |

All 209,658 requests were verified, with no application, transport or verification
failures. Latency ranges use completion windows of at least nine seconds;
short final drain windows are excluded from that range, not from totals. These
are window percentiles, not pooled percentiles. Memory samples stayed in narrow
bands during the final minute; three minutes does not establish leak freedom
or long-term behavior for other expressions. Fresh-process peaks through recovery
were 10.32, 13.25 and 17.60 MiB respectively.

In the separate overload fixture, four incomplete connections occupied the
workers. All 60 new requests received HTTP 503 `server_busy`, with maximum
round-trip latency 1,122.54 ms. After releasing the connections and waiting
250 ms, a fresh 2+2 was correct in 1.45 ms. No crash or restart was needed.
Raw recording: `build/http-sustained-workers-queue64.jsonl`.

Repeat on the deployment host with its proxy, CPU/memory limits and actual
workload. TLS/WAN, open-loop arrivals, arbitrary expensive expressions and
overnight operation are outside these measurements.

## Windows Quick Tunnel preview

On 22 October 2026, the [verified static Windows package](../../project/RELEASE_PACKAGING.md#restart-and-rollback-rehearsal)
from `347dd8d` was served by helper `58ea36c`, with four workers and 64 queue
slots. Requests used the public HTTPS hostname through Cloudflare, from the
hosting PC; this is not independent mobile-network or geographic validation.
One fresh-connection burst per client count verified arithmetic, fractions,
quadratic roots, graph points and sqrt(2) with 1,000-digit precision, followed by
separate session start, commit and isolated-read bursts:

| Clients | Verified requests across four bursts | Mixed p95, ms | Session-read p95, ms |
| --- | --- | --- | --- |
| 10 | 40/40 | 131.19 | 139.08 |
| 30 | 120/120 | 153.19 | 268.18 |
| 60 | 240/240 | 276.83 | 346.80 |

All 400 requests passed without transport or application errors; replay preserved
revision/history counts. Public HTML and five assets loaded, and a different
POST Origin was rejected with 403. Backend restart, an intentionally failing
candidate with automatic rollback, and manual recovery after a simulated backend
crash all retained the same tunnel process/URL. Public mathematical checks passed
after each recovery; old sessions were lost as expected.

Raw ignored evidence: `build/public-tunnel-check.jsonl` and its one-off probe
`build/public-tunnel-check.cjs`. The log also retains an initial malformed-query
probe; the table uses the corrected canonical query order. These short bursts
do not establish sustained proxy capacity, memory stability or uptime.

The subsequent opt-in Windows task was also exercised on this host: its real
one-minute trigger restored a killed backend in 24.16 seconds without changing
the tunnel, and a killed tunnel in 24.12 seconds with a new URL/backend origin.
Public HTML/assets and mathematical fixtures passed after both recoveries.
Disabling registration preserved both running processes, and a delayed scheduled
invocation was ignored; registration was then re-enabled. These are observed
recovery times, not deadlines. Task definitions use the current account name
(`computer\user`), interactive limited privileges and no stored password.
Raw scheduler evidence is in `build/remote-state/startup-check-8766.jsonl` and
`startup-task-8766.xml`. A reboot/login rehearsal and host resource limits remain
open; sustained proxy observations follow below.

### Sustained HTTPS state and memory

Clean driver `62a1b88`, the same live static package, four workers and 64 queue
slots. Each group ran for five minutes, with a one-second pause after each turn,
fresh TLS connections and no automatic retries. Commits, isolated reads,
mathematical fixtures and replay checks share the
[sustained protocol](../protocols/APPLICATION.md#sustained-windows-preview-through-https).
No other build/test ran alongside this recording; the normal Windows supervisor
remained enabled. Both process identities and the public URL stayed unchanged.

| Clients | Verified / completed requests | Full-window p95 range, ms | Backend RSS in final minute, MiB | Tunnel RSS in final minute, MiB |
| --- | --- | --- | --- | --- |
| 10 | 2,910 / 2,910 | 70.95–89.34 | 10.957–10.961 | 31.270–32.504 |
| 30 | 8,732 / 8,732 | 75.68–146.62 | 15.383–15.422 | 32.125–32.609 |
| 60 | 17,424 / 17,424 | 79.15–275.26 | 17.234–17.316 | 32.234–33.176 |

All 29,066 requests passed without application, transport or mathematical
verification errors. Every final value/revision/history count passed, followed
by correct 2+2 recovery checks and release of all 100 test sessions. Latency
ranges use completion windows of at least 29 seconds; shorter final windows
remain in totals. These are window percentiles, not a pooled percentile.
Resident samples stayed in narrow bands in each final minute. At 60 clients,
final-minute private bytes were 11.957–12.043 MiB for the backend and
75.684–76.719 MiB for the tunnel; private bytes are separate from resident memory.

The post-load manual status check collided with a scheduled check's action lock.
The scheduled check reported failure without changing either process. It now
skips a busy lock successfully; isolated tests and a real scheduler collision
passed. Raw measurement: `build/http-sustained-tunnel.jsonl`; scheduler evidence
remains in `build/remote-state/startup-check-8766.jsonl`.

This validates the selected workload on this Windows preview through the public
proxy from the hosting PC. Fifteen minutes does not establish leak freedom,
overnight uptime or capacity for arbitrary expensive expressions. OS resource
limits, independent-network/device checks and reboot/login remain separate.
## Current preview profile

Measured 25 October 2026 on Ryzen 7 7435HS (8 cores/16 logical CPUs), 31.82 GiB
RAM, Windows x64, MSVC 19.51 Release. Embedded server revision `49239c3`;
driver changes add `--mixed-heavy`, Windows CPU/private-memory observations and
BelowNormal priority. The live helper now applies that priority after every start.
Raw local records: `build/http-mixed-heavy-4.jsonl` and
`build/http-mixed-heavy-tunnel.jsonl` (ignored diagnostic outputs).

Profile: 4 workers, queue 64, 10 s cooperative budget, 128 MiB cumulative
allocation requests, 8 MiB single allocation. Two heavy clients each submit two
uncached `246000!+turn*0` requests; the remaining clients each submit two existing
mixed arithmetic/fraction/equation/graph/sqrt(2)-1000 requests. No retries;
correctness is checked. Factorial rounding was independently checked using
Python `math.factorial` and integer division/remainder, without decimal strings.

| Clients | Local verified | Local light max | HTTPS verified | HTTPS light max |
| --- | --- | --- | --- | --- |
| 10 | 20/20 | 24 ms | 20/20 | 322 ms |
| 30 | 60/60 | 65 ms | 60/60 | 249 ms |
| 60 | 120/120 | 159 ms | 120/120 | 684 ms |

Local factorial maxima were 4.22–4.47 s; HTTPS maxima stayed below 4.67 s.
Local peak resident RAM was 17.3 MiB. The already-running HTTPS backend's
lifetime peak was 31.0 MiB (not an isolated interval peak); tunnel lifetime peak
was 40.4 MiB. Backend CPU deltas were approximately 15–17 CPU seconds per
8–9 s scenario, roughly two busy logical CPUs on average. This does not measure
desktop responsiveness, CPU peak, or a universal throughput guarantee.

Local forced overload held all four workers with incomplete connections:
60/60 requests returned `503 server_busy`, followed by correct recovery in
1.8 ms. HTTPS test sessions were released after each scenario. Isolated helper
tests passed start/restart, bad-candidate rollback, process-loss recovery,
identity/lock protection and windowless scheduler construction. Actual Windows
reboot/login remains deferred; this gate is not claimed complete.

Retain 4/64 and BelowNormal for the preview. More clients are plausible when
most requests are short; this test covers only two simultaneous heavy clients,
not 60 factorials. There is no process RAM or hard CPU cap. Reassess if workloads
or concurrency change rather than raising budgets based on this burst alone.

## Fixed arrivals with all workers occupied — 30 October 2026

Windows x64, Ryzen 7 7435HS, GCC 16.2 Release, ordinary uninstrumented backend
containing numeric implementation `3845eca`, BelowNormal process priority. This
is a separate workload from the historical six-turn and sustained profiles.
Each row starts a fresh owned loopback server. Heavy clients each submit one
uncached `246000!`; 80 independent `2+2` requests then arrive on a fixed 25-ms
schedule, starting 100 ms later. Subsequent light sends do not wait for earlier
responses. Driver scheduling lag is recorded (maximum 14.5–16.3 ms here).

One diagnostic repetition per setting, queue capacity 64 unless stated:

| Workers | Heavy clients | Verified / all requests | Verified light / 80 | Successful-light p95 / p99, ms |
| ---: | ---: | ---: | ---: | ---: |
| 2 | 0 | 80/80 | 80 | 2.53 / 4.04 |
| 2 | 2 | 35/82 | 33 | 944.51 / 974.65 |
| 4 | 0 | 80/80 | 80 | 2.82 / 3.33 |
| 4 | 2 | 82/82 | 80 | 2.80 / 3.80 |
| 4 | 4 | 32/84 | 28 | 949.43 / 964.39 |
| 4 | 8 | 33/88 | 29 | 957.98 / 989.14 |
| 8 | 4 | 84/84 | 80 | 2.48 / 3.51 |
| 8 | 8 | 23/88 | 15 | 978.94 / 978.94 |

All returned mathematical successes were verified, including every accepted
factorial. With queue 64, every unsuccessful request was `503 server_busy`;
there were no verification errors or transport failures. Percentiles in the
table exclude rejected requests; the raw output also retains all-response
percentiles and error counts. Neither a low refusal latency nor the percentile
of the surviving subset establishes service capacity.

At four workers/four heavy clients, queue 32 yielded 27/84 verified requests,
including 23/80 light requests, 45 busy responses and 12 connection resets.
Queue 64 removed resets in this observation but did not remove worker
saturation or bounded queue-wait rejection. All 13 settings (2/4/8 workers,
0/2/4/8 heavy clients, plus that queue-32 case) recovered correctly without
restart: fresh `2+2` recovery was 1.75–2.61 ms. Fresh-process peak resident memory
across the matrix was 8.18–33.75 MiB; this is not a sustained-memory result.

Eight workers can preserve light response time with four simultaneous expensive
requests on this host; eight expensive requests can occupy them all again.
Default workers, budgets and queue policy remain unchanged. A host-specific
increase does not solve arbitrary heavy-load admission, and this single short
matrix does not replace the sustained profile or deployment measurements.

```powershell
$env:NUMFORGE_QUEUE_BENCH_WORKERS = '4'
$env:NUMFORGE_QUEUE_BENCH_CAPACITY = '64'
$env:NUMFORGE_QUEUE_BENCH_HEAVY_CLIENTS = '4'
node benchmarks/application/http_queue_benchmark.js BINARY_DIR/numforge_web.exe --arrival-load
```

The default is 80 light requests at 25 ms; `NUMFORGE_QUEUE_BENCH_LIGHT_REQUESTS`
and `NUMFORGE_QUEUE_BENCH_ARRIVAL_MS` configure those bounds. CI validates the
mode through `--arrival-load --quick` using small independently checked
factorials. Raw settings, executable hashes, hardware, outcomes and memory/CPU
observations are retained in ignored `build/performance-20261030/http-arrival-*.jsonl`.

## Bounded request receipt — 30 October 2026

Normal Windows x64 MSVC 19.51 Release (`/O2 /Ob2 /DNDEBUG`), same Ryzen 7
7435HS host, four compute workers and unchanged numeric core `c68f675`.
Server baseline checkout `3cb89db`; final implementation `991cda7` (including
`a7da9ce`). Both executables have allocation statistics off and the same
test-enabled failure hooks. Metadata retains their exact executable hashes.
Driver and server share the host; server priority is BelowNormal. No own
build/test/other benchmark ran alongside these recordings. There is no affinity
or frequency lock. These are local observations, not deployment guarantees.

### Change and resource boundaries

An incomplete HTTP request previously occupied a compute worker while reading.
One receiver now multiplexes incomplete inputs; workers take complete bounded
HTTP frames. Admission attempts one nonblocking read outside the pool lock and
can hand off an already-buffered frame immediately. Remaining fragments use the
receiver's 10-ms poll, subject to OS scheduling granularity. Framing/origin/body
rules and calculation budgets are unchanged.

The shared admission bound remains **workers + queue reserve** across incomplete,
ready and active connections: 36 with defaults, at most 80. This preserves idle
worker headroom; an initial implementation that counted only the queue reserve
reduced 60-client burst admission and was corrected before selecting these
results. Buffers are preallocated: 8 KiB plus frame metadata per admission slot,
up to 640 KiB of input payload at the maximum configuration. There is one extra
I/O thread, plus worker-owned frame snapshots; this is not a hard RSS bound.
Windows readiness sets are explicitly batched to support all 80 slots.

Incomplete input retains its two-second acceptance deadline. A complete queued
frame expires after one second or that original deadline, whichever is earlier.
Expiry scans all ready entries because completion order can differ from
acceptance order. No peer I/O or calculation holds the pool lock. Shutdown joins
the receiver, rejects incomplete/ready inputs, then joins active compute owners
before freeing buffers and sessions.

### Four incomplete inputs and fixed light arrivals

Three interleaved fresh-server before/after pairs, reversing order in the middle
pair. `--slow-clients` opens four incomplete `G` requests, then sends 80 independent
`2+2` requests at 25-ms intervals after a 100-ms settle. OS memory sampling is
before the holders open. Every successful result must equal exactly `4`.

| Version | Verified responses in each run | All-response p95 range, ms | Successful-only p95 range, ms |
| --- | --- | ---: | ---: |
| Before | 36/80, 36/80, 36/80 | 1014.74–1027.92 | 812.13–850.30 |
| After | 80/80, 80/80, 80/80 | 17.05–17.41 | 17.05–17.41 |

All 240 after requests completed correctly without rejection. Each before run
verified 36/80; its other 44 requests were 503 `server_busy` or connection resets.
Both success and failure latency are retained, rather than reporting only fast
refusals. Driver maximum scheduling lag was 15.09–15.30 ms before and
14.49–14.88 ms after, separately from request latency. The visible roughly
16-ms after floor is consistent with Windows polling/scheduling granularity;
this does not establish a universal millisecond bound.

End-of-scenario resident memory was 8.273–8.277 MiB before and
8.559–8.562 MiB after, roughly 0.29 MiB more for this small workload.
Process memory includes runtime, retained caches, thread stacks and OS effects;
these short runs do not prove leak freedom or long-term memory behavior.

### Ordinary mixed requests and bounded admission

Three fresh-server repetitions per client count and setting using the existing
six-turn mathematical workload. This recording groups complete baseline runs
by version; unlike the slow-input comparison it does not interleave individual
cases. All returned successes pass full value/graph checks. Timings move in both
directions, including higher after p95 values in some queue-32 bursts; the
evidence does not establish a general mixed-request speedup.

| Queue reserve | Clients | Before verified per repetition | After verified per repetition | Successful-only p95 range before / after, ms |
| ---: | ---: | --- | --- | ---: |
| 32 | 10 | 60/60, 60/60, 60/60 | 60/60, 60/60, 60/60 | 10.57–14.01 / 11.78–15.09 |
| 32 | 30 | 180/180, 180/180, 180/180 | 180/180, 180/180, 180/180 | 28.61–31.47 / 28.18–41.72 |
| 32 | 60 | 252/360, 247/360, 243/360 | 253/360, 252/360, 252/360 | 60.19–69.06 / 61.28–80.05 |
| 64 | 10 | 60/60, 60/60, 60/60 | 60/60, 60/60, 60/60 | 11.27–16.01 / 11.52–17.54 |
| 64 | 30 | 180/180, 180/180, 180/180 | 180/180, 180/180, 180/180 | 31.86–36.29 / 28.75–41.91 |
| 64 | 60 | 360/360, 360/360, 360/360 | 360/360, 360/360, 360/360 | 60.68–73.68 / 57.28–65.47 |

Serial session prechecks retained all requested values in every scenario.
At 60 clients, simultaneous session reads were 36/60 per run with reserve 32
and 60/60 with reserve 64 in both versions. These default-reserve burst failures
are transport/admission limits, not lost session state. The 64-slot setting
completes all 360 mixed requests and all 60 reads in each repetition, without
changing compute worker count or defaults.

The separate full-admission fixture now opens workers plus reserve incomplete
connections (36 at defaults), then attempts 60 light requests. All 60 were
rejected in each of three runs on both versions, here as connection resets
racing unread request bodies. Best-effort 503 is also an allowed overload
outcome. Every run recovered without restart; after recovery was 1.50–2.22 ms.
Rejecting a saturated pool is not successful throughput, and four incomplete
inputs are no longer an adequate saturation fixture.

### Compute saturation remains separate

One fixed-arrival check per version starts four independently verified
`246000!` calculations plus 80 light additions, with four workers/reserve 32.
All four factorials succeeded in both versions. Light successes were 33/80
before and 34/80 after; all-response light p95 was 1014.43/1011.39 ms. Both
recovered correctly. This receive-stage change **does not isolate arbitrary
expensive calculations** or clients that stop reading large responses. They
can still occupy all workers until the existing computation/send deadlines.

### Verification and retained evidence

Full final ordinary Release CTest suites passed **83/83 MSVC** and **83/83 GCC**.
Native scheduling tests use controlled handler barriers, not hardware-dependent
factorials, to check ready-queue expiry behind a busy worker, expiry behind a
younger head, socket ownership and shutdown joining. Other cases cover partial
bodies, EOF, unchanged acceptance deadlines, capacity/recovery and 80 simultaneous
partial inputs including completion beyond Windows' first readiness batch.
The real-server origin/session test checks a free worker under partial input,
full admission, independent 408 replies and recovery. Its old queued-socket
assumption was replaced with these explicit contracts.

The preliminary receiver build passed 102 selected Chromium regressions for
calculator, session API, release graph scenes and visibility/cache reuse. After
the final capacity/handoff refinement, all 22 graph/session/visibility checks
were repeated successfully. These are correctness checks, not UI latency
measurements or physical-phone results. No Linux, WAN or deployed proxy capacity
was measured in this comparison.

```sh
node benchmarks/application/http_queue_benchmark.js SERVER --slow-clients
node benchmarks/application/http_queue_benchmark.js SERVER --load
node benchmarks/application/http_queue_benchmark.js SERVER --arrival-load
```

Set `NUMFORGE_QUEUE_BENCH_WORKERS`, `NUMFORGE_QUEUE_BENCH_CAPACITY` and the
documented arrival/heavy-client variables for each scope; do not combine
different settings into one capacity claim. `--slow-clients --quick` uses eight
light requests. Keep correctness suites, allocation tracing and speed recording
separate.

Raw evidence remains under ignored `build/performance-ingress/`.
`final-metadata.json` records sources, compiler settings, hashes and scope;
`final-summary.json` preserves scenario rows. Selected files are
`final-paired-slow-{before,after}-*.jsonl`, `final-mixed-*.jsonl` and
`final-compute-saturated-*.jsonl`. The initial unprefixed/mismatched-capacity
recordings are exploratory and retained, not the selected comparison.
`ctest-{gcc,msvc}-handoff.log`, `browser-chromium.log` and `browser-final.log`
retain final native and browser verification. No CI speed threshold was added.

## Mixed Docker pilot (30 October 2026)

Exact deployed C/Django/PostgreSQL/Caddy/Kuma images, cloned into fresh private
resources; [protocol](../protocols/APPLICATION.md#full-application-stack).
One loopback TLS run on this 16-thread/32-GiB Windows PC, with desktop/live pilot
activity and a colocated driver. Image IDs and driver hash are in ignored
`build/mixed-stack-full-20261030/results.json`; raw CPU/RAM and a fuller summary
are alongside it. This is not latest-source C performance or internet capacity.

44,781 fixture responses passed validation, including 10 expected shared-IP 429s;
no unexpected status, transport, mathematical, identity/client-state or replay failures.
Seven separate public health probes passed. Initial login bursts used distinct
simulated edge IPs; a shared IP correctly admitted only 10/20 attempts.

| Clients | Heavy jobs | Active seconds | Small math p95 ms | Account session p95 ms | Login burst p95 ms | Sampled stack MiB |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 | 0 | 20.3 | 5.5 | 32.6 | 622 | 281 |
| 10 | 2 | 20.1 | 4.4 | 33.7 | 626 | 322 |
| 30 | 0 | 20.3 | 4.5 | 34.7 | 1797 | 453 |
| 30 | 2 | 20.3 | 6.5 | 259.7 | 1795 | 380 |
| 60 | 0 | 20.5 | 4.3 | 670.0 | 3540 | 409 |
| 60 | 2 | 20.5 | 4.3 | 382.3 | 3419 | 492 |
| 30 | 2 | 120.3 | 4.5 | 39.3 | 1761 | 387 |

The final row is the sustained phase. Memory is a rolling sampled sum of five
containers, excluding inactive file cache and VM/driver overhead. It is not an
exact peak. Host free memory stayed above 6.5 GiB; the user reported no noticeable
PC slowdown. During the sustained phase, per-service median memory remained
within about 1 MiB between its initial/final 20-second windows. This short run
does not establish long-term stability.

At 60 clients, session reads and concurrent login bursts queue noticeably,
motivating the connection comparison below. No production limits or login policy
were changed in this initial recording.
Client-state checks are not authenticated C-session authorization; that binding
remains separate planned work.

### Persistent account connections

The same deployed images were compared with only Django connection reuse changed:
zero lifetime versus 60 seconds with per-request health checks. Gunicorn remained
one worker/two threads; Argon2 and login limits were unchanged. Three serial pairs
ran in A/B, B/A, A/B order, with fresh data, 60 clients, 250-ms think time and
10 active seconds per phase. Median **run p95**, including queued relogins:

| Heavy jobs | Account session before/after ms | Login burst before/after ms |
| ---: | ---: | ---: |
| 0 | 4541 / 1305 | 5572 / 3767 |
| 2 | 2945 / 1283 | 5266 / 3803 |

Desktop contention caused wide variation: session p95 without heavy jobs ranged
2000–5747 ms before and 439–2056 ms after. Each short phase opened 652–970 DB
connections before versus two after. The final pair added 75 seconds at 30+2:
session p50/p95 fell from 60/126 to 16/37 ms, with 6908 versus eight new connections,
including connection expiry. Login bursts still queue; this is not instant login
or an internet capacity guarantee.

All 54,569 fixture responses verified, including 60 expected shared-IP 429s;
14 separate public probes passed. TLS/browser/identity checks, logout replay,
full-row restore and the first authenticated request after DB restart passed.
Raw runs, pinned images, driver hash and summary: ignored
`build/accounts-connections-20261030/{A1,B1,B2,A2,A3,B3}/` and `summary.json`.

## Candidate qualification — 1 November 2026

The candidate is exact source `0036cbf`, incorporating the arithmetic changes
through `31860f8` and a queue-expiry correction found during this qualification.
The Windows portable package and its old/new/restart/rollback rehearsal are
recorded in [release packaging](../../project/RELEASE_PACKAGING.md#restart-and-rollback-rehearsal).
The Docker web uses GCC 12.2 Release, without benchmark instrumentation. Accounts
use the previously tested `31860f8` image; account sources are unchanged by the
candidate's worker/harness commits. PostgreSQL, gateway and monitor images remain
the captured pilot versions.

During preparation, another task deployed account preferences to the live pilot.
Comparisons therefore explicitly pin the **former** pilot images captured at the
start, rather than silently changing the baseline to that new deployment.
Canonical effective fixture settings compare equal before/after that rollout.
Selected web IDs start `9b26c5bc88da` / `733aa0787a65`; account IDs start
`352758dd8284` / `9cf68d9fca82`. Complete IDs, source/driver hashes and settings
are retained in the raw records. These checks use fresh disposable data, not a
copy of the pilot's account database.

### Queue-expiry regression and fix

The initial `31860f8` full mixed-load run failed qualification: three session-start
requests received `503 server_busy` in the 10-client/two-heavy phase. Those
uncreated sessions then produced 57 failed commits and 57 failed value reads.
These 117 failed validations are not 117 independent initial admission faults.
The recording remains under `build/pilot-candidate-20261101-31860f8/full-candidate/`.
It is a failed diagnostic, not a selected speed comparison; a diagnostic test
build also overlapped its final phase.

The acceptor samples monotonic time before entering the queue lock. A concurrent
receiver/submitter can publish a ready frame with a later timestamp before that
lock is acquired. The sweep previously interpreted that newer timestamp as a
backwards clock and immediately rejected the frame. `0036cbf` skips entries newer
than the sweep's snapshot; the one-second ready deadline, original two-second
admission deadline, bounded capacity and worker-side expiry remain unchanged.

A deterministic regression blocks compute, publishes both buffered and fragmented
requests after an older time sample, then applies that sample to the queue. It
fails on the original code and passes after the correction; a subsequent real
deadline still rejects both queued requests. The seven selected server/client
suites pass in both Windows GCC and MSVC, including actual HTTP-server checks.
All 14 worker cases also pass on Linux with ASan/UBSan and leak detection using
non-PIE executables. The MSVC client suites were relinked against the final
libraries before the final run.

### Upgrade and restore contracts

The fixed images pass old → candidate → old → candidate on disposable volumes.
Migration `identity.0003_preferences` preserves the account and its existing TLS
login cookie. Full-row backup/restore compares all ten old tables, then all eleven
candidate tables, including saved preference values/revisions; source databases
remain unchanged by verification. The old application remains usable against
the additive migrated schema, and preference values survive reapplying the
candidate. This is application rollback, not destructive schema reversal.

The existing integration suite additionally passes all 15 account/preference
backend checks, runtime-role restrictions, service/DB restart, TLS, SK/EN,
CSRF, cookie replay rejection and Chromium account/preferences flows. Browser
checks cover second-browser restore, explicit guest import, concurrent revision
conflicts, offline retry, account switching and delayed-response isolation.
The first authenticated request after DB restart succeeds without restarting
the account process.

Initial fixture-adapter failures are retained separately: another task occupied
the default test ports; a remapped direct HTTP test used the wrong Origin port;
and direct Django Client checks initially inherited the network service's signed
gateway requirement. Final fixtures use independent ports, consistent direct
listener/Origin settings and the intended unit-process context. Real HTTPS and
browser checks retain the gateway guard. No production security rule was relaxed.

### Fixed-candidate mixed load

The complete candidate fixture uses 10/30/60 clients, a 250-ms think time and
20 active seconds at each level with zero/two continuously repeated `100000!`
jobs. It then runs 120 seconds at 30 clients plus two heavy jobs. Every client
relogs once; login bursts, C session starts/commits/reads, 1000-place sqrt,
small calculations and logout-cookie replay are checked. Python Decimal/math
references are prepared outside measurement. Gunicorn remains one worker/two
threads, with the same Argon2 and login policy; C remains four general workers,
the bounded arithmetic reserve and queue reserve 64.

All **56,097 fixture responses** verify, including ten expected shared-IP 429s.
There are no unexpected HTTP, transport, value, session/revision or replay
failures. Seven separate public health probes pass. Results use local TLS on
loopback; simulated distinct edge IPs exist only in the private fixture.

| Clients | Heavy jobs | Active seconds | Small math p95 ms | Account session p95 ms | Login burst p95 ms | Factorial p95 ms | Sampled stack MiB |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 | 0 | 20.2 | 4.92 | 16.74 | 604 | — | 266 |
| 10 | 2 | 20.3 | 4.76 | 15.15 | 591 | 166.87 | 454 |
| 30 | 0 | 20.2 | 4.42 | 20.16 | 1722 | — | 420 |
| 30 | 2 | 20.3 | 4.66 | 28.01 | 1642 | 181.49 | 482 |
| 60 | 0 | 20.3 | 4.10 | 40.90 | 3243 | — | 387 |
| 60 | 2 | 20.3 | 4.42 | 70.87 | 3279 | 200.27 | 425 |
| 30 | 2 | 120.3 | 4.69 | 14.90 | 1691 | 182.90 | 384 |

Memory is the rolling sampled sum of five containers after all five service
samples are present in a phase, not an exact simultaneous peak. Docker CLI
inactive file cache and host/VM/driver overhead are excluded. Host free memory
stays above 6.8 GiB. Desktop, live-pilot and colocated-driver contention remain
in timings/host measurements; no own build, test or other benchmark overlaps
the selected fixed-candidate recordings. This short run does not prove long-term
stability, another host's capacity or internet latency. Login bursts still queue.

The complete record is under ignored
`build/pilot-candidate-20261101-0036cbf/full-candidate/`; sibling upgrade,
functional, compiler, sanitizer and package logs establish the candidate's
other gates. These isolated restores do not verify Windows reboot/login,
physical-phone behavior, external Restic recovery or authenticated C-session
ownership. None of these steps replaces or publishes a running pilot.

### Matched former-pilot comparison

Separate fixtures run three immediate pairs in A/B, B/A, A/B order: 60 clients,
ten active seconds each with zero/two heavy jobs, no sustained phase. A pins the
captured former-pilot web/account IDs; B pins the fixed candidate. All other
images/settings are identical. Each fixture starts with fresh data and repeats
the mathematical, login, shared-IP-limit and replay checks. The table contains
**medians of three run p95 values**, not pooled request percentiles.

| Heavy jobs | Operation | Former pilot p95 ms | Candidate p95 ms |
| ---: | --- | ---: | ---: |
| 0 | Small calculation | 5.45 | 4.80 |
| 0 | Account session | 219.33 | 237.13 |
| 0 | Login burst | 3310.18 | 3323.24 |
| 0 | 1000-place sqrt | 4.95 | 5.30 |
| 2 | Small calculation | 4.66 | 4.77 |
| 2 | Account session | 481.15 | 603.47 |
| 2 | Login burst | 3353.10 | 3329.84 |
| 2 | 1000-place sqrt | 6.01 | 5.71 |
| 2 | `100000!` | 399.20 | 210.53 |

Factorial run p95 ranges separate: 396.85–429.92 / 200.85–218.87 ms. The ratio
of the displayed medians is 1.896, about 47% shorter time for that operation
under this workload. This compares the combined retained changes against the
former images, not the isolated effect of the expiry guard. Small operations
move in both directions. Account-session tails with heavy jobs range
429.20–1161.97 / 376.67–700.71 ms; the worse candidate median is retained, with
overlapping ranges and no account-session speedup claim. Midpoint relogin bursts
affect these short-phase tails; do not compare their p95 directly with the
20/120-second candidate phases above. Login bursts still take about 3.3 seconds
at 60 clients; password policy and worker counts were not tuned here.

The six focused fixtures verify 37,128 responses with zero unexpected errors.
Together with the full candidate they verify **93,225 responses**, including
70 expected 429s, and 19 separate public health probes. All seven selected
fixtures pass resource collection and validation. Raw siblings `A1`, `B1`, `B2`,
`A2`, `A3`, `B3`, `load-plan.json` and `summary.json` retain pair order, every
run's values, identities, settings and sampled host/container resources.

### Pilot deployment after qualification

On 1 November 2026, after explicit approval, the pilot's web service was switched
from `numforge-web:preferences-bdf5a39` to `numforge-web:expiry-0036cbf`, using the
exact qualified image
`sha256:733aa0787a6588add01c3f44077aa2da1950577fd7236800dcdfa4d935f27ab0`.
Accounts, PostgreSQL, gateway and monitoring retained their container identities;
there was no database migration. The existing tunnel and public URL were retained.
The supervisor's operation lock serialized the change, and the previous image
and runtime profile remain available for rollback. Restarting C cleared its
temporary calculation sessions; persisted accounts/preferences were retained.

Both before and after deployment, the configured external Restic repository
received an encrypted backup of the logical account database, private operating
configuration and committed Git history. Each backup passed byte-for-byte restore,
database restore and repository checks; an additional restore compared all eleven
tables and their rows with the unchanged source. Both restore points were retained
at completion. These two runs supplemented the usual retention policy with the
last ten snapshots to preserve the same-day pre-deployment point.

Local and public HTTPS checks passed bootstrap/guest-session checks, five mathematical
references (including 1000-place sqrt and `100000!`), candidate asset comparisons
and foreign-Origin rejection. All five services were healthy and all six monitors,
including verified backup, were up. A subsequent standard supervisor Ensure check
retained the deployed image, other container identities and public URL. Ignored evidence is in
`build/web-deploy-0036cbf-20261101/`: `deployment.json`, pre/post backup records,
`monitoring.json`, the retained runtime profile and execution log. This deployment
does not establish a Windows reboot/login test or an independently stored recovery key.
