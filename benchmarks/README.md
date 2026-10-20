# Benchmark tools

Build with `cmake --preset benchmarks` then `cmake --build --preset benchmarks`
(sets `NUMFORGE_BUILD_BENCHMARKS=ON`); ordinary builds omit instrumentation.
Use a separate Release build without sanitizers/coverage. See the
[measurement method](../docs/testing/PERFORMANCE.md) and selected
[results](../docs/performance/README.md). Routine recordings belong in ignored
`build/`; no timing threshold is a correctness assertion.

## Source tree

```text
benchmarks/
  CMakeLists.txt   targets and opt-in correctness checks
  common/         shared clock and allocation instrumentation check
  core/           integer, decimal, formatting and logarithm measurements
  modules/        polynomial, matrix and statistics measurements
  numerical/      public root/scan and combined sampling measurements
  application/    expression, cache, graph, equation and HTTP measurements
  tools/          independent fixture generators, checkers and recorders
  references/     frozen oracle inputs required by benchmarks/tests
```

Binary names stay independent of source folders. Executables are in the build
root (Visual Studio: its `Release/` subdirectory), so recorders take one binary
directory. Tests for allocation/deadline failures and corrupt references remain
enabled with `BUILD_TESTING=ON`; they are useful correctness checks, not old logs.

## Choose a measurement

| Executable | What it measures | Details |
| --- | --- | --- |
| `bigint_arithmetic_benchmark` | Exact arithmetic, dense operand sizes, aliases, factorials and threshold comparisons | [BigInt protocol](../docs/performance/protocols/BIGINT.md) |
| `decimal_math_benchmark` | Decimal arithmetic/functions; `--quick`, `--case NAME`, `--validate`, `--timing-only` | [Decimal protocol](../docs/performance/protocols/DECIMAL.md) |
| `decimal_format_benchmark` | Full/prefix output conversion and rounding; `--quick` | [Formatting protocol](../docs/performance/protocols/APPLICATION.md) |
| `logarithm_agm_benchmark` | Series/public dispatch, bounded interval implementation and private fixed-guard comparison | [Logarithm decision](../docs/performance/baselines/LOGARITHM.md) |
| `polynomial_benchmark`, `matrix_benchmark`, `statistics_benchmark` | Exact module operations, including rectangular systems and covariance | [Module baselines](../docs/performance/baselines/MODULES.md) |
| `numerical_sampling_benchmark [all\|grid\|refinement\|curve\|phase]` | Four public sampling kernels with shared CSV columns | [Sampling baselines](../docs/performance/baselines/SAMPLING.md) |
| `numerical_benchmark`, `numerical_scan_benchmark` | Public callback root searches; `--high-precision` on the single-root tool adds expensive 1000-place work | [Public roots](../docs/performance/baselines/ROOTS.md) |
| `calculator_benchmark`, `calculator_limit_benchmark`, `cache_memory_benchmark` | Repeated expression phases, complete budgeted requests and cache/destination reuse | [Application protocol](../docs/performance/protocols/APPLICATION.md) |
| `graph_benchmark`, `graph_fast_benchmark` | Uniform/adaptive requests and isolated fast-expression compilation/evaluation | [Graph corpus](../docs/performance/baselines/GRAPHS.md) |
| `graph_verify_benchmark [cache\|refinement] [1..10]` | Controlled cache-off/on or legacy/Brent comparisons; separate CSV schema per mode | [Refinement comparisons](../docs/performance/baselines/GRAPH_REFINEMENT.md) |
| `numeric_root_benchmark`, `numeric_root_scan_benchmark` | Equation adapters, domain checks, private methods and local discovery | [Equation comparisons](../docs/performance/baselines/ROOT_EXPERIMENTS.md) |

`complex_oracle_driver --benchmark` is built with tests, outside this directory;
it validates every timed result. See the [complex protocol](../docs/performance/protocols/COMPLEX.md).

## Record and verify

`tools/run_baseline.js BINARY_DIR EMPTY_OUTPUT_DIR --bigint|--decimal` records
three isolated runs with fixtures, revision/patch, compiler and host metadata.
Omit the suite flag for formatting/cache/HTTP. `--quick` checks the harness;
`--case-timeout-ms N` bounds the complete child process, not one arithmetic call.
Use `tools/summarize_decimal_baseline.js` for decimal outcomes, including failures.

`tools/run_karatsuba.cjs` records controlled threshold/factorial comparisons;
`tools/run_factorial_stress.cjs` additionally checks complete budgeted calculator
projection. Their different timing scopes are intentional. Keep the independent
`tools/check_*` validators and frozen `references/` inputs; do not replace them
with comparisons against the implementation's own output.

`numeric_root_benchmark --hybrid` compares all three private methods;
`--compare` compares bisection/Brent, `--quick` uses precision 34 once and
`--adversarial` exercises the vanishing-jump rejection. Verify complete CSV with
`tools/check_numeric_root_benchmark.py`. Application scan CSV is independently
checked by `scripts/check_numeric_root_scan.py`. Production solving uses bisection.

For full calculator checks set `NUMFORGE_BENCH_EXPECTED` or
`NUMFORGE_BENCH_EXPECTED_FILE`; expected UTF-8 file content has no trailing newline.
`budget_reason:none` alone does not establish success. Requested allocation bytes
are cumulative traffic, not peak/live memory or process RSS. Keep phase/live-memory
probes separate from speed recordings.

## HTTP and browser work

`application/http_benchmark.js` measures calculator HTTP phases;
`http_queue_benchmark.js` observes independent-session queue waiting.
`application/benchmark_graph_audit.cjs` owns independent HTTP curve/domain/dense
checks and supports matching `--before` comparisons. `benchmark_graph_multi.cjs`
measures browser interaction, `benchmark_graph_stress.cjs` measures sustained cache
lifetime, and `benchmark_graph_budget.cjs` checks heavy graph queue/abort recovery.
They launch and close their own loopback servers. Browser tools use dependencies
from `tests/browser`; mobile emulation does not establish physical-device behavior.

## Keep only useful tools

Add a tool only for a distinct measurement or independent correctness check.
Extend an existing selector/fixture set when timing scope and verification match.
Retain failed/limited outcomes; never report an unvalidated fast row as a success.
