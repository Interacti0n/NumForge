# Bounded theta/AGM logarithm integration — 11 October 2026

The earlier fixed-guard prototype is research history, not the acceptance rule.
The production path is a separate outward-rounded interval implementation in
`src/core/bigdecimal/logarithm_agm.c`, reached through the existing `bigdecimal_ln`.
No installed header, function syntax, calculator budget or release version changes.

## Selection and failure contract

The private dispatcher selects interval AGM for 2000–10000 significant digits.
Lower and higher precision requests retain the original reduced atanh series;
10000 is a dispatch bound, not a new public C precision cap. The inclusive
input band `[0.999, 1.001]` also keeps the well-conditioned series. One is
exact zero. Nonpositive real inputs and invalid rounding modes retain errors.

AGM starts with 40 extra working digits. If the final interval endpoints do not
round to the same requested value, it retries with 80, 160, 320 and at most 512
extra digits. All attempts share the caller's budget. Unsupported representation
or unresolved precision can fall back to the series only when no budget failure
has occurred. Allocation and deadline errors never restart computation.

Every scratch owner is per-call. A final successful copy publishes the result;
failed attempts leave the destination and original input unchanged, including
aliases. No global pi cache or implicit C API deadline is introduced.

## Error enclosure

Positive interval addition, multiplication and division use downward/upward
significant rounding. Division reverses denominator endpoints. Subtraction
uses `[a_lo-b_hi, a_hi-b_lo]`. Square roots use the existing directed, integer
root/sticky-tail implementation. Both endpoints are evaluated before replacing
an input interval. Negative logarithms negate and swap the final endpoints.

