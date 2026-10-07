# Benchmark tools

These opt-in tools measure the production library and calculator. Build with
`NUMFORGE_BUILD_BENCHMARKS=ON`; ordinary builds omit the private instrumentation.
See [the benchmark guide](../docs/BENCHMARKS.md) for setup and recording commands.

| Files | Purpose |
| --- | --- |
| `*_benchmark.c` | C timing harnesses for arithmetic, formatting and calculator/cache behavior. |
| `benchmark_clock.c`, `benchmark_clock.h` | Shared monotonic clock. |
| `allocation_stats_check.c` | Allocation-ledger validation. |
| `bigint_cases.js`, `decimal_math_cases.js` | Generate inputs and independent exact arithmetic expectations. |
| `check_*_benchmark.js` | Validate harness results and metric consistency; no speed thresholds. |
| `http_benchmark.js` | Measure the real loopback HTTP path. |
| `run_baseline.js` | Record three isolated runs with source/build/machine metadata. |
| `summarize_decimal_baseline.js` | Check completed decimal recordings and summarize their outcomes. |
| `references/` | Frozen high-precision decimal/math expectations and their developer-only generator. |

The reference generator requires mpmath 1.3.0; running the benchmarks consumes
the checked-in TSV and does not require Python or mpmath. Regeneration instructions
and strict comparison rules are in [the decimal/math report](../docs/DECIMAL_MATH_BENCHMARKS.md).

Save raw recordings under ignored `build/` directories. Keep timing, allocation
and phase probes separate, and validate results before drawing performance
conclusions. Timeouts and reference mismatches remain explicit outcomes.

Read the [BigInt report](../docs/BIGINT_BENCHMARKS.md) and
[formatting/cache report](../docs/BENCHMARKS.md) for existing measurements.
