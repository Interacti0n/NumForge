# Rational arithmetic, conversion and comparison — 30 October 2026

Windows x64, Ryzen 7 7435HS (16 logical CPUs), approximately 32 GiB RAM,
GCC 16.2 Release (`-O3 -DNDEBUG`). This comparison starts at numeric source
`033eb5b`, after the first rational optimization pass, and ends at `bfc785e`
(including `1b9df30`). It measures additional gains over that already optimized
implementation. Public precision, rounding, root tolerances, iteration policy
and application resource defaults are unchanged.

Both builds enable allocation counters and test failure hooks; neither uses
sanitizers or coverage. Profile clocks and live-payload tracking run separately
from timing. The retained before rational binary is byte-identical to the
verified first-pass binary. The before numerical binary adds only the current
diagnostic/limit CLI harness against the unchanged numeric library. Metadata
records executable hashes, source identities and compiler settings.

## Changes and safety boundaries

- Decimal comparison uses conservative integer bounds on coefficient digit counts
  to prove disjoint decimal exponents. Overlapping bounds fall back to exact digit
  counts and coefficient alignment. It no longer serializes operands to decimal
  strings. Sign, equality and extreme scales remain exact.
- A terminating reduced fraction with denominator `2^a * 5^b` constructs its
  coefficient directly as `n * 2^(t-a) * 5^(t-b)`, where `t = max(a,b)`, instead
  of performing general long division. Common powers of ten are removed first
  with the existing bounded-workspace decimal factor routine. This avoids a
  temporary-memory regression for mixed denominators.
- Rational-to-decimal conversion borrows canonical numerator/denominator
  descriptors and avoids copying them merely to repeat normalization and GCD.
  Owned temporaries still commit only after success. Finite results retain all
  their digits; recurring results retain all six rounding modes.
- Integer exponentiation moves its final owned accumulator into the output
  instead of copying it. Aliases, allocation failures and deadline checks retain
  their public behavior.

## Complete public root search

Three alternating before/after fresh-process runs of
`numerical_benchmark --high-precision`, with no other build/test/benchmark from
this review running alongside them. There is no affinity or frequency lock;
unrelated host activity is not controlled. Initial grouped runs showed host
speed drift and are excluded from the selected comparison.

This is the exact-rational callback for `x*x-2` on `[0,2]`, not the direct decimal
`sqrt` operation. Working precision is absolute places plus 20, with width and
residual tolerances of `1E-places`. Independent Newton sqrt and original
residual checks run outside timing. Iterations/evaluations are unchanged:
42/45, 335/338 and 3324/3327 respectively.

| Absolute places | Before median ms | After median ms | Before / after allocation calls | Before / after requested bytes |
| ---: | ---: | ---: | ---: | ---: |
| 12 | 0.2685 | 0.2361 | 4,059 / 2,693 | 71,571 / 34,955 |
| 100 | 4.1264 | 2.5340 | 36,069 / 26,079 | 1,757,607 / 853,139 |
| 1000 | 1,340.2893 | 202.4467 | 373,794 / 280,508 | 139,837,913 / 64,827,091 |

The 1000-place solve is approximately **6.6 times faster**, with 25.0% fewer
allocation calls and 53.6% less requested traffic. Its three before timings
span 1305.9–1363.3 ms; after timings span 199.8–214.8 ms. The 100-place median
improves by about 1.6 times. The short 12-place after runs span 0.1853–0.5564 ms,
so that row does not establish a reliable small-case speedup.

Requested bytes are cumulative traffic, not peak/live memory or RSS. Separate
`--allocation-limit-mib 64` runs now succeed at all three precisions, including
the independent validation inside the same budget. The before implementation
stops at 1000 places after 67,109,021 requested bytes with allocation-budget
failure. The reported after solve traffic is 61.8 MiB; validation is outside
that counter snapshot but still inside the enforced 64 MiB budget. The harness
retains a 120-second deadline and 8 MiB single-allocation cap. This result does
not establish success for arbitrary callbacks or application workloads.

## Direct conversion

Medians of three process medians, five calibrated samples per row, with the
versions interleaved and order reversed in the middle repetition. Fixture
construction, complete text checks and alias checks are outside timing. The
same canonical operands are used in both versions; the bit labels identify
generator parameters rather than every component's exact bit count. All rows
below use HALF_EVEN conversion.

| Fixture | Before / after microseconds | Before / after allocation calls | Before / after requested bytes |
| --- | ---: | ---: | ---: |
| Dyadic, 1024 parameter | 16.029 / 6.960 | 37 / 18 | 5,379 / 1,792 |
| Power of five, 1024 parameter | 105.795 / 16.386 | 104 / 73 | 16,834 / 9,960 |
| Mixed terminating, 1024 parameter | 37.411 / 36.922 | 28 / 9 | 1,718 / 952 |
| Coprime recurring, 1024 parameter | 4.927 / 3.671 | 43 / 28 | 3,082 / 1,610 |
| Integer, 256 parameter | 1.106 / 0.165 | 22 / 3 | 600 / 112 |

