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
The [decimal allocation report](../docs/benchmarks/DECIMAL_ALLOCATIONS_2026_10_11.md)
records coefficient borrowing and private series-sum reuse under unchanged limits.
The [digit counting report](../docs/benchmarks/DIGIT_COUNTING_2026_10_11.md)
records exact text-free precision selection and its allocation savings.
The [complex paired evaluation report](../docs/benchmarks/COMPLEX_PAIR_2026_10_11.md)
records shared scalar sine/cosine work and high-precision calculator outcomes.
The [hyperbolic paired evaluation report](../docs/benchmarks/HYPERBOLIC_PAIR_2026_10_11.md)
records shared sinh/cosh work while preserving the stable small-argument series.
The [performance refresh](../docs/benchmarks/PERFORMANCE_REFRESH_2026_10_11.md)
compares the current cross-layer results and prioritizes remaining allocation
failures, high-precision work and large multiplication experiments.
The [GCD workspace report](../docs/benchmarks/GCD_WORKSPACE_2026_10_11.md)
records reusable remainder/normalization scratch and restored large rational requests.
The [decimal workspace report](../docs/benchmarks/DECIMAL_WORKSPACE_2026_10_11.md)
records ln/exp allocation reduction and remaining high-precision CPU limits.
The [logarithm algorithm review](../docs/benchmarks/LOGARITHM_ALGORITHM_REVIEW_2026_10_11.md)
compares high-precision alternatives and proposes a theta/AGM prototype.
The opt-in `logarithm_agm_benchmark` evaluates that fixed-guard experiment,
the bounded interval implementation, the original series and public dispatcher,
with independent references and failure checks;
the [prototype results](../docs/benchmarks/LOGARITHM_AGM_PROTOTYPE_2026_10_11.md)
record the original experiment's crossover and prerequisites. The separate
[bounded AGM integration](../docs/benchmarks/LOGARITHM_AGM_INTEGRATION_2026_10_11.md)
now serves suitable high-precision requests through the public C API.
The [factorial budget report](../docs/benchmarks/FACTORIAL_BUDGETS_2026_10_11.md)
records inputs above the former cap, deadline cancellation and separate final
text-conversion timing for logarithms.
The [Karatsuba integration](../docs/benchmarks/KARATSUBA_INTEGRATION_2026_10_11.md)
records compiler/operand-shape checks and the production budget-aware dispatcher.
Use `run_karatsuba.cjs executable empty-directory --focused` for a serial
three-run balanced/square/3:2/4:3/near-2:1 crossover recording. The ordinary
matrix includes intermediate ratios too; all products are compared in full.

| Files | Purpose |
| --- | --- |
| `karatsuba_experiment.c`, `karatsuba_cases.cjs`, `check_karatsuba.cjs`, `run_karatsuba.cjs` | Threshold adapter over the shared private multiplication kernel, complete Node BigInt oracles, failure checks and serial threshold measurements; see the [experiment report](../docs/benchmarks/KARATSUBA_EXPERIMENT_2026_10_11.md). |
| `generate_karatsuba_factorials.py` | Independent Python factorial references for grouped product-tree comparisons. |
| `*_benchmark.c` | C timing harnesses for arithmetic, formatting and calculator/cache behavior. |
| `logarithm_agm_benchmark.c`, `check_logarithm_agm_benchmark.cjs` | Fixed-guard experiment, interval/public/series logarithm comparisons, exact-reference checks and opt-in allocation/deadline injection. |
| `generate_logarithm_agm_cases.py`, `data/logarithm_agm_smoke.tsv` | Standard-library Decimal references checked at two extra precisions; Python is needed only to regenerate fixtures. |
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

The audit also measures 128–1,024-limb division shapes and 2,048–8,192-limb
balanced/unbalanced multiplication. Complex pipeline probes include forward and
inverse functions, tiny/boundary/scaled arguments and intermediate 5,000-place
settings. Separate decimal profiles attribute core time and call counts to
add/sub/mul/div/mod/pow/GCD/divmod; those extra clocks run only during profiling,
never in ordinary timing samples or production builds. A dedicated server
records queue waiting without touching the user's server or tunnel.
Large squaring compares the same operands through generic multiplication and
the private experiment. The 100,000-bit GCD case uses `--validate` (one verified
API timing and a separate memory probe) instead of the ordinary seven-sample
protocol. Calculator CSV includes `budget_reason` (`none`, `time`, `allocation`);
allocation covers either cumulative or single-allocation exhaustion, while
`none` does not imply that an unsuccessful call passed its output/domain limits.

Read the [BigInt report](../docs/benchmarks/BIGINT_BENCHMARKS.md) and
[formatting/cache report](../docs/benchmarks/BENCHMARKS.md) for existing measurements.

Complete calculator references can use `NUMFORGE_BENCH_EXPECTED` or
`NUMFORGE_BENCH_EXPECTED_FILE` (exact UTF-8 bytes, no trailing newline). Both
checks run after timing; file expectations avoid environment-size limits for
large exact fractions. Neither setting changes production calculator limits.
