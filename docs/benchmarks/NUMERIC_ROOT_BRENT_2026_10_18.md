# High-precision Brent experiment — 18 October 2026

Baseline: `872b4bd`, following the [domain checks](NUMERIC_ROOT_DOMAINS_2026_10_18.md).
**Do not enable this experiment globally yet.** It improves several cases but
has substantial regressions at flat roots and poles. Production C/HTTP still
selects bisection; both methods now use the final residual stability check below.

## Experiment

Private `numeric_root_experiment.h` selects baseline bisection or a guarded
Brent-Dekker variant. No HTTP option, browser selector or installed SDK API was
added. Both paths share prepared original expressions, domain checks, tolerances,
96 iterations, 192 point-pair evaluations and the inherited request budget.

The experiment maintains an opposite-sign bracket, keeps the endpoint with the
smaller residual, and attempts secant or inverse-quadratic interpolation. Brent
displacement/interior guards reject unsuitable proposals in favor of bisection.
Proposals have a working-precision minimum displacement and are explicitly
quantized to finite decimal coordinates, then stored exactly as BigRational.
All interpolation arithmetic uses our BigRational/BigDecimal types; no double
coordinate/value conversion is used. Interpolation-only ordinates are rounded
to working precision to avoid carrying incidental exact polynomial digits.
Original samples remain available for acceptance checks. Rounded proposals
outside the current bracket fall back to bisection, as do guaranteed failures of
the zero-displacement guard. Approximate-zero quarter probes reset Brent state.

Statistics distinguish selected bisections, interpolations, secant/IQI steps,
minimum-step nudges and rejected interpolation proposals. Bisections can still
pay for rejected IQI work. Structural/resource failures preserve both the root
destination and statistics; numerical diagnostics publish no fake root.

## A correctness defect exposed by the experiment

At precision 34, the search works at 50 digits. For exp(x)=1E100, a displayed
residual of zero can reflect rounding of a very large value, rather than a
small actual difference. Independent 180-digit Decimal reevaluation rejected
the first experimental result. This is not a continuity defect.

The old production bisection is affected too. A side-by-side executable built
from the actual 872b4bd source reproduced it using:

```text
equation: exp(x)=1E100
precision: 34
min: 230.25850929940456840179914546843642076010
max: 230.25850929940456840179914546843642076012
```

The old solver returned found, residual 0, at the rational coordinate
`3164648854613294379387911415512579287743899362315999/13743895347200000000000000000000000000000000000000`.
Independent evaluation gives an absolute residual about **2.464112511043E+50**.
The current solver returns precision_limit and no coordinate for the same input.
The local reproduction, old source/driver and CSV are retained under ignored
`build/numeric-root-brent-2026-10-18/`.

Before publishing a non-exact candidate, **both methods now prepare/evaluate the
original sides again at twice their working precision**, under the same budget.
The checked residual must satisfy 1E-12 and initial endpoint improvement; an
unstable nonzero sign also fails. Disagreement produces precision_limit, with
no root. Published residuals use this final evaluation. The extra point pair
counts toward 192; the search reserves capacity for it. Exact typed rational
zeros retain their separate pointwise path.

This is a stability check, not rigorous error propagation. Arbitrarily severe
cancellation can exceed both precisions; certified existence/error bounds still
require stronger interval/error methods. Old browser numerical results are
invalidated again (stored policy version 2), while inputs remain available.

## Measurements

Windows x64, GCC 16.2 UCRT, Release/WX, optional cumulative allocation counters,
no live-allocation ledger during timing. Same input corpus at precisions
16/34/100, three serial repeats, paired methods: **486 rows**. Timing includes
preparation, solving, final verification and internal cleanup; bound parsing and
CSV serialization/reference checks are outside timing.

```powershell
./build/equations-2026-10-17/gcc/numeric_root_benchmark.exe --compare > build/root-comparison.csv
python benchmarks/check_numeric_root_benchmark.py build/root-comparison.csv
./build/equations-2026-10-17/gcc/numeric_root_benchmark.exe --compare --adversarial
```

