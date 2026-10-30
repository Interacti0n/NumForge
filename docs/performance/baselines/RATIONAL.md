# Rational roots, powers and symmetric squares — 30 October 2026

Windows x64, Ryzen 7 7435HS (16 logical CPUs), approximately 32 GiB RAM,
GCC 16.2 Release (`-O3 -DNDEBUG`). Numeric baseline `bfc785e`, checkout
`1387815`; optimized implementation `c68f675`. This comparison measures
additional gains over the previous rational/conversion optimizations. Their
complete report remains in Git at `1387815`; their raw evidence remains under
`build/performance-round2/` and `build/performance-20261030/`.

Both builds enable allocation statistics and test failure hooks, without
sanitizers or coverage. Live-payload and operation-profile probes run separately
from speed measurements. The retained before rational, numerical and decimal
math binaries have the exact hashes recorded for the previous pass's after
binaries. There is no affinity or frequency lock; unrelated host activity is
uncontrolled. No build, test or other benchmark from this review ran alongside
the selected timing recordings. Timings are local observations, not portable
promises or CI speed thresholds.

## Changes and safety boundaries

- Binary exponentiation now consumes the leading one by copying the base, then
  scans remaining bits from most to least significant. Set bits multiply by the
  original base rather than a growing power of it. A single-limb base uses the
  private scalar multiplier on the owned accumulator.
- Multiplication of the same input object at **16–127 limbs** uses a symmetric
  square: accumulate off-diagonal products once, double the triangle once, then
  add diagonal products. It requires `n*(n+1)/2` full limb products instead of
  `n*n`, using only the ordinary result buffer. Distinct input objects retain
  general multiplication even if their values compare equal.
- Smaller squares keep the classical kernel because setup cost offsets the
  saving. Larger squares retain the existing budget-aware Karatsuba entry.
  Trial measurements with a four-limb entry did not justify that smaller cutoff.
- Outputs commit only after success. Both output/input aliases, the single
  object used for base/exponent/output, signs, zero and multi-limb exponent
  parity retain their contracts. Allocation/deadline failures do not retry or
  publish partial accumulators.

No global cache, public API, precision, rounding, root tolerances, iteration
policy, worker count or application resource default changed.

## Complete public root search

Seven immediate before/after fresh-process pairs, reversing order on even
pairs, using `numerical_benchmark --high-precision`. This is the exact-rational
callback for `x*x-2` on `[0,2]`, not the direct decimal `sqrt` operation.
Working precision is absolute places plus 20; width/residual tolerances are
`1E-places`. Independent Newton sqrt and residual validation run outside
timing. Iterations/evaluations remain 42/45, 335/338 and 3324/3327.

| Absolute places | Before median ms | After median ms | Before / after allocation calls | Before / after requested bytes |
| ---: | ---: | ---: | ---: | ---: |
| 12 | 0.3599 | 0.3210 | 2,693 / 2,581 | 34,955 / 33,139 |
| 100 | 4.2477 | 4.1444 | 26,079 / 24,444 | 853,139 / 804,627 |
| 1000 | 409.3764 | 345.3135 | 280,508 / 258,430 | 64,827,091 / 61,883,283 |

The 1000-place median takes **15.6% less time** (1.19 times faster), with 7.9%
fewer allocation calls and 4.5% less requested traffic. Before runs span
392.8–463.9 ms; after runs span 307.6–373.8 ms. All seven pairs improve.
The 12/100-place ranges overlap and do not establish reliable speed gains.

Host speed differed from the previous pass, whose after median was about
202 ms. That old recording must not be compared directly with this pass's
345 ms: the matched before binary now measures about 409 ms. Use the paired
comparison above, not absolute timings from different sessions.

Requested bytes are cumulative traffic, not peak/live memory or RSS. Separate
64 MiB cumulative-budget runs succeed on both before and after versions,
including independent validation inside the same budget. The after solve alone
requests 59.0 MiB; validation is outside that counter snapshot but inside the
budget. The harness retains a 120-second deadline and 8 MiB single-allocation
cap. This fixture does not establish success for arbitrary callbacks.

## Symmetric square

Seven immediate pairs per selected case, alternating order, seven calibrated
5-ms samples per process. Table entries are medians of process medians.
Every complete result is checked against an independent Python integer
reference outside timing. Inputs are deterministic random negative values,
with one object used for both operands and output (`alias_all`).

| Input limbs | Before microseconds | After microseconds | Before / after allocation calls | Requested bytes, either version |
| ---: | ---: | ---: | ---: | ---: |
| 16 | 1.098 | 0.802 | 1 / 1 | 256 |
| 32 | 3.813 | 2.441 | 1 / 1 | 512 |
| 64 | 15.921 | 9.186 | 1 / 1 | 1,024 |
| 127 | 64.715 | 33.469 | 1 / 1 | 2,032 |

The selected square medians improve by about 1.4–1.9 times without extra
workspace. Initial whole-corpus recordings also suggested large slowdowns at
128/129 limbs, where dispatch is unchanged. Immediate per-case follow-ups
reduced those before/after median ratios to 1.02/0.95; a 15-limb control was
0.96. These controls and the initial recordings are retained. They show why
the broad corpus alone is unsuitable for small timing claims and do not
establish that every unchanged path has identical latency.

## Powers and conversion

Five immediate pairs per power fixture, reversing order on even pairs, seven
calibrated samples per process. Complete references and aliases are checked
outside timing. Base sizes identify generator parameters.

