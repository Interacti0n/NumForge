# Decimal-weight root hybrid — 19 October 2026

Follow-up to the [Brent experiment](NUMERIC_ROOT_BRENT_2026_10_18.md), baseline
`3775343`. The third private method reduces interpolation cost and permanently
returns to plain bisection on poor progress. **Production C/HTTP still selects
bisection.** No browser option, public SDK ABI or acceptance tolerance changed.

## Method

`application_numeric_root_method` selects baseline bisection, the original
rational Brent variant or the new decimal hybrid. The older boolean experiment
entry point remains available. Both interpolation paths retain rational
coordinates and the same Brent interior/displacement guards. The hybrid uses
the centered form `b + (a-b)*wa + (c-b)*wc`: only the bounded significant-digit
weights are formed with BigDecimal division. Repeated ordinates select a secant.
The original expressions still supply every actual sample and final check.

Ordinary proposal coordinates use 24 significant digits to avoid carrying
50–116 incidental digits into later rational bisections. Full working proposal
precision is retained when the bracket is already <=1E-12 but the residual still
exceeds its target. At coordinate magnitudes >=1E12, an eight-digit coarse common
offset is removed before local quantization and restored exactly afterward.
This preserves small components near ±1E100. Minimum displacement uses the
corresponding local coordinate scale. All rounded proposals must stay strictly
inside the bracket; otherwise the method bisects. These proposal rules do not
relax the coordinate, residual, domain or higher-precision final checks.

The request permanently disables interpolation when either condition holds:

- Four consecutive rejected proposals without the best residual halving.
  A halving clears the rejection streak so useful bisections near a corner
  do not prematurely prevent the next successful interpolation.
- After twelve initial steps, a four-step checkpoint fails to contract the
  bracket by at least a factor of eight. Tiny flat-root residuals cannot replace
  bracket contraction in this policy.

After disabling, the main solver uses its existing rational midpoint path and
does not update interpolation state. It does not restart or expand the bracket,
reset the budget or add another iteration allowance. One-way switch-off occurs
at most once. Statistics distinguish rejected proposals from subsequent plain
bisection fallback steps. Approximate-zero quarter probes and exact typed zeros
keep their existing behavior; disabled interpolation is never reenabled.

## Recording

Windows x64, GCC 16.2 UCRT Release/WX, optional allocation counters, no live
allocation tracking during timing. Twenty-seven cases × three precisions
(16/34/100) × three repetitions × three methods: **729 serial rows**.
All rows passed C contract/statistic checks. The independent standard-library
Python checker reevaluated all **417 found coordinates** at 180 digits and
checked the published residuals. The separate nine-row adversarial comparison
also passed, withholding the vanishing-jump false root for all methods.

```powershell
./build/equations-2026-10-17/gcc/numeric_root_benchmark.exe --hybrid > build/root-hybrid.csv
python benchmarks/check_numeric_root_benchmark.py build/root-hybrid.csv
./build/equations-2026-10-17/gcc/numeric_root_benchmark.exe --hybrid --adversarial
```

`--hybrid` includes all three methods; `--compare` retains the previous two.
Add `--quick` for one precision-34 repetition. New CSV fields fallback_steps and
switch_offs supplement the existing counters. Raw recordings are local/ignored
under `build/numeric-root-hybrid-2026-10-19/`. Earlier tuning recordings under
the previous date are not the final measurements.

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

## Validation and decision

- GCC 16.2 Release/WX/allocation statistics: all seven numerical CTest entries
  passed (19.63 s): six benchmark smoke/domain checks and sixteen numerical unit
  groups. The shared corpus covers both interpolation variants at all three
  precisions, including bracket containment, counters, actual fallback and
  unchanged previously-found reference roots.
- All three methods undergo the complete narrow-interval allocation-failure
  sweep, memory/deadline checks and atomic root/statistic output checks.
  Both interpolation methods undergo 64 distributed iterative failures and
  distributed deadline checks. Tracked live allocations return to zero.
- Added large-offset checks: cos at +1E100 and -1E100, and sqrt at +1E100, with
  independently known small local coordinates. Invalid method selection preserves
  previous outputs. Existing domain and residual-stability regressions pass.
- MSVC 19.44 v143/WX: full build and all 60 CTest entries passed (37.89 s).
- No browser implementation changed in this follow-up. Existing production
  HTTP/server checks ran in CTest; browser tests were not unnecessarily repeated.

Keep all three methods available privately and keep production bisection for
now. The hybrid is a useful candidate for expensive smooth expressions, but
neither its flat-root cost nor lost huge-exponential coverage supports replacing
the default unconditionally. A future method-selection policy must assess both
cost and convergence coverage, rather than only counting evaluations. Further
micro-optimization is not needed to establish that the measured cases fit the
request time budget; graph integration can proceed independently of this choice.

No push, release, deployment or live-server/tunnel change is included.
