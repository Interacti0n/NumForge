# Benchmark tools

Use this index to choose an executable. [Performance methodology](../docs/testing/PERFORMANCE.md)
owns build/profiling rules; linked reports preserve input shapes, exact checks,
recorded revisions and results. A dated baseline is not a current speed guarantee.

Build with `NUMFORGE_BUILD_BENCHMARKS=ON`; ordinary builds omit instrumentation.
Save raw local measurements under ignored `build/` directories. Verify results
outside timing and keep timing, allocation and phase probes separate.

## Exact grid baseline

`numerical_grid_benchmark` measures exact traversal and callback accumulation.
See the [grid measurements](../docs/performance/baselines/SAMPLING.md#exact-grid-baseline).

## Adaptive refinement baseline

`numerical_refinement_benchmark` measures the public interval scheduler with
exact quadratic samples. See the [refinement measurements](../docs/performance/baselines/SAMPLING.md#adaptive-refinement-baseline).

## Curve acceptance baseline

`numerical_curve_benchmark` measures the reusable curve classifier. See the
[curve measurements and HTTP parity table](../docs/performance/baselines/SAMPLING.md#curve-acceptance-baseline).

## Phase anchor baseline

`numerical_phase_benchmark` measures periodic target selection/refinement. See
the [phase measurements](../docs/performance/baselines/SAMPLING.md#phase-anchor-baseline).

## Other tools

| Executable | Purpose / useful options | Reference |
| --- | --- | --- |
| `bigint_arithmetic_benchmark` | Exact arithmetic, limb/input shapes and independent fixtures | [BigInt baseline](../docs/performance/protocols/BIGINT.md) |
| `decimal_math_benchmark` | Direct decimal/functions; fixture file, `--case NAME`, `--quick`, `--validate`, `--timing-only` | [Decimal/math protocol](../docs/performance/protocols/DECIMAL.md) |
| `complex_oracle_driver` | Complex unary/binary/stability paths, timing and isolated memory probes; built with tests enabled | [Complex baseline](../docs/performance/protocols/COMPLEX.md) |
| `logarithm_agm_benchmark` | Series, fixed-guard experiment, bounded intervals and public dispatch | [Interval algorithm and timings](../docs/performance/baselines/LOGARITHM.md) |
| `polynomial_benchmark` | Exact rational/radical/complex coefficient solutions | [Solver baseline](../docs/performance/baselines/MODULES.md#polynomials) |
| `matrix_benchmark` | Products, determinants, square solves, rank and rectangular families | [Matrix/system baselines](../docs/performance/baselines/MODULES.md#matrices) |
| `statistics_benchmark` | Exact summaries; `cov_*` rows cover paired covariance | [Statistics/covariance baselines](../docs/performance/baselines/MODULES.md#statistics) |
| `numerical_benchmark` | Public callback bisection; `--high-precision` adds the costly 1000-place case | [Public roots](../docs/performance/baselines/ROOTS.md#single-root) |
| `numerical_scan_benchmark` | Public multiple-root scan, exact contacts and high-magnitude coordinates | [Public scan](../docs/performance/baselines/ROOTS.md#multiple-roots) |
| `numeric_root_benchmark` | Equation adapter and private method comparisons; options below | [Private method comparison](../docs/performance/baselines/ROOT_EXPERIMENTS.md#private-interpolation-decision) |
| `numeric_root_scan_benchmark` | Equation scan at 16/34/100 digits; `--quick` uses 34 once | [Candidate discovery](../docs/performance/baselines/ROOT_EXPERIMENTS.md#local-candidate-discovery) |
| `calculator_benchmark`, `calculator_limit_benchmark` | Whole expression requests, limits, phases and optional expected results | [Timing/cache protocol](../docs/performance/protocols/APPLICATION.md) |
| `graph_benchmark` | Application graph sampling and candidate verification | [Graph corpus](../docs/performance/baselines/GRAPHS.md) |
| `decimal_format_benchmark`, `cache_memory_benchmark` | Presentation/cache costs; `--quick` for smoke checks | [Formatting/cache protocol](../docs/performance/protocols/APPLICATION.md) |

Selected results and algorithm decisions live in the
[performance evidence tree](../docs/performance/README.md). Routine recordings
belong under ignored `build/`; superseded tuning diaries remain in Git history.

## Root comparison and validation

`numeric_root_benchmark --compare` records paired bisection/private Brent runs
at 16/34/100 digits, three repeats. `--quick` selects precision 34 once;
`--adversarial` selects the vanishing-jump domain regression. `--hybrid` adds
bounded-decimal interpolation with progress-based permanent bisection fallback.
Flags can be combined. Production C/HTTP solving still selects bisection.

Run `python scripts/check_numeric_root_scan.py scan.csv` on full scan output.
`python benchmarks/check_numeric_root_benchmark.py roots.csv` independently
reevaluates root-benchmark coordinates at 180 digits;
use it on default/comparison CSVs, not just smoke rows. Outcome limits and
reference mismatches remain failures/diagnostics, not successful timing claims.

## Specialized probes

- Factorial stress: generate `--workspace --stress` fixtures and use
  `run_factorial_stress.cjs` for serial whole-request/phase/tree diagnostics.
  `calculator_limit_benchmark --phases-profile` measures exclusive normalization.
  See [stress diagnostics](../docs/performance/baselines/FACTORIAL.md#stress-diagnostics).
- The 100,000-bit GCD case uses `--validate`: one verified API timing and a
  separate memory probe, rather than the normal seven-sample protocol.
- Optional decimal profiling attributes add/sub/mul/div/mod/pow/GCD/divmod work;
  those clocks are separate from normal timing. A dedicated server can record
  queue waiting without using the user's running server.
- Calculator-limit CSV `budget_reason` distinguishes time/allocation from none;
  none does not mean an unsuccessful request passed all domain/output limits.
- Set `NUMFORGE_BENCH_EXPECTED` or `NUMFORGE_BENCH_EXPECTED_FILE` for an exact
  whole-result check after timing. File contents are UTF-8 with no trailing
  newline. These checks do not change production limits.