| Power | Before microseconds | After microseconds | Before / after allocation calls | Before / after requested bytes |
| --- | ---: | ---: | ---: | ---: |
| `5^1023` | 6.116 | 3.934 | 21 / 10 | 1,120 / 696 |
| `5^3324` | 36.720 | 19.161 | 21 / 12 | 2,896 / 2,040 |
| `10^4095` | 108.268 | 52.252 | 25 / 12 | 5,344 / 3,512 |
| `UINT64_MAX^65` | 10.678 | 7.453 | 10 / 8 | 1,560 / 2,040 |
| 256-bit base, exponent 31 | 32.278 | 17.125 | 11 / 9 | 2,832 / 3,488 |
| 4096-bit base, exponent 17 | 924.750 | 926.617 | 11 / 9 | 52,912 / 52,384 |

The improvements are workload-specific. Accumulator growth and different
intermediate sizes can increase cumulative requested bytes. For
`UINT64_MAX^65`, the warmed destination also retains more capacity: total
tracked peak payload rises from 1584 to 2064 bytes. For the 256-bit-base row,
peak payload changes from 3016 to 2984 bytes despite higher traffic. Fewer
calls or lower peak payload do not imply less cumulative traffic or success
under every smaller budget.

Five paired conversion runs show dyadic 1024-parameter HALF_EVEN conversion
changing from 5.926 to 5.559 microseconds, with 18 to 15 calls and 1792 to 1456
requested bytes. This modest timing change has overlapping ranges. Mixed
terminating, recurring and integer conversion rows provide no convincing
additional speedup. They retain exact finite digits and six rounding modes.

## Representative decimal functions

Three immediate pairs per case, five timing samples per row, frozen independent
mpmath references, HALF_EVEN. Profile/memory probes are excluded. Precision
denotes significant decimal digits.

| Fixture | Precision | Before median ms | After median ms |
| --- | ---: | ---: | ---: |
| Ordinary logarithm | 1000 | 29.623 | 32.570 |
| Ordinary logarithm | 2000 | 83.101 | 81.962 |
| Ordinary exponential | 2000 | 158.235 | 156.811 |
| Ordinary sine | 2000 | 65.279 | 65.375 |
| Ordinary square root | 2000 | 0.677 | 0.688 |

The 1000-digit logarithm initially appears slower. Seven further immediate pairs
give overlapping ranges: 31.55–36.67 ms before and 31.25–35.04 ms after. Four of
seven pairs improve; the median paired speed ratio is 1.01, spanning 0.92–1.08.
A repeatable slowdown was not established. These results support no general
additional decimal-function speedup. Separate profiles identify multiplication
and exponentiation as remaining major costs in the root fixture.

## Verification and reproduction

- Full native suites passed: **116/116 GCC**, **83/83 MSVC**, with a separate
  successful rerun of the final instrumented allocation-failure suite.
- **1252 independent Python integer references** validate powers, negative
  bases, carry patterns, identical/distinct operands, aliases and limb/exponent
  boundaries on both GCC and MSVC. The benchmark's separate live-ledger pass
  requires complete teardown. Native radix-identity tests independently check
  `(B^n-1)^2 = B^(2n)-2*B^n+1` around both dispatch boundaries.
- **11,604 Fraction/Decimal references** pass on both compilers, including
  arithmetic aliases, six rounding modes, corrupt-reference rejection and zero
  tracked live payload after teardown. The existing numeric/division/complex
  oracles also pass; numeric oracle coverage remains 16,800 cases.
- New allocation/deadline injection walks every actual failure point in small-
  and multi-limb-base powers and the 16/127-limb square paths. It checks aliases,
  unchanged inputs/outputs, resource failure reasons and, in instrumented
  builds, unchanged live payload on failure.
- The existing CI benchmark smoke now includes random/maximal/sparse/alternating
  squares at 15/16/127/128 limbs and small-base exponent boundaries 63/64/65.
  The 1000-place 64 MiB resource guard remains enabled; no timing threshold was
  added.

```sh
cmake --preset benchmarks -DPython3_EXECUTABLE=/path/to/python3
cmake --build --preset benchmarks
node benchmarks/tools/bigint_cases.js build/bigint-cases.tsv --quick
BINARY_DIR/bigint_arithmetic_benchmark build/bigint-cases.tsv --validate
python benchmarks/tools/check_bigrational_benchmark.py BINARY_DIR/bigrational_benchmark --random 1000
BINARY_DIR/numerical_benchmark --high-precision
BINARY_DIR/numerical_benchmark --high-precision --allocation-limit-mib 64
BINARY_DIR/numerical_benchmark --high-precision --profile
ctest --test-dir BINARY_DIR --output-on-failure
```

Use `.exe` and configuration subdirectories where applicable. Omit `--quick`
from fixture generation for the full integer corpus. Keep profile and live
tracking passes separate from speed recordings.

Raw evidence remains under ignored `build/performance-round3/`:
`final-metadata.json` records source/binary/fixture hashes and settings;
`final-summary.json` aggregates selected root, power, conversion and math
recordings. `followup-summary.json` and `followup-*.csv` contain the immediate
square/logarithm follow-ups used above. `final-profile-*` and `final-bounded-*`
are separate diagnostics. `powers.tsv`, `final-validation-*.csv` and
`validate-msvc.csv` retain exact integer checks; `check-rational-*.log` and
`ctest-*-final*.log` retain verification outcomes. Exploratory `square-trial-*`
and the earlier power-only runs are not the selected speed comparison.

## Browser and server limits

This arithmetic-only pass does not rerun browser tests or change server
scheduling. Earlier browser evidence is retained in Git at `033eb5b` and
`1387815`. All-worker saturation can still cause fast requests to receive 503
responses; arithmetic improvements alone do not establish admission fairness
or overload isolation. The [benchmark tool index](../../../benchmarks/README.md)
documents the separate server diagnostic.
