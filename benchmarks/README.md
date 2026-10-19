# Benchmark tools

## Exact grid baseline

`numerical_grid_benchmark` visits exact coordinates and accumulates their rational
sum in the callback. It checks `sum = count*(lower+upper)/2` outside timing and
runs three repetitions per case. Build with `NUMFORGE_BUILD_BENCHMARKS=ON`, then
run the executable; no engine or server is involved. The measurement includes
callback arithmetic, but excludes setup, expected-value construction and checking.

On Windows x64, GCC 16.2 Release with allocation instrumentation (19 October 2026),
the median was 6.310 ms for 1024 points on `[-10,10]`, 0.368 ms for 33 points on
`[1E1000,1E1000+1]`, and 0.486 ms for 33 points on `[0,1E-1000]`.
[Raw repetitions](../docs/benchmarks/NUMERICAL_GRID_2026_10_19.csv) also record
allocation calls and cumulative requested bytes, not peak live memory. These are
initial observations; this extraction does not establish a speed improvement.

## Adaptive refinement baseline

`numerical_refinement_benchmark` uses the public scheduler with caller-owned
exact rational samples of `x*x` and a midpoint chord-error criterion. It checks
closed-form point counts and exact spacing outside timing, with three repeats
per case and a capacity of 257 points. Setup and validation are excluded;
callback evaluation, point insertion and queue allocation are included. This
does not measure expression parsing or the complete graph pipeline.

On Windows x64, GCC 16.2 Release with allocation instrumentation (19 October 2026),
medians were 3.324 ms for 129 points on `[0,1]`, 0.819 ms for nine points on
`[1E1000,1E1000+1]`, and 1.970 ms for 17 points on `[0,1E-1000]`.
The absolute midpoint tolerances were respectively `1/16384`, `1/64` and `1E-2002`.
[Raw repetitions](../docs/benchmarks/NUMERICAL_REFINEMENT_2026_10_19.csv) include
allocation calls and cumulative requested bytes, not peak live memory. This is
an initial baseline; no before/after speedup is established. Scheduler index
updates retain the original worst-case quadratic work in the point limit.

## Curve acceptance baseline

`numerical_curve_benchmark` measures 1000 calls to the reusable public curve
classifier, with three repetitions per case. It checks the expected decision
outside timing. Setup, classifier creation and validation are excluded; decimal
arithmetic and phase checks are included. The cases accept a linear triple,
reject an unsafe phase and reject a `1E-1000` midpoint deviation at a `1E1000`
offset under tolerance `1E-1001`. No expression parsing or rendering is measured.

On Windows x64, GCC 16.2 Release with allocation instrumentation (19 October 2026),
median times for 1000 assessments were 6.228 ms, 4.110 ms and 38.239 ms respectively.
[Raw repetitions](../docs/benchmarks/NUMERICAL_CURVE_2026_10_19.csv) record allocation
calls and cumulative requested bytes, not peak memory. This is an initial baseline,
not evidence of a speedup from extraction.

The MSVC Release server at `5759ea5` and the extracted implementation also returned
identical complete parsed JSON for these `POST /api/graph/plot` requests, including
every coordinate, connection, classification and diagnostic field:

| Expression | x range | Extra query | Points |
| --- | --- | --- | --- |
| `x*x` | `[-1,1]` | defaults | 65 |
| `sin(33x)` | `[-10,10]` | defaults | 723 |
| `1/(x-0.1)` | `[-1,1]` | defaults | 771 |
| `floor(x)` | `[-2,2]` | defaults | 153 |
| `sqrt(x)` | `[-1,1]` | defaults | 127 |
| `sin(x)` | `[-4.71238798038469,-4.71238098038469]` | defaults | 65 |
| `sinh(10x)` | `[-2,2]` | fast mode, precision 40, 800x440, y `[-2,2]` | 265 |
| `floor(x)` | `[-2,2]` | fast mode, precision 40, 800x440, y `[-1,1]` | 153 |

Precision 40 selects the decimal fallback for those camera cases. This finite
comparison complements the C and browser graph regressions; it is not a proof
for all expressions or resource-limited executions.