The mixed-denominator row demonstrates less allocation traffic with essentially
unchanged timing. The broad corpus initially flagged 15 very short arithmetic
rows as 20–50% slower. A focused follow-up used seven alternating processes per
version and ten repeated rows per case (70 row medians per version). All 15
before/after speed ratios were 0.972–1.094; the dyadic positive control remained
2.69 times faster. Those initial slowdowns were not reproduced. This evidence
does not mean every arithmetic operation improves.

## Representative decimal functions

Three alternating processes per version, five timing samples per row, frozen
independent mpmath references, HALF_EVEN. Instrumented profile/memory probes are
excluded from these timings. Precision denotes significant decimal digits.

| Fixture | Precision | Before median ms | After median ms |
| --- | ---: | ---: | ---: |
| Ordinary logarithm | 1000 | 29.770 | 27.535 |
| Ordinary logarithm | 2000 | 79.119 | 80.558 |
| Ordinary exponential | 2000 | 149.246 | 148.729 |
| Ordinary sine | 2000 | 70.739 | 61.465 |
| Ordinary square root | 2000 | 0.665 | 0.659 |

The largest additional benefit is in repeated rational conversion and
comparison during root search. These representative functions show smaller or
negligible changes, including a slightly slower 2000-digit logarithm median.
The measurements do not support a general claim that all decimal math is faster.
Separate operation/phase profiles now identify multiplication and exponentiation
as the remaining major costs in the high-precision root fixture.

## Verification

- Full final MSVC Release CTest run: **83/83 passed**. Full instrumented GCC
  Release run: **116/116 passed**, including allocation/deadline suites, numeric,
  division and complex oracles, graph/HTTP regressions and benchmark checks.
- The Python `Fraction`/`Decimal` checker validated **11,604 independent
  references** on each compiler, with 1000 deterministic random operand pairs,
  both arithmetic aliases, six conversion rounding modes, corrupt-reference
  rejection and zero tracked live payload after teardown. Exact finite text is
  checked in full. Fixed cases cover powers of two/five around
  0/1/26/27/28/63/64/65/127/128/129.
- Decimal comparison gained **1776 exact independent ordering cases**, including
  signed digit-position references, binary/decimal boundaries, aligned values
  and extreme scales. The numeric oracle now has 16,800 cases with zero failures;
  its existing 16 bounded directed-rounding transcendental cases retain their
  original one-ulp policy. New comparisons require exact agreement.
- Allocation and deadline injection walks every actual failure point through
  finite/recurring conversion, large powers of five, dyadic and integer paths,
  mixed denominators including `1/10^64` and `-3/10^512`, and decimal comparison.
  Failure paths preserve inputs/output and release tracked temporaries.
- CI now runs `numerical_benchmark_bounded_precision`: the verified 1000-place
  solve under the 64 MiB cumulative allocation cap, with a 120-second timeout.
  This is a correctness/resource guard, not a speed threshold.

## Browser and server limits

This pass does not change browser code or server scheduling. Earlier evidence
at `033eb5b` remains historical: constant expression batches benefited from
batch-local reuse, while browser initial-frame measurements moved in both
directions. Its 55 selected Chromium/Firefox/WebKit checks were not rerun in
this arithmetic-only pass. The earlier complete report is retained in Git at
`033eb5b`; raw browser/graph recordings remain under
`build/performance-20261030/`.

The fixed-arrival server experiment still applies: occupying every worker with
expensive requests can cause fast requests to receive 503 responses. Queued
items are sockets; workers parse requests and evaluate expressions. Admission
based on expression cost therefore needs a distinct parsed-computation stage.
Faster arithmetic alone does not establish fairness or overload isolation, and
increasing worker count only moves the saturation point. See the
[benchmark tool index](../../../benchmarks/README.md) for the server diagnostic.

## Reproduction and retained evidence

```sh
cmake --preset benchmarks -DPython3_EXECUTABLE=/path/to/python3
cmake --build --preset benchmarks
python benchmarks/tools/check_bigrational_benchmark.py BINARY_DIR/bigrational_benchmark --random 1000 --record build/rational-recording
BINARY_DIR/numerical_benchmark --high-precision
BINARY_DIR/numerical_benchmark --high-precision --allocation-limit-mib 64
BINARY_DIR/numerical_benchmark --high-precision --profile
ctest --test-dir BINARY_DIR -R numerical_benchmark_bounded_precision --output-on-failure
```

Use `.exe` and configuration subdirectories where applicable. Keep `--profile`
separate from timing recordings; its CSV diagnostics go to stderr. Compare old
numeric sources using the same benchmark harness and build switches, identifying
numeric and driver revisions separately.

Raw recordings and binaries remain under ignored `build/performance-round2/`.
`metadata.json` records selected sources, flags, hashes and measurement scope;
`summary.json` aggregates selected timings. Root recordings are
`paired-final-{before,after}-*.csv`; direct conversion recordings are
`paired-rational-{before,after}-*.csv`; representative math recordings are
`paired-math-{before,after}-*.csv`. `followup-*` retains the short-row regression
investigation. `bounded-outcomes.json`, `bounded-before.err` and
`bounded-after.csv` retain resource-limit outcomes. `roots-profile-*` and
`roots-final-profile*` are separate diagnostic passes. Final native test logs
are `ctest-gcc-final.log` and `ctest-msvc-final.log`. These local timings are
observations, not portable performance promises or CI speed thresholds.
