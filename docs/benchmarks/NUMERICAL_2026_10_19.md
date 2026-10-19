# Public numerical bisection baseline — 19 October 2026

This records the first single-root slice. The subsequent same-day
[scan extraction](NUMERICAL_SCAN_2026_10_19.md) moves scan orchestration into Numerical.

The first [Numerical API](../reference/NUMERICAL_API.md) extracts the production
single-bracket loop into the numeric library. Parsing, domain analysis and scan
orchestration remain application-owned. Private Brent/hybrid experiments reuse
the same sampling/acceptance loop through internal strategy hooks.

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
