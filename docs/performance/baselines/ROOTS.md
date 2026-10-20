# Public root-search baselines — 19 October 2026

These observations measure standalone public C callbacks, independently of the
expression engine and HTTP adapters. They establish initial baselines rather
than an extraction speedup. See the [root API](../../reference/ROOT_API.md) for
current contracts and [private comparisons](ROOT_EXPERIMENTS.md) for research.

## Single root
With a Release benchmark build:

```sh
cmake --build build/bench --target numerical_benchmark
build/bench/numerical_benchmark --high-precision
```

Adjust configuration paths and `.exe` on Windows. Default smoke covers 12 and
100 places; the optional argument adds 1000. Each row solves `x*x-2=0` on `[0,2]`
with both absolute tolerances `1E-digits` and working scale `digits+20`. The
callback uses exact rational arithmetic. Independent Newton decimal sqrt and
original residual checks run outside timing.

One Windows x64 GCC 16.2 Release observation while other verification ran:

| Absolute places | Time, ms | Iterations | Evaluations | Allocation requests | Requested bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| 12 | 1.395 | 42 | 45 | 11,705 | 202,371 |
| 100 | 65.542 | 335 | 338 | 199,435 | 8,034,623 |
| 1000 | 18,944.926 | 3324 | 3327 | 11,940,195 | 4,708,937,497 |

All rows passed value and residual checks. These single-run diagnostics are
neither cross-machine estimates nor an optimization comparison. Requested bytes
are cumulative traffic, not measured peak live memory. A trial with a 512 MiB
cumulative budget rejected the 1000-place case. The recorded run uses 120 seconds,
unlimited cumulative requested bytes and an 8 MiB single-allocation cap; application
budgets are unchanged. On 32-bit hosts the optional stress can exhaust the
representable cumulative counter/budget.

Adversarial rational-Brent/hybrid comparisons and scan smoke pass separately.
Future acceleration should preserve the callback, continuity, residual and
higher-precision safeguards rather than duplicating the search kernel.

## Multiple roots



Configure a Release build with `NUMFORGE_BUILD_BENCHMARKS=ON`, then:

```sh
cmake --build build/bench --target numerical_scan_benchmark numeric_root_scan_benchmark
build/bench/numerical_scan_benchmark
build/bench/numeric_root_scan_benchmark --quick
```

Adjust configuration directories and `.exe` on Windows. The public harness runs
three repetitions per case, with the default working scale 34, absolute tolerances
`1E-12`, 32-root capacity and 4096 total evaluations. Each bracket retains its
96-iteration/192-evaluation allowance. A caller-owned budget limits each operation
to 30 seconds, 512 MiB cumulative requests and 8 MiB per request.

Quadratic roots are compared with independent decimal Newton sqrt and their
residuals outside timing. Contacts are compared exactly with supplied rational
coordinates, including a common offset `1E1000+1/3` and a tiny coordinate
`1E-1001` inside `[0,1E-1000]`. Those cases test exact arithmetic range, not a new
1000-place approximation guarantee. All callback counts are checked against the
returned evaluation total.

### Observations

Windows x64, GCC 16.2, Release; other local verification was running. Medians of
three observations from the [raw rows](../data/NUMERICAL_SCAN_2026_10_19.csv):

| Case | Grid cells | Roots | Median ms | Evaluations | Requested bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| `x*x-2` | 128 | 2 | 10.542 | 333 | 1,746,872 |
| Both quadratic roots in one cell | 1 | 2 | 3.042 | 93 | 416,622 |
| Rational contact at `1/3` | 128 | 1 | 8.527 | 258 | 1,461,984 |
| Contact at `1E1000+1/3` | 128 | 1 | 12.471 | 258 | 3,543,528 |
| Contact at `1E-1001` | 128 | 1 | 87.389 | 258 | 18,471,822 |

All 15 rows passed. Requested bytes are cumulative allocation traffic, not peak
live memory. These small local samples are diagnostics, not timing gates or a
before/after speedup claim. No scan claims complete root enumeration.

The existing application scan harness also passed all 15 quick rows. Comparing
against the pre-extraction baseline preserved statuses, stop reasons, root counts,
full coordinates, evaluation/iteration counts, scanned segments, grid domain gaps,
rejected brackets and candidate counters. Adversarial Brent and hybrid comparison
rows remained verified. Work/time/allocation failure tests require atomic results;
only explicit root/evaluation limits return successful partial lists.

The scan unit suite also passed a separate allocation-instrumented GCC build:
each test, including stratified allocation failures and partial discovery,
finished with zero tracked live allocations. Timing rows above do not enable
that additional live-allocation probe. Static GCC analysis and installed C/C++
numeric-only consumers passed separately.
