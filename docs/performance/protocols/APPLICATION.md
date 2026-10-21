# Formatting, cache and memory benchmarks

For independently validated complex function timings, see
[complex performance diagnostics](COMPLEX.md).

For uniform/adaptive/HTTP graph sampling, independent curve checks and persistent
coordinate axes, see the [graph audit](../baselines/GRAPHS.md).

For the arithmetic suite, exact references, factorial/square experiments and
three-run recording, see [BigInt benchmarks](BIGINT.md).

These opt-in diagnostics measure the real numeric/client code. They do not
change numeric algorithms, release versions, public ownership contracts or
production cache behavior. Timing results are diagnostic, without CI limits.

## Build and run

Use a separate Release build. Do not combine it with sanitizers or coverage:

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DNUMFORGE_BUILD_BENCHMARKS=ON -DNUMFORGE_BUILD_APPS=ON -DBUILD_TESTING=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/bench --config Release --parallel 1
ctest --test-dir build/bench -C Release --output-on-failure
```

For Visual Studio, executables are under `build/bench/Release`; with a
single-config generator they are directly under `build/bench`. Node.js is
needed only for the HTTP driver and baseline recorder. The installed web
application continues to work without Node.js.

```powershell
.\build\bench\Release\allocation_stats_check.exe
.\build\bench\Release\decimal_format_benchmark.exe --quick
.\build\bench\Release\cache_memory_benchmark.exe --quick
node benchmarks/application/http_benchmark.js build/bench/Release/numforge_web.exe --quick
node benchmarks/tools/run_baseline.js build/bench/Release build/baseline-smoke --quick
node benchmarks/tools/run_baseline.js build/bench/Release build/baseline-full
```

The recorder requires an empty output directory. It records three complete
runs, raw CSV, stderr, the tracked working-tree patch, new harness/profiler
sources, CMake cache, commit, CPU/OS and Windows power scheme when available.
Reported CPU MHz is a system snapshot, not proof of fixed clock frequency.
Run on a quiet machine and compare matching cases with identical compiler,
flags, precision and instrumentation. Results belong in the build directory,
not automatically in source control. An uncommitted baseline must retain the
saved patch/source snapshot to identify the measured implementation.

## Formatting

The existing canonical `scientific_10`, `scientific_100`, `scientific_full`
and `fixed_10` cases remain. Explicit `auto_10`, `auto_full`,
`mode_scientific_10` and `mode_scientific_full` exercise
`bigdecimal_format_mode`. Full precision (`places=-1`) is independent of
notation. Full-size sweeps cover 16–16384 coefficient digits; quick runs use
16, 256 and 1024. Inputs and parsing are outside the timed region.

Literal edge cases cover zero, negative values, `1E100000`, `1E-100000`, carry
across 9, half-even ties of both parities and a distant nonzero discarded
digit. Their expected text is checked against literal expectations. Full
BigInt output is compared with its original fixture. These checks complement
the numeric/formatter test suites; benchmarks do not replace correctness tests.

Latency uses calibrated batches, three warmups, seven samples and a 10 ms
batch target (quick: three samples, 2 ms). CSV min/median/max are batch-average
nanoseconds per operation, not individual-call latency percentiles. Output
allocation and destruction are included. Phase profiling and live tracking
use separate single diagnostic invocations after timing; their cost is not
included in the latency samples. Allocation-request counters remain enabled.

```powershell
.\build\bench\Release\decimal_format_benchmark.exe --case auto_10 --digits 16384
.\build\bench\Release\decimal_format_benchmark.exe --case tie_even
```

The recorder launches a new process for each operation/size or literal edge,
so its process peak belongs to that scenario. Running the entire executable
directly reports a cumulative process peak across all preceding cases.

## Cache, sessions and destination reuse

`cache_memory_benchmark` times one transition after constructing an untimed
fixture, using seven independent fixtures (quick: three). It compares each
nonrandom result against a cold evaluation and explicitly asserts the reuse
flag. Scenarios cover legacy cold/hit, output precision/notation changes,
RAD/DEG invalidation, changed expressions, stale revisions, and higher
precision of `sqrt(2)`. Session scenarios cover preview cold/hit, confirmation
adopting a preview, and stable random previews. Confirmation also checks
history state. No cache-hit claim is inferred from speed alone.

Fresh versus reused destinations use the existing `bigdecimal_rescale` API;
both must produce `12345.6789`. Reuse of the destination does not imply reuse
of every internal limb/text buffer, and this work adds no public buffer API.

```powershell
.\build\bench\Release\cache_memory_benchmark.exe --case legacy_hit
.\build\bench\Release\cache_memory_benchmark.exe --case rescale_reused
```

Latency and phase samples have live tracking disabled. A separate tracked
sample includes fixture construction, then resets counters while preserving
fixture/live-cache ownership. `baseline_live_bytes`, `live_after_bytes` and
`peak_tracked_payload_bytes` report scope-wide payload, not just net growth.
After validation and complete fixture teardown the harness requires zero
tracked live bytes. The phase columns describe the final untracked diagnostic
sample; they are not components of the median sample. Use isolated `--case`
processes, as the recorder does, to interpret process peak per scenario.

## Real HTTP

The Node driver starts an isolated loopback server for every scenario/sample,
sends requests with a fresh connection and verifies response values, status,
cache reuse, fraction hints, mathematical copy text and session state.
It includes cold/hit, context changes, FIFO eviction after eight clients,
stale revisions, previews/confirmation/randomness, expired sessions and a
plain output rejected by the existing output limit.

The driver obtains a cold-path reference before the measured request when
applicable. Stale-request and confirmation scenarios additionally check the
retained state afterward. Randomness is checked against the previous preview.

Server traces are enabled only in benchmark builds, with
`NUMFORGE_BENCH_TRACE=1`. The driver sets this automatically; normal requests
produce no trace. `NUMFORGE_BENCH_MEMORY=1` enables live tracking for the
separate memory sample. Invalid/missing traces fail the driver. The server
emits diagnostic CSV to stderr, never to its HTTP response. Tests and normal
clients see the same response protocol.

```sh
node benchmarks/application/http_benchmark.js build/bench/Release/numforge_web.exe --case eviction
```

The default port is 18771, configurable with `NUMFORGE_BENCH_PORT`. Do not
run two drivers on the same port. The driver terminates its own server after
each sample. Round-trip time includes client overhead, connection setup,
HTTP request handling, transfer and OS scheduling. Server `send` measures
response-header construction and blocking send calls; it does not measure
client receipt or pure wire time. Serialization includes fraction hints and
copy-text construction, response allocation, JSON construction and cleanup.
Error scenarios remain separately labeled and are not successful calculations.

## Multi-client load

```sh
node benchmarks/application/http_queue_benchmark.js build/apps/Release/numforge_web.exe --load
```

Use a normal Release server without instrumentation; use its actual platform
path. Without `--load`, the tool retains its three-sample head-of-line CSV.
`--load --quick` validates fixtures with 2/10 clients once; full mode uses
10/30/60 clients, three repetitions and a fresh isolated loopback server per
scenario. It reserves a free port, never uses the running user server/tunnel,
and closes its own process on success or failure.

The mixed workload has six requests per virtual user, cycling through exact
arithmetic, fractions, quadratic equations, a 17-point graph and 1000-digit sqrt(2).
Each user waits for their own response; there is no think time, retry or connection
reuse. Successful results are independently checked, including every graph point
and an integer midpoint rounding test for the square-root decimal. Client-side
checks/scheduling affect total throughput; this is not a pure C kernel measurement.

The session scenario serially starts clients, assigns a distinct x and checks
retention before concurrent reads. Setup/retention checks are outside timing;
the timed reads check each client's own value. FIFO expiry is recorded separately
from connection errors. This measures capacity and isolation, not authenticated
users or persistent accounts. Both scenarios verify recovery with a fresh 2+2.

JSONL starts with source revision/dirty files, executable SHA-256, Node/host and
policy metadata. Rows contain outcome counts, errors, completed/verified throughput,
nearest-rank p50/p95/p99/max latency (including failures) and per-operation mixed
results. Fresh connections have a 15-second absolute deadline and a 256-KiB
response cap. Wrong mathematical results fail the tool; application/transport
failures remain measured outcomes, so exit zero does **not** certify capacity.
No latency threshold is a CI assertion. The quick mode also runs against extracted
Windows/Linux applications in the package CI job.

After timing/recovery, Windows PeakWorkingSet64 or Linux `/proc/PID/status` VmHWM
records process peak resident bytes, including startup and all request work.
It is neither retained heap nor a per-request delta. The Windows OS query runs
outside the timed window. Unsupported/unavailable metrics are null, never zero.
Host/client and OS queueing affect results; TLS, WAN, browser asset loads,
open-loop arrival rates and sustained-memory behavior require separate scenarios.
Keep full JSONL under ignored `build/`; retain only useful summarized evidence.

## Measurement definitions and boundaries

- **Requested bytes/calls:** cumulative application allocation attempts,
  including the full requested size of realloc. They are neither retained
  memory nor peak memory. Benchmark builds count redirected ordinary C
  allocation calls as well as the existing budget-aware helpers.
- **Tracked live/peak payload:** sizes of successful allocations in an enabled
  same-thread scope. The opt-in build redirects malloc/calloc/realloc/free in
  project translation units. System pointer layout stays unchanged, and
  ordinary `free()` remains valid. Tracking needs the redirected release path
  in the benchmark driver; an external client's unobserved free cannot be
  accounted for. Ledger metadata, CRT/OS allocations and allocator overhead
  are excluded. Metadata allocation failure marks the measurement incomplete,
  and the harness refuses to publish it as valid.
- **Reset:** clears requests and sets the high-water mark to current live
  payload, preserving all live ownership records. Failed realloc preserves
  the original record; successful realloc updates its pointer/size whether
  the address moves or stays. Tracking cannot be disabled with outstanding
  records. Private redirected `realloc(p,0)` explicitly frees; this does not
  change the production allocator or its public contract.
- **Process peak:** Windows `PeakWorkingSetSize`, or POSIX `ru_maxrss`
  normalized to bytes (macOS already reports bytes). It includes process,
  libraries, allocator and diagnostic overhead. It is not a heap-only metric
  and cannot be reset per phase. Zero means unavailable. Baseline collection
  uses a new process per selected scenario to avoid cross-case high-water marks.
- **Phase time:** thread-local exclusive scopes from monotonic clocks. Nested
  conversion/rounding/composition time is excluded from its enclosing
  parse/evaluate/format/serialize phase. `other` includes unassigned setup,
  cache/session bookkeeping and teardown inside the measurement window.
  Internal phases can occur during evaluation too; columns must not be
  interpreted as purely top-level stages. Timer calls add diagnostic overhead.

The ledger uses a private pointer list and is intentionally disabled during
latency samples. Memory-enabled timings are not used to claim performance.
The standard production build has no profiler implementation, forced allocator
redirection, live ledger or benchmark HTTP tracing.

## Bounded-prefix formatting follow-up

The selected [paired comparison](../baselines/ARITHMETIC.md#bounded-prefix-formatting)
records the interval-prefix optimization, fallback policy and unchanged full-output
path. Initial pre-optimization recordings remain in Git history. Use the same
compiler, fixture and timing scope when comparing a fresh build.