Argument reciprocation and repeated squaring retain endpoint bounds. Expansion
continues until the lower bound for `y` reaches `10^ceil((work+4)/36)`, ensuring
`q=1/y <= 0.1`. Using the theta identity from
[Modern Computer Arithmetic, sections 4.8.3–4.8.4](https://maths-people.anu.edu.au/~brent/pd/mca-cup-0.5.9.pdf):

```text
theta2(q^4) = 2(q + q^9 + q^25 + ...)
theta3(q^4) = 1 + 2(q^4 + q^16 + q^36 + ...)
ln(y) = pi / (4 AGM(theta2(q^4)^2, theta3(q^4)^2))
```

The complete omitted positive tails, not just their first terms, are bounded by
`2*q^49/(1-q^32) < 3*q^49` and `2*q^36/(1-q^28) < 3*q^36`.
These bounds are added to the upper theta endpoints. AGM is monotone in both
arguments; after its first step, geometric lower and arithmetic upper endpoints
enclose its exact limit. Equality of approximate iterates is not the acceptance
criterion. Rounded agreement at a higher precision merely decides when to try
the final interval calculation.

Pi is enclosed independently with outward-rounded Gauss-Legendre iteration,
rather than assuming that the approximate public constant factory is certified.
For its exact `a_n >= b_n > 0`, `d_n=a_n-b_n`, `p_n=2^n` and
`t_n=1/4-sum_(j<n) 2^j*(a_j-a_(j+1))^2`, the gap satisfies
`d_(n+j) <= d_n/2^j`. Since `a_j-a_(j+1)=d_j/2`, the entire remaining correction
is bounded by `p_n*d_n^2/2`. Together with `b_n <= M <= a_n` and
`pi=M^2/t_infinity`, this gives

```text
b_n^2/t_n <= pi <= a_n^2/(t_n - p_n*d_n^2/2).
```

Interval endpoints extend these bounds to include arithmetic rounding. The
upper denominator must remain strictly positive. Dividing the pi enclosure
by the AGM enclosure and the exact expansion factor encloses the real logarithm.
The result is accepted only when both endpoints round identically under the
requested mode. This relies on the existing directed arithmetic/root contracts;
it is not a blanket correctly-rounded guarantee for the series fallback,
logarithm ratios or composed complex functions.

## Independent and failure checks

Python standard-library Decimal supplies independent logarithm references at
two higher working precisions. Checked-in fixtures cover six output modes, reciprocal and
extreme decimal arguments, exact one, both edges of the near-one fallback band,
and two exact decimal inputs roughly 200 digits from opposite sides of a
34-digit rounding midpoint. The latter force extra-precision retries and apply
only to the interval path, not the earlier fixed-guard experiment.

The extended smoke fixture has 204 rows. Both native MSVC/GCC runs compare 792
complete values across the fixed-guard and interval paths; Win32 and portable
division each compare 408 interval values. Optional references through 5000
digits exercise the public dispatcher. Aliased outputs and live-allocation
cleanup alternate across these checks. Additional unit regressions cover all
six modes at 2000 digits, an aliased principal complex logarithm, a custom base,
invalid rounding and deterministic cancellation through the production entry.

The exhaustive interval failure harness checks 86002 allocation/deadline
checkpoints with separate/aliased destinations, comparing successes to the
independent series and failures to the original destination, with no live
scratch left. CTest registers four disjoint shards (separate/aliased output
crossed with allocation/deadline injection), each with its own timeout, rather
than placing all 86002 checkpoints under one deadline. The optional unsharded
`--fault-checks-interval` command remains available for local diagnosis.
Speed is not a CI assertion.

The final four registered shards all pass: 26.04/93.70/24.86/88.30 seconds,
139.22 seconds total with `ctest -j 2`. The earlier single registration hit its
300-second wall limit under concurrent validation; sharding preserves every
checkpoint while giving each independent case a separate timeout.

## Fresh serial measurements

Ryzen 7 7435HS, Windows x64, MSVC 19.51.36247.0, Release with benchmark
instrumentation. Three fresh processes per algorithm/precision, run serially
after correctness checks and before the final shard verification/build. CPU
frequency/affinity and unrelated background tasks were not controlled. Every
complete result matches its independent reference. Medians for `ln(2)`:

| Significant digits | Original series | Public dispatcher |
| --- | ---: | ---: |
| 34 | 0.134 ms | 0.116 ms |
| 250 | 0.958 ms | 1.174 ms |
| 1000 | 26.143 ms | 28.872 ms |
| 2000 | 200.449 ms | 97.159 ms |
| 5000 | 3260.157 ms | 661.568 ms |

Below 2000 both columns execute the same series; their small differences are
run variability, not a different algorithm. At 2000 the bounded path is about
twice as fast; at 5000 about five times. Its complete canonical text conversion
at 5000 takes 0.157 ms median outside the calculation timer. Earlier reports
had slower absolute times; compare paired algorithms from the same recording.

The complete calculator `ln(2)` request at 5000 decimal places, Plain notation,
has a 632.406 ms median (three full-output comparisons) under its actual 5000 ms
deadline and unchanged allocation/output budgets. The first sample requested
40385440 cumulative bytes and produced 5002 bytes. A separate public C API
live-memory probe requests 41794394 bytes, peaks at 259067 live bytes and frees
all scratch. The cumulative traffic and live-memory figures are different
metrics; memory instrumentation is not included in the speed medians.

An HTTP request at 5000 places also matches all 5002 reference bytes through a
temporary loopback server. This validates the server pipeline; it is not a
measurement of network or multi-user throughput. No existing server or tunnel
was replaced. Recordings, scripts and references are retained locally in
ignored `build/logarithm-interval-2026-10-11`.

Native MSVC passes 24 selected suites including arithmetic, public API,
application/session, numeric/division/complex references and the extended smoke.
Native GCC passes the corresponding 23 core suites; the changed composition and
cancellation suites pass again after their additions. Both public-dispatcher
high-precision runs compare 960 complete values through 5000 digits. Additional
adaptive 2000-digit inputs pass 24 public checks on each native compiler and 48
forced-interval checks on MSVC. Win32 and portable division each pass five
selected production suites and eight complete interval references at
2000/5000 digits. Portability checks were run concurrently, with diagnostic
60-second budgets, and do not establish five-second completion on those paths.

## Reproduction

Enable `NUMFORGE_BUILD_BENCHMARKS=ON`. The diagnostic CLI accepts
`agm|interval|series|public`, where `agm` retains the earlier fixed-guard
experiment, `interval` forces the bounded path with supplied extra digits,
`series` forces the original series, and `public` exercises the dispatcher.
The `attempts` CSV column reports interval attempts for the forced interval
mode; the public API does not expose this diagnostic state.

```sh
ctest --test-dir build/factorial-bench -C Release -R logarithm_agm --output-on-failure
python benchmarks/tools/generate_logarithm_agm_cases.py --max-digits 5000 --output build/logarithm-interval/cases.tsv
node benchmarks/tools/check_logarithm_agm_benchmark.cjs build/factorial-bench/Release/logarithm_agm_benchmark.exe build/logarithm-interval/cases.tsv public
```

Generated references and measurement recordings belong under ignored `build/`.
Earlier review/prototype reports remain in Git history at `c63e5c9`.
