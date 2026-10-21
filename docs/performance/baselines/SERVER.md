# Serial server baseline

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