## Other tools

`numerical_benchmark` checks standalone rational bisection at 12 and 100 places;
`--high-precision` adds a costly 1000-place case. See the
[checked baseline and resource limits](../docs/benchmarks/NUMERICAL_2026_10_19.md).
`numerical_scan_benchmark` measures the public multiple-root API using independent
sqrt references and exact contact coordinates, including 1000-digit offsets/scales;
see [its baseline](../docs/benchmarks/NUMERICAL_SCAN_2026_10_19.md).

These opt-in tools measure the production library and calculator. Build with
`NUMFORGE_BUILD_BENCHMARKS=ON`; ordinary builds omit the private instrumentation.
See [the benchmark guide](../docs/benchmarks/BENCHMARKS.md) for setup and recording commands.
The opt-in `statistics_benchmark` measures exact rational dataset summaries,
including 1024-sample and 1000-digit offset/spread cases, with closed-form checks
and allocation counters; see [the baseline](../docs/benchmarks/STATISTICS_2026_10_19.md).
The opt-in `polynomial_benchmark` measures the [standalone exact solver](../docs/reference/POLYNOMIAL_API.md)
on rational, irrational, complex and 1000-digit-coefficient cases, checking exact
root-sum/product invariants outside timing and claiming no algorithmic speedup.
See the [initial solver baseline](../docs/benchmarks/PLATFORM_SOLVER_2026_10_19.md).
The opt-in `matrix_benchmark` measures dense exact rational products, determinants
and solves with closed-form references and allocation counters; see the
[matrix baseline](../docs/benchmarks/MATRIX_2026_10_19.md).
Its [general-system extension](../docs/benchmarks/MATRIX_SYSTEMS_2026_10_19.md)
also measures exact rectangular rank and complete solution families/inconsistency.
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
The [factorial tree integration](../docs/benchmarks/FACTORIAL_TREE_INTEGRATION_2026_10_11.md)
records reusable workspace, budget admission and complete calculator results.
The [factorial stress report](../docs/benchmarks/FACTORIAL_STRESS_2026_10_11.md)
separates the admission cliff from actual scratch limits and decimal normalization.
Generate `--workspace --stress` fixtures and use `run_factorial_stress.cjs`
for serial budgeted whole-request/phase/tree diagnostics. Calculator limit
benchmark `--phases-profile` additionally measures exclusive normalization time.
The [zero-normalization integration](../docs/benchmarks/ZERO_NORMALIZATION_2026_10_11.md)
compares scalar/inverse blocks and complete calculator requests. Generate its
coefficient/count references with `generate_zero_normalization.py`; `--timing`
selects the smaller recording matrix.
The [factorial admission report](../docs/benchmarks/FACTORIAL_ADMISSION_2026_10_12.md)
records checked range/workspace bounds. Generate `--workspace --admission`
fixtures; the arithmetic harness exposes `--factorial-plan n` and diagnostic
`--plan-budget` to check allocations against those bounds. Application-limit
measurements use `--budget` or the serial factorial stress runner.
The [split workspace report](../docs/benchmarks/SPLIT_WORKSPACE_2026_10_12.md)
closes this factorial experiment. Generate `--workspace --split` fixtures;
`--timing` compares unsplit and split buffers. Stress/admission fixtures now
compare production with the current `tree_split` layout.
Use `generate_karatsuba_factorials.py output.tsv --workspace` for public,
grouped and workspace-tree references; add `--timing` for the serial speed matrix.
Use `run_karatsuba.cjs executable empty-directory --focused` for a serial
three-run balanced/square/3:2/4:3/near-2:1 crossover recording. The ordinary
matrix includes intermediate ratios too; all products are compared in full.

