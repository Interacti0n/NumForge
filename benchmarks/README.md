# Benchmark tools

Use this index to choose an executable. [Performance methodology](../docs/testing/PERFORMANCE.md)
owns build/profiling rules; linked reports preserve input shapes, exact checks,
recorded revisions and results. A dated baseline is not a current speed guarantee.

Build with `NUMFORGE_BUILD_BENCHMARKS=ON`; ordinary builds omit instrumentation.
Save raw local measurements under ignored `build/` directories. Verify results
outside timing and keep timing, allocation and phase probes separate.

## Exact grid baseline

`numerical_grid_benchmark` measures exact traversal and callback accumulation.
See the [grid measurements](../docs/benchmarks/NUMERICAL_COMPONENTS_2026_10_19.md#exact-grid-baseline).

## Adaptive refinement baseline

`numerical_refinement_benchmark` measures the public interval scheduler with
exact quadratic samples. See the [refinement measurements](../docs/benchmarks/NUMERICAL_COMPONENTS_2026_10_19.md#adaptive-refinement-baseline).

## Curve acceptance baseline

`numerical_curve_benchmark` measures the reusable curve classifier. See the
[curve measurements and HTTP parity table](../docs/benchmarks/NUMERICAL_COMPONENTS_2026_10_19.md#curve-acceptance-baseline).

## Phase anchor baseline

`numerical_phase_benchmark` measures periodic target selection/refinement. See
the [phase measurements](../docs/benchmarks/NUMERICAL_COMPONENTS_2026_10_19.md#phase-anchor-baseline).

## Other tools

| Executable | Purpose / useful options | Reference |
| --- | --- | --- |
| `bigint_arithmetic_benchmark` | Exact arithmetic, limb/input shapes and independent fixtures | [BigInt baseline](../docs/benchmarks/BIGINT_BENCHMARKS.md) |
| `decimal_math_benchmark` | Direct decimal/functions; fixture file, `--case NAME`, `--quick`, `--validate`, `--timing-only` | [Decimal/math protocol](../docs/benchmarks/DECIMAL_MATH_BENCHMARKS.md) |
| `complex_oracle_driver` | Complex unary/binary/stability paths, timing and isolated memory probes; built with tests enabled | [Complex baseline](../docs/benchmarks/COMPLEX_BENCHMARKS.md) |
| `logarithm_agm_benchmark` | Series, fixed-guard experiment, bounded intervals and public dispatch | [AGM prototype](../docs/benchmarks/LOGARITHM_AGM_PROTOTYPE_2026_10_11.md), [integration](../docs/benchmarks/LOGARITHM_AGM_INTEGRATION_2026_10_11.md) |
| `polynomial_benchmark` | Exact rational/radical/complex coefficient solutions | [Solver baseline](../docs/benchmarks/PLATFORM_SOLVER_2026_10_19.md) |
| `matrix_benchmark` | Products, determinants, square solves, rank and rectangular families | [Matrices](../docs/benchmarks/MATRIX_2026_10_19.md), [systems](../docs/benchmarks/MATRIX_SYSTEMS_2026_10_19.md) |
| `statistics_benchmark` | Exact summaries; `cov_*` rows cover paired covariance | [Summaries](../docs/benchmarks/STATISTICS_2026_10_19.md), [covariance](../docs/benchmarks/COVARIANCE_2026_10_20.md) |
| `numerical_benchmark` | Public callback bisection; `--high-precision` adds the costly 1000-place case | [Public roots](../docs/benchmarks/NUMERICAL_2026_10_19.md) |
| `numerical_scan_benchmark` | Public multiple-root scan, exact contacts and high-magnitude coordinates | [Public scan](../docs/benchmarks/NUMERICAL_SCAN_2026_10_19.md) |
| `numeric_root_benchmark` | Equation adapter and private method comparisons; options below | [Brent experiment](../docs/benchmarks/NUMERIC_ROOT_BRENT_2026_10_18.md), [hybrid](../docs/benchmarks/NUMERIC_ROOT_HYBRID_2026_10_19.md) |
| `numeric_root_scan_benchmark` | Equation scan at 16/34/100 digits; `--quick` uses 34 once | [Scan](../docs/benchmarks/NUMERIC_ROOT_SCAN_2026_10_19.md), [local candidates](../docs/benchmarks/NUMERIC_ROOT_CANDIDATES_2026_10_19.md) |
| `calculator_benchmark`, `calculator_limit_benchmark` | Whole expression requests, limits, phases and optional expected results | [Timing/cache protocol](../docs/benchmarks/BENCHMARKS.md), [audit](../docs/benchmarks/PERFORMANCE_AUDIT_2026_10_10.md) |
| `graph_benchmark` | Application graph sampling and candidate verification | [Graph audit](../docs/benchmarks/GRAPH_AUDIT_2026_10_12.md), [graph history](../docs/project/RECORDS.md#graphs) |
| `decimal_format_benchmark`, `cache_memory_benchmark` | Presentation/cache costs; `--quick` for smoke checks | [Formatting/cache protocol](../docs/benchmarks/BENCHMARKS.md) |

Historical integer/factorial/workspace optimizations are indexed in
[records](../docs/project/RECORDS.md#arithmetic-and-complex-functions), rather than
repeated as a current implementation chronology here.

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
  See [stress interpretation](../docs/benchmarks/FACTORIAL_STRESS_2026_10_11.md).
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
