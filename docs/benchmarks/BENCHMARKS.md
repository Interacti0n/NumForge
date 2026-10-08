# Formatting, cache and memory benchmarks

For the arithmetic suite, exact references, factorial/square experiments and
three-run recording, see [BigInt benchmarks](BIGINT_BENCHMARKS.md).

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
node benchmarks/http_benchmark.js build/bench/Release/numforge_web.exe --quick
node benchmarks/run_baseline.js build/bench/Release build/baseline-smoke --quick
node benchmarks/run_baseline.js build/bench/Release build/baseline-full
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
node benchmarks/http_benchmark.js build/bench/Release/numforge_web.exe --case eviction
```

The default port is 18771, configurable with `NUMFORGE_BENCH_PORT`. Do not
run two drivers on the same port. The driver terminates its own server after
each sample. Round-trip time includes client overhead, connection setup,
HTTP request handling, transfer and OS scheduling. Server `send` measures
response-header construction and blocking send calls; it does not measure
client receipt or pure wire time. Serialization includes fraction hints and
copy-text construction, response allocation, JSON construction and cleanup.
Error scenarios remain separately labeled and are not successful calculations.

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

## Validation and next decisions

`allocation_stats_check` tests retained allocations across reset, growing and
shrinking realloc, failed budget-aware realloc, calloc overflow, NULL/untracked
free, tracking-scope boundaries and nested phase scopes. Quick formatting and
cache harnesses are registered with CTest in benchmark/test-enabled builds.
Run the normal Release tests, allocation-failure tests, C/C++ installed-package
consumers and SK/EN browser suite as well. Keep timing thresholds out of CI.

The measurements identify candidates for review point 1.4. Prefix conversion,
buffer reuse or numeric algorithm changes require their own correctness work
and before/after measurements, rather than being implemented inside this
measurement-only change.

## Initial baseline: 6 October 2026

Three full runs were collected on Windows x64, AMD Ryzen 7 7435HS, with
MSVC 19.51 Release and the instrumentation described above. This was working
tree code based on `574704b`; its patch and new sources are saved with the
local baseline at `build/baseline-1-1-full`. The runner completed 232 commands,
including three HTTP runs of all 16 scenarios. Raw files remain local build
artifacts. CPU affinity/frequency were not locked; these are observations
from one machine, not cross-platform guarantees or release performance claims.

For the 16384-digit fixture, the range of the three per-run batch medians was:

| Operation | Median range | Output bytes | Peak tracked payload |
| --- | ---: | ---: | ---: |
| Auto, 10 places | 2.951–2.975 ms | 19 | 32784 bytes |
| Explicit scientific, 10 places | 2.886–2.973 ms | 19 | 32784 bytes |
| Explicit scientific, full precision | 2.878–3.038 ms | 16392 | 49172 bytes |
| Full BigInt coefficient conversion | 2.921–2.972 ms | 16384 | 23327 bytes |

The separate phase probes assign over 98% of the Auto-formatting time to
coefficient conversion. Short output still incurs full coefficient conversion
and a full-size temporary significand. This supports investigating prefix
conversion with rigorous guard/sticky rounding in point 1.4. It does not
establish a crossover size or justify changing the formatter without tests.

For `1/3`, legacy cache cold samples used 356 allocation requests and 5735
requested bytes, versus 127 and 2231 for a hit. Hit probes had no parse/evaluate
phase; formatting remained active. Three per-run medians ranged from
37.4–59.4 microseconds cold and 15.2–18.2 microseconds on a hit. The HTTP
measurements additionally account for fraction hints, serialization and send,
which a direct cache timing omits.

Fresh versus reused rescale destinations required 20 versus 18 allocation
requests, and 376 versus 328 requested bytes. Reuse did not consistently
improve latency: fresh medians were 2.2–2.3 microseconds, reused 2.1–3.9.
Peak tracked payload was 408 versus 416 bytes, with different retained fixture
baselines (56 versus 112). The data supports reporting reduced allocation
traffic, not claiming reduced peak memory or a general speedup.

Validation passed: 28 normal MSVC Release CTests, 31 instrumented MSVC
CTests, 31 GCC 16.2 Release CTests, 66 SK/EN browser tests, installed C and
C++ consumers, and all full benchmark scenarios. Linux/macOS runtime and
other architectures were not exercised locally; the existing CI matrix
remains the place to validate those platforms.

## Direct decimal and mathematical functions

See [BigDecimal and higher-function measurements](DECIMAL_MATH_BENCHMARKS.md)
for direct arithmetic, scale/normalization diagnostics, independently checked
high-precision math, constant factory boundaries and time-bounded three-run
recording. That suite keeps numerical mismatches separate from valid timings.


## Bounded-prefix formatting follow-up

Short scientific output now forms positive lower/upper bounds from the leading
binary limbs and omitted-bit interval. Powers of two and products use outward
FLOOR/CEILING rounding at a bounded working precision. Only when both endpoints
produce the same requested result and agree on the scientific exponent is that
text accepted. Ambiguous sticky/tie/exponent boundaries use the full conversion.
No floating-point digit estimate decides a result; full output retains its path.

The fast path currently accepts 0–128 mantissa places and a conservative size
threshold of `64 + places*places/32` coefficient limbs. The threshold avoids a
measured regression for 100-place output on 4,096-digit inputs. It is a local
selection policy, not a universal crossover claim.

Three matching before/after runs on the same Windows x64/Ryzen 7 7435HS/MSVC
19.51 Release setup are preserved in `build/format-prefix-comparison/`, including
CSV, compiler diagnostics, source patch and machine metadata. The before
executable contains the unchanged formatter from `fd248fc`; after is the current
development snapshot. No CPU affinity/frequency locking was used.

| Case | Before median range | After median range |
| --- | ---: | ---: |
| Auto, 10 places, 4,096 digits | 170–176 µs | 105–135 µs |
| Auto, 10 places, 16,384 digits | 2.894–3.066 ms | 0.124–0.193 ms |
| Scientific, 100 places, 4,096 digits (fallback) | 171–183 µs | 172–176 µs |
| Scientific, 100 places, 16,384 digits | 2.884–2.920 ms | 0.602–0.645 ms |
| Full scientific, 16,384 digits | 2.841–2.914 ms | 2.847–2.878 ms |

The 16,384-digit Auto result remains 19 bytes. Peak tracked payload in the
operation probe drops from 32,784 to 632 bytes; cumulative requested bytes drop
from 47,957 to 17,382, while allocation calls increase from 7 to 702. The method
trades many small temporary allocations for less conversion work and lower
peak payload; these figures are not process RSS or a general allocator claim.

Independent Node exact-integer rounding checks cover both signs, all six modes,
0/10/100/128 places, large exponents, carry/sticky tails and the 129-place fallback.
Every allocation in an exercised prefix path is failed in turn to verify output
preservation. GCC and MSVC correctness checks pass; comparable performance on
other compilers/platforms remains future measurement work.
