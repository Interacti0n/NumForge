# Benchmark tools

These opt-in tools measure the production library and calculator. Build with
`NUMFORGE_BUILD_BENCHMARKS=ON`; ordinary builds omit the private instrumentation.
See [the benchmark guide](../docs/benchmarks/BENCHMARKS.md) for setup and recording commands.
The [11 October optimization report](../docs/benchmarks/PERFORMANCE_OPTIMIZATIONS_2026_10_11.md)
compares direct integer conversion and combinatorial arithmetic with the audit.
The [factor grouping report](../docs/benchmarks/FACTORIAL_GROUPING_2026_10_11.md)
compares the next factorial optimization with the recursive product tree.
The [division report](../docs/benchmarks/DIVISION_2026_10_11.md) records the
radix-2^64 division change and its effects on high-precision calculations.

| Files | Purpose |
| --- | --- |
| `*_benchmark.c` | C timing harnesses for arithmetic, formatting and calculator/cache behavior. |
| `benchmark_clock.c`, `benchmark_clock.h` | Shared monotonic clock. |
| `allocation_stats_check.c` | Allocation-ledger validation. |
| `bigint_cases.js`, `decimal_math_cases.js` | Generate inputs and independent exact arithmetic expectations. |
| `check_*_benchmark.js` | Validate harness results and metric consistency; no speed thresholds. |
| `http_benchmark.js` | Measure the real loopback HTTP path. |
| `http_queue_benchmark.js` | Observe latency behind a slow request on a dedicated loopback server. |
| `run_baseline.js` | Record three isolated runs with source/build/machine metadata. |
| `run_performance_audit.js`, `summarize_performance_audit.js` | Record and check a cross-layer audit with production resource outcomes. |
| `calculator_limit_benchmark.c` | Time the complete calculator pipeline with shared time/allocation/output budgets. |
| `bigint_materialization_benchmark.c` | Isolate integer-to-decimal conversion; verify an exact round trip outside timing. |
| `check_combinatorial_optimization.cjs` | Check large permutations/combinations against independent JavaScript BigInt factorial quotients. |
| `summarize_decimal_baseline.js` | Check completed decimal recordings and summarize their outcomes. |
| `references/` | Frozen high-precision decimal/math expectations and their developer-only generator. |

The reference generator requires mpmath 1.3.0; running the benchmarks consumes
the checked-in TSV and does not require Python or mpmath. Regeneration instructions
and strict comparison rules are in [the decimal/math report](../docs/benchmarks/DECIMAL_MATH_BENCHMARKS.md).

Save raw recordings under ignored `build/` directories. Keep timing, allocation
and phase probes separate, and validate results before drawing performance
conclusions. Timeouts and reference mismatches remain explicit outcomes.

Read the [BigInt report](../docs/benchmarks/BIGINT_BENCHMARKS.md) and
[formatting/cache report](../docs/benchmarks/BENCHMARKS.md) for existing measurements.
