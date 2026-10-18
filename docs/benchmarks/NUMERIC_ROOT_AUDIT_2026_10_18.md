# Numerical interval root audit — 18 October 2026

Historical baseline: the [domain-check follow-up](NUMERIC_ROOT_DOMAINS_2026_10_18.md)
now rejects the vanishing jump as domain_unverified. The current --adversarial
harness expects that diagnostic and passes; the failed runs below describe the
original solver at 5592092, not current behavior.

## Scope and reproduction

Baseline: `5592092`, unchanged production bisection. Windows x64, GCC 16.2
UCRT, Release with warnings as errors and optional allocation counters. This
is the first numerical-equation benchmark baseline, not a speedup comparison.
Timing covers expression preparation and the entire bounded C solve, including
cleanup. Bound parsing, result validation and text serialization are outside the
timer. Allocation bytes are cumulative requested bytes, not peak live memory or
RSS. No live-allocation ledger is enabled during timing.

With `NUMFORGE_BUILD_BENCHMARKS=ON`, build `numeric_root_benchmark`, then run:

```powershell
./build/equations-2026-10-17/gcc/numeric_root_benchmark.exe > build/numeric-roots.csv
python benchmarks/check_numeric_root_benchmark.py build/numeric-roots.csv
./build/equations-2026-10-17/gcc/numeric_root_benchmark.exe --adversarial
```

Default recording: 27 cases × precisions 16/34/100 × three serial repetitions,
243 rows. `--quick` runs all 27 at precision 34 once and is registered as a CTest
smoke test, without machine-specific timing thresholds. The C verifier checks
analytic/reference coordinates, residuals, expected diagnostic outcomes and work
caps. The separate standard-library Python checker reevaluates all 126 reported
coordinates at 180-digit Decimal precision, using independent exp/ln/sqrt and
sine/cosine series. It also compares independently evaluated and emitted
residuals to within 1E-25. It does not prove continuity or existence.

Raw local recordings: ignored `build/numeric-root-audit-2026-10-18/`.
The shared corpus also runs in the ordinary C unit suite at all three
precisions, independently of whether benchmarks are enabled.

## Results

All 243 ordinary rows matched the expected baseline behavior. All 126 found
coordinates passed independent residual reevaluation. A non-found diagnostic is
an expected limitation, not a successfully solved equation.

Representative median times at precision 34:

| Case | Outcome | Point pairs | Milliseconds |
| --- | --- | ---: | ---: |
| `cos(x)=x`, [0,1] | found | 43 | 7.63 |
| `exp(x)=2`, [0,2] | found | 44 | 8.59 |
| `ln(x)=1`, [1,4] | found | 45 | 10.37 |
| `x^3=2`, [0,2] | found | 45 | 4.35 |
| `(x-0.1234)^3=0`, [0,1] | found | 43 | 3.52 |
| `sin(1000*x)=0`, [0.003,0.0033] | found | 39 | 4.23 |
| `x+1E100=1E100+1/3`, [0,1] | found | 43 | 3.35 |
| `1E20*(x-0.1234)=0`, [0,1] | iteration limit | 98 | 6.54 |
| `x=1/3`, [-1E30,1E30] | iteration limit | 98 | 5.60 |
| `exp(x)=1E100`, [0,300] | iteration limit | 98 | 29.29 |
| `ln(x)=-100`, [1E-50,1] | iteration limit | 98 | 46.63 |
| `1/ln(ln(sqrt(x)))=0`, [2,10] | iteration limit | 98 | 34.73 |

Worst measured row: **77.04 ms**, precision 100, logarithm near the domain
boundary. Maximum cumulative allocation volume: **13,057,405 bytes**, also that
case; below the existing 64 MiB request allocation budget. No ordinary corpus
case reached the 5 s deadline or exhausted memory. This is a finite corpus and
does not establish a bound for arbitrary expression cost.

Exact rational midpoints resolve `x=1E100` immediately. Cancellation against a
common 1E100 offset preserves the small rational root. Sampled domain gaps,
hidden/scaled poles, ordinary jumps and plateaus retain their honest diagnostics.
Tangencies without endpoint sign changes remain unsupported.

## Confirmed correctness limitation

The separate `--adversarial` run **fails verification and exits 1** at all three
precisions. It is intentionally not registered as a passing smoke test:

```text
x-0.1234 + 1E-30*(x-0.1234)/abs(x-0.1234) = 0, interval [0,1]
```

Let t=x-0.1234. For t>0, the left side is t+1E-30>0; for t<0 it is
t-1E-30<0; at t=0 it is undefined. **There is no root.** Nevertheless, the current
solver returns `found`, at x=`271359469735/2199023255552`, with residual about
5.31E-14. Width, residual and endpoint improvement all pass. Independent
arithmetic confirms this is a small nonzero value, not an evaluator error.

The documented continuity assumption explains this outcome but does not make
the result an actual root. More digits, Brent interpolation, midpoint-neighbor
sampling or a stricter fixed residual threshold do not generally remove this
class of failure: the jump can be scaled smaller. Finite sampling cannot certify
arbitrary continuity. Production behavior has not been changed during this audit.

## Recommended sequence

1. **Domain checks before speed optimization.** Add conservative interval/domain
   analysis to the prepared expression path: division must exclude zero from
   its denominator range, logarithms require positive arguments, square roots
   require nonnegative arguments, and step functions need explicit handling.
   On unresolved domains, retain an unverified candidate/diagnostic instead of
   presenting a root as found. Avoid rejecting safe divisions merely because
   the formula contains `/`; the actual candidate interval matters. Exact typed
   zeros evaluated in the original expression can keep their separate path.
2. **Benchmark safeguarded Brent interpolation over BigRational/BigDecimal.**
   Reuse existing prepared expressions, residual checks and inherited budgets;
   keep bisection as the fallback. Compare evaluations, allocations and complete
   outputs against this baseline. Linear examples should particularly benefit.
   The graph's existing double-based refinement is not a replacement for this
   high-precision solver.
3. **Review stopping criteria and scaling as a separate contract change.**
   Absolute 1E-12 residuals are too strict for huge values, while 96 bisections
   cannot resolve arbitrary wide intervals or roots close to a domain boundary.
   Define relative/absolute coordinate and residual policies explicitly, with
   conditioning diagnostics and bounded work; do not silently loosen tolerances
   or assume displayed digits are correct root digits.
4. Only then add graph integration and multi-bracket/tangent discovery. These
   extend coverage and should not conceal the current domain limitation.

## Validation

- Eight C test groups, including the 81 new corpus checks, allocation-failure
  sweeps, inherited memory limits and forced deadline checks: passed GCC 16.2
  and MSVC 19.44 v143 with warnings as errors.
- GCC benchmark smoke and 243-row recording: passed; independent Python checker
  passed all 126 found coordinates.
- Separate adversarial recording: three explicit failures, still unresolved.
- No production algorithm, browser behavior, public limits or release version
  changed in this audit. No push or deployment performed.