| Files | Purpose |
| --- | --- |
| `numeric_root_benchmark.c`, `numeric_root_cases.h`, `check_numeric_root_benchmark.py` | Bounded high-precision equation baseline, shared analytic/diagnostic corpus and independent Decimal residual checks; [original audit](../docs/benchmarks/NUMERIC_ROOT_AUDIT_2026_10_18.md) and [domain-check follow-up](../docs/benchmarks/NUMERIC_ROOT_DOMAINS_2026_10_18.md). The separate `--adversarial` run now verifies that the vanishing jump returns domain_unverified. |
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
| `../scripts/benchmark_graph_audit.cjs`, `../scripts/check_graph_audit_decimal.py` | Independent graph value/geometry corpus with before/after HTTP timings, coverage and Decimal references; suites `narrow`, `exponential`, `steps`, `dense`, `quadratic`, `mixed`, `transition` and `absolute` extend the default corpus. See the [release audit](../docs/benchmarks/GRAPH_RELEASE_AUDIT_2026_10_15.md), [narrow peaks](../docs/benchmarks/GRAPH_NARROW_FEATURES_2026_10_15.md), [exp tails](../docs/benchmarks/GRAPH_EXP_TAILS_2026_10_15.md), [step guards](../docs/benchmarks/GRAPH_STEP_GUARDS_2026_10_15.md), [dense sums](../docs/benchmarks/GRAPH_DENSE_SUMS_2026_10_15.md), [quadratic phases](../docs/benchmarks/GRAPH_QUADRATIC_PHASES_2026_10_15.md), [mixed ranges](../docs/benchmarks/GRAPH_MIXED_RANGES_2026_10_15.md), [subpixel curves](../docs/benchmarks/GRAPH_SUBPIXEL_CURVES_2026_10_15.md) and [absolute valleys](../docs/benchmarks/GRAPH_ABSOLUTE_2026_10_16.md). |
| `../scripts/benchmark_graph_audit_browser.cjs`, `../scripts/benchmark_graph_budget.cjs` | Actual SVG reproductions and installed Chromium/Chrome/Edge smoke checks; bounded graph rejection, queue waiting and recovery. Known rendering defects are recorded explicitly, not treated as successful graphs. |
| `../scripts/benchmark_graph_stress.cjs` | Sustained mixed, pole and precision-step graphs across desktop/portrait/landscape; unique bounded cache entries, settled view state, forced-GC/navigation cleanup and interaction telemetry. See [view reconciliation](../docs/benchmarks/GRAPH_VIEW_RECONCILIATION_2026_10_15.md). |
| `../scripts/benchmark_graph_multi.cjs --suite precision` | Four precision-fallback curves on desktop/mobile layouts; see the [camera correction and measured tradeoffs](../docs/benchmarks/GRAPH_PRECISION_CAMERA_2026_10_15.md). Default scenarios are unchanged. |
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

For multiple-root discovery, `numeric_root_scan_benchmark` measures 15 cases at
16/34/100 digits with three repetitions; `--quick` runs precision 34 once.
Validate emitted full coordinates with `python scripts/check_numeric_root_scan.py scan.csv`
from the repository root. See the [scan report](../docs/benchmarks/NUMERIC_ROOT_SCAN_2026_10_19.md)
and [local-candidate follow-up](../docs/benchmarks/NUMERIC_ROOT_CANDIDATES_2026_10_19.md)
for timing, allocation and explicitly incomplete discovery results.

For numerical roots, `numeric_root_benchmark --compare` records paired bisection
and private Brent runs at precisions 16/34/100 (three repeats). Add `--quick` for
one precision-34 pass, or `--adversarial` for the vanishing-jump domain regression.
The Python checker independently reevaluates emitted coordinates at 180 digits;
run it on the complete comparison CSV as well as the default baseline. Production
C/HTTP still uses bisection. See the [Brent experiment](../docs/benchmarks/NUMERIC_ROOT_BRENT_2026_10_18.md).

`--hybrid` records all three methods, adding bounded-decimal interpolation with
a progress-based permanent bisection fallback. It can be combined with --quick
or --adversarial. Statistics include fallback_steps and switch_offs; the checker
also validates their consistency. See the [hybrid follow-up](../docs/benchmarks/NUMERIC_ROOT_HYBRID_2026_10_19.md).

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