Raw recording: `build/numeric-root-brent-2026-10-18/gcc.csv`. All rows passed the
C contract/invariant checks; all **273 found coordinates** passed independent
180-digit original-expression residual checks. Some previously nonconvergent
cases now have analytic reference roots, so newly found results can be checked.
Diagnostic changes are allowed for those cases; losing a baseline-found root or
finding a root in a known no-root/domain case fails verification.

Precision-34 medians (milliseconds and point pairs, including final verification):

| Case | Bisection ms / pairs | Brent ms / pairs | Brent result |
| --- | ---: | ---: | --- |
| cos(x)=x | 9.12 / 44 | 6.48 / 10 | found |
| exp(x)=2 | 9.85 / 45 | 7.98 / 12 | found |
| ln(x)=1 | 10.27 / 46 | 7.50 / 12 | found |
| x^3=2 | 4.89 / 46 | 4.18 / 11 | found |
| (x-0.1234)^3=0 | 3.92 / 44 | 79.72 / 89 | found, much slower |
| 1E20*(x-0.1234)=0 | 6.69 / 98 | 0.21 / 3 | found instead of step limit |
| x=1/3 over [-1E30,1E30] | 5.60 / 98 | 1.71 / 7 | found instead of step limit |
| large-offset cancellation | 2.92 / 44 | 0.80 / 6 | found |
| sqrt(x)=0.1 | 3.44 / 45 | 1.35 / 6 | found |
| sin(1000*x)=0 | 4.71 / 40 | 2.76 / 8 | found |
| abs(x-0.1234)=x | 2.38 / 44 | 0.93 / 8 | found |
| 1/(x-0.1234)=0 | 7.51 / 98 | 70.41 / 98 | step limit, much slower |
| 1E-100/(x-0.1234)=0 | 9.40 / 98 | 106.94 / 98 | step limit, much slower |
| nested logarithm pole | 40.57 / 98 | 167.79 / 98 | step limit, much slower |

At precision 100, Brent finds exp(x)=1E100 in 22 point pairs with a checked
residual satisfying the tolerance; precisions 16/34 correctly return
precision_limit. The boundary case ln(x)=-100 still reaches the step limit.
Worst recorded row: **538.09 ms**, precision-100 nested logarithm pole. That
case requested **18,974,205 cumulative allocation bytes**, below the shared
64 MiB limit. No recorded request reached five seconds. Separate timing runs
are noisy; individual ratios are observations, not portable speed guarantees.

## Decision and next experiment

Keep production bisection. Retain the tested residual-stability fix in production,
but do not trade the flat-root/pole regressions for the favorable cases above.

The next useful experiment is a cheaper bounded-precision interpolation path:
compute ratios/weights in BigDecimal rather than constructing long exact IQI
fractions, then verify the actual rational proposal through the original
expressions. Add a progress policy that disables interpolation after repeated
rejections or poor bracket shrinkage and spends the remaining request on
bisection. Measure this against both current methods, including the adverse
cases; fewer evaluations alone are not a promotion criterion. Tolerance scaling,
bracket discovery, tangent detection and graph integration remain separate work.

## Validation

- GCC 16.2 Release/WX/allocation statistics: 15 numerical test groups passed,
  including both-method corpus checks, interval/statistic invariants, every
  observed allocation failure of a complete narrow-interval solve/final check,
  64 distributed IQI-path failures, inherited memory limits and forced deadlines.
  No tracked live allocations remain. Four benchmark smoke/domain checks passed.
- MSVC 19.44 v143/WX: full build and 60/60 CTest entries passed (39.54 s).
- Independent Python reference: 486 comparison rows, 273 found coordinates.
- Real HTTP and browser regressions cover unstable huge-value residuals and
  previous-policy result invalidation in addition to the domain checks. All 19
  Chromium equation/guide tests passed (44.1 s), including both languages and
  phone/landscape/desktop layouts.

No push, release, deployment or live-server/tunnel change is included.
