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

## Windows Quick Tunnel preview — 22 October 2026

The [verified static Windows package](../../project/RELEASE_PACKAGING.md#restart-and-rollback-rehearsal)
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
after each recovery; old sessions were lost as expected. No watchdog was installed.

Raw ignored evidence: `build/public-tunnel-check.jsonl` and its one-off probe
`build/public-tunnel-check.cjs`. The log also retains an initial malformed-query
probe; the table uses the corrected canonical query order. These short bursts
do not establish sustained proxy capacity, memory stability or uptime. Windows
startup, automatic failure supervision and host resource limits remain open.
