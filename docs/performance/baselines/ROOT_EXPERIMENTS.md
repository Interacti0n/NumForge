# Equation root-search comparisons — 18–19 October 2026

These are historical private application comparisons. Public/HTTP equation
solving selects bisection; graph marker refinement uses a separate Brent path.
The tested method-selecting diagnostic API remains available. Its superseded
boolean wrapper was removed in the [structural audit](../../project/AUDIT.md).

## Domain safeguards

The initial audit exposed a false root for `t+1E-30*t/abs(t)=0`, t=x-0.1234.
Original-expression interval checks now withhold it as `domain_unverified` at
16/34/100 digits. Unknown ranges do not establish continuity; exact typed zeros
retain pointwise checks. The 243-row domain comparison retained all expected
ordinary outcomes and independently checked 126 found coordinates at 180 digits.
Its worst row was 78.23 ms, maximum traffic 13,057,405 bytes. These finite tests
do not certify existence, uniqueness, completeness or arbitrary domain validity.
The [HTTP reference](../../reference/NUMERIC_ROOT_HTTP_API.md) owns those rules.

## Private interpolation decision

GCC 16.2 UCRT Release with allocation counters, live tracking off, serial runs;
27 cases × 16/34/100 digits × three repeats × three methods = 729 rows. The
independent Decimal-180 checker checked all 417 found coordinates, and nine
adversarial rows withheld the vanishing-jump false root.

Rational Brent retains exact interpolation coordinates but can generate costly
intermediates. The decimal hybrid rounds interpolation weights, checks every
proposal through original expressions and permanently falls back to bisection
after poor progress. Neither method relaxes residual, bracket, domain or final
higher-precision checks. All attempts share one budget; failures do not restart.

Precision-34 complete-solve medians in milliseconds:

| Case | Bisection | Rational Brent | Decimal hybrid |
| --- | ---: | ---: | ---: |
| cos(x)=x | 8.73 | 6.21 | **4.53** |
| exp(x)=2 | 10.55 | 7.04 | **5.55** |
| ln(x)=1 | 10.24 | 6.82 | **5.08** |
| x^3=2 | 4.75 | 4.18 | **2.78** |
| sqrt(x)=0.1 | 3.16 | 1.43 | **0.94** |
| sin(1000*x)=0 | 5.01 | 2.76 | **2.05** |
| large-offset cancellation | 2.92 | 0.91 | **0.86** |
| abs(x-0.1234)=x | 4.02 | 1.57 | **1.56** |
| (x-0.1234)^3=0 | **4.51** | 86.61 | 10.23 |
| 1/(x-0.1234)=0 | **6.92** | 69.50 | 8.92 |
| scaled pole | 11.99 | 104.95 | **10.82** |
| nested logarithm pole | **45.11** | 177.59 | 48.27 |

The flat cubic switches once and uses 36 subsequent midpoint steps. The hidden
and scaled poles switch once and use 91 midpoint steps. The hybrid avoids the
first experiment's large regressions there, but the flat cubic remains about
2.3× slower than pure bisection. Small already-trivial cases can also cost more
because interpolation setup is unnecessary. Measurements vary with the host;
small differences are not portable speed guarantees.

The hybrid retains the steep-linear and wide-linear improvements (three and
seven point pairs respectively, rather than the baseline's step limit). It does
**not** retain every old-Brent coverage improvement: exp(x)=1E100 at precision
100 is found by rational Brent in 22 pairs, but the hybrid's progress policy
disables interpolation and reaches the step limit. The logarithm near its domain
boundary also remains unconverged. Known non-found diagnostics count as expected
behavior, not solved equations.

Worst hybrid row: **123.57 ms**, precision-100 boundary logarithm, with
**13,342,508 cumulative requested bytes**. This is not peak live memory or RSS.
No measured hybrid request reached the five-second or 64 MiB budget.


Keep production bisection: the hybrid's flat-root regression and lost huge-
exponential coverage do not justify an unconditional switch. Timing, convergence
and discovery coverage must be evaluated together. Research code is used by
benchmarks/failure tests and is not an unused public feature.

## Local candidate discovery

Baseline `d61ffde`, GCC 16.2 UCRT Release, allocation counters on/live tracking off;
15 cases × three precisions × three serial repeats = 135 rows. Exact midpoint
and quadratic vertex proposals improve discovery at additional sampling cost.
Only an actual typed exact zero or an accepted original-expression bracket may
publish a root. A fitted minimum or small rounded residual alone is insufficient.

Median milliseconds at precision 34:

| Case | Original scan | Local candidates | Result |
| --- | ---: | ---: | --- |
| Three polynomial roots | 5.83 | 16.85 | Same three roots |
| Seven sine roots | 54.21 | 87.03 | Same seven roots |
| cos(x)=x | 25.40 | 60.97 | Same root |
| Partial logarithm domain | 36.94 | 56.01 | Same root, domain diagnostics |
| Off-grid quadratic contact | 6.17 | 17.38 | Previously missed; now exact 0.1234 |
| Two roots in one grid cell | 5.46 | 19.68 | Previously missed; now 0.01 and 0.02 |
| Rational contact at 1/3 | — | 22.20 | Exact recurring fraction |
| Pair separated by 2E-10 | — | 35.71 | Both checked coordinates |
| Positive minimum 1E-100 | — | 26.41 | No false root |
| General cos-square contact | — | 92.53 | Still unconfirmed; limitation retained |
| Evaluation ceiling | 221.93 | 229.12 | Bounded stop, no root |

Additional discovery costs about one extra sample per same-sign cell; this is
a deliberate coverage tradeoff, not a speedup claim. Sine at precision 34 uses
630 coordinate pairs and 11,918,356 cumulative allocation bytes; the previous
scan used 504 pairs/8,233,344 bytes. The worst final row was **298.24 ms**, repeated
steps at precision 100, with 3962 pairs and 40,493,624 cumulative bytes. Tested
cases remain within one five-second/64-MiB budget. Arbitrary expressions can
still exhaust cooperative time/memory before the coordinate ceiling.

The independent Decimal-180/Fraction checker passed all 135 rows and **450 full
coordinates**: 162 finite-root coordinates and 288 intentionally capped identity
grid points. It includes known polynomial/π/fixed-point references, independently
evaluated cosine residuals, empty diagnostic cases and new candidate counters.


## Reproduction

Build opt-in targets `numeric_root_benchmark` and `numeric_root_scan_benchmark`.
Save raw output locally; use current executable paths rather than dated build
folders from older diaries:

```sh
numeric_root_benchmark --hybrid > build/root-comparison.csv
python benchmarks/tools/check_numeric_root_benchmark.py build/root-comparison.csv
numeric_root_benchmark --hybrid --adversarial
numeric_root_scan_benchmark > build/root-candidates.csv
python scripts/check_numeric_root_scan.py build/root-candidates.csv
```

The current [public callback baselines](ROOTS.md) measure a different layer.
No new benchmark was run during documentation consolidation.
