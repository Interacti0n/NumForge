# Arithmetic optimization evidence

Selected October 2026 measurements, Windows x64, Ryzen 7 7435HS,
Release builds with benchmark instrumentation. These are historical paired
comparisons, not measurements of today's checkout. Each subsection identifies
its own protocol; timings from separate recordings must not be combined.
Division, Karatsuba, reuse and complex comparisons below are from 11 October;
the formatting follow-up identifies its own baseline revision.
Public precision, ownership and failure rules belong in the [C reference](../../reference/C_API.md).

## Limb division

MSVC Release, baseline `51efef6`; seven-sample direct API medians, complete
independent references checked outside timing, separate allocation probes.
Normalized radix-2^64 long division replaces bitwise shift/subtract division;
portable scalar division remains supported. Signed truncation and atomic outputs
are preserved.

| Fixture | Before | After |
| --- | ---: | ---: |
| 4096-bit dividend / 97 | 89.11 µs | 0.724 µs |
| Similar 4096-bit operands | 188.11 µs | 1.008 µs |
| 4096-bit / 2048-bit | 410.10 µs | 6.215 µs |
| Exact 6144-bit / 2048-bit | 730.53 µs | 10.193 µs |
| Consecutive Fibonacci GCD, about 1423 bits | 29.60 ms | 1.119 ms |

## Karatsuba

MSVC 19.51 / GCC 16.2, three serial processes in forward/reverse/forward order,
three calibrated samples per process. Table entries are medians of process
medians through public `bigint_mul`, with full Node BigInt verification outside
timing. No affinity/frequency lock. The selected entry uses at least 128 limbs
in the smaller magnitude and an operand ratio below 2:1, with 32-limb leaves.
These private thresholds are not universal crossover claims.

| Balanced limbs | MSVC before / after ms | GCC before / after ms |
| ---: | ---: | ---: |
| 64 | 0.0273 / 0.0266 | 0.0260 / 0.0233 |
| 128 | 0.1082 / 0.0587 | 0.0947 / 0.0578 |
| 256 | 0.5176 / 0.1966 | 0.3690 / 0.1403 |
| 1024 | 7.2747 / 1.8026 | 6.0725 / 1.3169 |
| 4096 | 117.9530 / 15.8936 | 99.1249 / 12.8885 |

At 4096 balanced limbs, requested bytes increase from 65,536 to 196,576.
Non-consuming scratch preflight can select classical multiplication when an
optional arena exceeds the caller's budget. Real allocation/deadline failures
do not retry; destinations remain unchanged. Factorial's later split workspace
and budget admission are documented [separately](FACTORIAL.md).

## Symmetric squares and powers

30 October 2026, GCC 16.2 Release (`-O3 -DNDEBUG`), allocation statistics and
failure hooks enabled in both versions. Numeric baseline `bfc785e`, optimized
`c68f675`; these precede the later rational shortcuts. Seven immediate
fresh-process pairs per selected square, alternating order, seven calibrated
5-ms samples per process; medians of process medians, full Python integer
references checked outside timing. No affinity/frequency lock or concurrent
own build/test/benchmark. Input is one negative random object used for both
operands and output.

| Input limbs | Before / after microseconds | Requested bytes, either version |
| ---: | ---: | ---: |
| 16 | 1.098 / 0.802 | 256 |
| 32 | 3.813 / 2.441 | 512 |
| 64 | 15.921 / 9.186 | 1,024 |
| 127 | 64.715 / 33.469 | 2,032 |

Each square makes one allocation, with no scratch arena. The selected kernel
accumulates the off-diagonal triangle, doubles once and adds the diagonal.
The 16–127-limb entry avoids setup cost for smaller operands; larger values
retain Karatsuba. Whole-corpus apparent slowdowns at unchanged 128/129-limb
dispatch were not reproduced in immediate per-case follow-ups (before/after
median ratios 1.02/0.95; the 15-limb control was 0.96).

Five immediate pairs per power fixture, seven calibrated samples per process,
complete exact references, record left-to-right exponentiation with scalar
set-bit products for single-limb bases:

| Power | Before / after microseconds | Before / after requested bytes |
| --- | ---: | ---: |
| `5^3324` | 36.720 / 19.161 | 2,896 / 2,040 |
| `10^4095` | 108.268 / 52.252 | 5,344 / 3,512 |
| `UINT64_MAX^65` | 10.678 / 7.453 | 1,560 / 2,040 |
| 4096-bit base, exponent 17 | 924.750 / 926.617 | 52,912 / 52,384 |

Gains depend on the workload. Accumulator growth can increase traffic and
warmed output capacity: the `UINT64_MAX^65` total tracked peak rises from 1584
to 2064 bytes. Fewer calls do not imply less traffic or success under every
smaller budget. Raw evidence is under ignored `build/performance-round3/`:
`final-metadata.json`, `final-summary.json`, `followup-summary.json` and their
selected CSVs. The earlier complete report remains in Git at `a424589`.

## Full-width limb multiplication

30 October 2026 follow-up, baseline checkout `8b71856` (including the canonical
rational changes in `6d21fcb`), optimized `04ac1e1`. The private limb product is
now inline in its callers. GCC/Clang targets advertising a 128-bit integer use that type; MSVC x64
uses `_umul128`. Other targets retain the previous four-product, 32-bit
decomposition. This removes repeated decomposition and external calls inside
classical, symmetric and Karatsuba multiplication, scalar multiplication and
division estimation. Dispatch thresholds, precision and allocation policy stay
unchanged. The measurements below combine native multiplication and inlining;
they do not isolate the contribution of either change.

Windows x64, Ryzen 7 7435HS, GCC 16.2 `-O3 -DNDEBUG` and MSVC 19.51
`/O2 /Ob2 /DNDEBUG`, Release with allocation statistics and failure hooks on
both sides. Seven immediate fresh-process pairs per direct fixture and per
complete root/matrix corpus, order reversed on even pairs. Direct entries are
medians of process medians, each using seven calibrated 5-ms samples; complete
Python integer references are checked outside timing. Profiles and live-memory
probes are separate. No own concurrent build/test/benchmark, no affinity or
frequency lock; unrelated host activity remains uncontrolled.

| Operation | GCC before / after µs | MSVC before / after µs |
| --- | ---: | ---: |
| Distinct 16-limb multiplication | 1.023 / 0.457 | 1.118 / 0.437 |
| Distinct 64-limb multiplication | 16.869 / 5.779 | 15.720 / 5.460 |
| Distinct 128-limb multiplication | 29.851 / 12.678 | 34.359 / 16.109 |
| Distinct 256-limb multiplication | 89.741 / 36.626 | 105.933 / 49.463 |
| Same-object 64-limb square, separate output | 8.367 / 3.878 | 8.873 / 3.570 |
| `5^3324` | 11.532 / 5.686 | 12.618 / 5.277 |
| Unequal 104-by-242-limb multiplication | 88.839 / 31.418 | 93.235 / 31.981 |

The complete public callback search for `sqrt(2)` uses exact rational
intermediates and retains the independent Newton/residual checks from the
[rational baseline](RATIONAL.md#complete-public-root-search). Medians across
seven processes:

| Absolute places | GCC before / after ms | MSVC before / after ms |
| ---: | ---: | ---: |
| 12 | 0.2223 / 0.1904 | 0.1641 / 0.1579 |
| 100 | 2.3764 / 1.6881 | 1.8572 / 1.7313 |
| 1000 | 160.5453 / 89.1109 | 172.2898 / 86.1703 |

The 1000-place search is 1.80× faster with GCC and 2.00× with MSVC in these
recordings. Its process ranges are 158.36–172.39 / 79.18–91.84 ms and
159.17–201.76 / 80.64–104.03 ms respectively. Smaller search ranges overlap;
their median ratios alone do not establish a reliable speedup. Every version
uses 3324 iterations, 3327 evaluations, 225,161 allocation calls and 54,918,875
cumulative requested bytes at 1000 places. Separate 64-MiB-budget runs succeed,
including independent verification. Cumulative bytes are not resident memory.

The matrix corpus remains a useful control: ordinary 16×16 fractional
multiplication is 2.678 / 2.750 ms with GCC and 2.762 / 2.677 ms with MSVC,
with overlapping ranges. This change does not establish a further general
small-matrix speedup. All root, direct and matrix allocation counts/bytes match
their baseline; direct tracked live/peak payload also matches. These are local
arithmetic measurements, not browser, concurrent server or deployment capacity.

Verification on the final code passes all 118 GCC benchmark-build tests and
all 85 ordinary MSVC tests. Each compiler also verifies 1,399 full Node BigInt
references, 1,252 independent power/square references and 23,614 Python
Fraction/Decimal cases, including aliases, corrupt-reference rejection and
tracked-payload teardown. The native and forced-portable primitive tests each
check 45,380 boundary/random products against independent base-256 convolution.
A separate GCC build forces portable multiplication throughout the library;
six relevant core/failure suites and all three complete reference corpora pass
there too. Local verification covers Windows x64, without new Linux/ARM or
browser performance claims.

Raw recordings are under ignored `build/performance-limb/`: `metadata.json`,
`summary.json`, compiler-prefixed CSVs, separate profile/budget probes and
verification logs. Metadata preserves binary/source/fixture hashes and exact
compiler settings. The previous rational and square tables describe their own
earlier revisions and must not be compared across runs to infer another gain.

## Multiply-add carry chains

31 October 2026, baseline `58d5643`, optimized `82d4d37`, following the
full-width limb change above.
Classical products, Karatsuba leaves and the off-diagonal square triangle now
share an exact `a*b + addend + carry` primitive. GCC/Clang use unsigned 128-bit
arithmetic; MSVC x64 uses multiplication and two explicit add/carry chains.
The portable C path retains the original decomposition and overflow arithmetic.
Even four `UINT64_MAX` inputs sum to at most `2^128-1`; no high bit is discarded.
There is no new allocation, CPU-feature requirement or installed API. Dispatch
thresholds, scratch/admission plans, precision and cancellation checkpoints are
unchanged.

Windows x64, Ryzen 7 7435HS; GCC 16.2 `-O3 -DNDEBUG` and MSVC 19.51
`/O2 /Ob2 /DNDEBUG`, allocation statistics/failure hooks enabled in both versions.
Seven immediate fresh-process pairs per direct fixture and per complete
factorial/root corpus; reverse order on even pairs. Direct entries below are
medians of seven process medians, each using seven calibrated 5-ms samples.
Independent complete references are checked outside timing; live-memory probes
are separate. No own concurrent build/test/other benchmark, affinity or
frequency lock. Unrelated host activity and frequency changes are uncontrolled.

| Operation/input limbs | GCC before / after µs | MSVC before / after µs |
| --- | ---: | ---: |
| Distinct 32-limb product | 1.562 / 1.021 | 2.703 / 2.180 |
| Distinct 64-limb product | 5.772 / 3.475 | 9.563 / 8.645 |
| Distinct 128-limb product | 12.929 / 12.913 | 32.263 / 25.945 |
| Distinct 256-limb product | 42.265 / 38.952 | 96.552 / 82.126 |
| Distinct 1024-limb product | 755.986 / 692.837 | 915.517 / 841.450 |
| Distinct 4096-limb product | 6127.900 / 6693.100 | 9081.600 / 6971.400 |
| Unequal 104-by-242-limb product | 63.005 / 40.399 | 38.997 / 30.083 |
| Same-object 64-limb square, separate output | 6.733 / 5.671 | 6.439 / 6.022 |
| `5^3324` | 10.295 / 8.011 | 5.417 / 5.436 |

The 32/64-limb GCC product ranges do not overlap (1.449–1.771 /
0.899–1.186 µs and 5.581–6.151 / 3.267–3.866 µs). MSVC 32/128-limb ranges
also separate. Other rows are noisier: GCC 128/4096-limb paired median speedups
are 0.962/0.984; MSVC square and power controls are approximately 1.005/1.010.
Retain these apparent slowdowns/flat results rather than inferring a universal
improvement from selected medians.

### Complete calculations

The public callback search for `sqrt(2)` keeps exact rational intermediates and
independent Newton/residual validation. A second seven-pair root corpus follows
the first recording; no binaries/settings changed between batches.

| Compiler | First batch before / after ms, 1000 places | Follow-up before / after ms | Median paired speedup, first / follow-up |
| --- | ---: | ---: | ---: |
| GCC | 95.358 / 78.232 | 125.116 / 90.280 | 1.219 / 1.269 |
| MSVC | 164.083 / 151.769 | 103.467 / 99.075 | 1.113 / 1.026 |

These are process medians, while the last column is the median of each
immediate before/after pair's ratio. They are different statistics. Host drift
is visible even in the unchanged baseline; do not compare absolute times
across batches. The GCC paired ratios correspond to about 18–21% shorter time.
MSVC's smaller, variable root effect and overlapping ranges do not establish
a stable magnitude. Low-precision root controls also overlap and can look
slower; every recording remains in the raw data.

All 1000-place searches use 3324 iterations, 3327 evaluations, 225,161
allocation calls and 54,918,875 cumulative requested bytes. Separate runs
within the 64-MiB allocation budget pass, including independent validation.
Direct allocation counts/bytes and tracked live/peak payload are identical.
Factorial parse/evaluate/format allocation counts/bytes also match.

The existing calculator factorial diagnostic validates complete scientific
output. Its evaluation medians are 117.769 / 115.677 ms (GCC) and
235.657 / 226.432 ms (MSVC) for `100000!`; `50000!` is 33.822 / 31.735 and
65.037 / 62.452 ms. Ranges overlap, and paired median speedups are only
1.014/1.026 for `100000!`. No substantial factorial speedup is claimed.

### Validation and rejected experiments

All 118 GCC benchmark-build tests and 85 ordinary MSVC tests pass. Both
compilers additionally verify 1,399 full Node BigInt references, 1,252 independent
power/square references and 23,614 Python Fraction/Decimal cases. A complete
forced-portable multiplication build passes the same reference corpora and
eight selected core/failure suites. The native/portable primitive targets each
check 45,380 plain products plus 36,864 accumulated products, using independent
base-256 convolution; accumulated cases include the full 8-by-8-by-8-by-8
boundary grid and 32,768 deterministic random quadruples.

Linux Release builds successfully. Linux AddressSanitizer/UndefinedBehaviorSanitizer
checks with leak detection pass 133 BigInt tests, seven property tests and both
three-test primitive variants. The final diagnostic uses explicit non-PIE
executables. An initial PIE sanitizer invocation hung with repeated
`DEADLYSIGNAL` messages and no source report; isolated plain, UBSan, ASan PIE and
ASan non-PIE repetitions passed. The initial failure/retry records are retained;
the final clean run does not diagnose that initial hang. No browser/WAN or
deployment performance claim is added by this arithmetic-only change.

Threshold exploration did not justify changing the existing dispatch. A
separate trial grouping inner-loop budget checks preserved checkpoint positions
but gave small/variable complete-calculation results; it was not retained.
Those preliminary recordings are not the selected carry-chain comparison.

Raw ignored directory: `build/performance-crossover/`. Selected files are
`madd-metadata.json`, `madd-data.json`, `madd-summary.json`, `madd-*.csv` and
`followup-summary.json`/`followup-*.csv`; separate bounded, compiler, native,
reference and Linux diagnostic logs accompany them. Metadata preserves binary
and source hashes. `exploration-*` and `candidate-*` belong to the rejected
threshold/chunk investigations, not the final change.

## Two-word zero normalization

31 October 2026, baseline `24e4d24`. Large decimal coefficients already remove
zeros through exact inverse division in 27-zero blocks. Each trial now computes
two quotient limbs using the inverse modulo `2^128`, then recovers the outgoing
carry with the original scalar identity. This shortens the serial multiplication
dependency. All operations use the existing unsigned full-width product helper;
the portable implementation also works without a 128-bit integer type.
The single scratch buffer, allocation preflight/fallback, 128-limb dispatch,
scalar tail, signs, precision and every-256-limb cancellation checks are retained.

A separate single-run phase profile attributes 540.7 of 1015.0 ms (GCC) and
580.5 of 1156.5 ms (MSVC) to normalization of `246000!`. These profiled runs
identify work; they are not the unprofiled timing baseline below.

Windows x64/Ryzen 7 7435HS, GCC 16.2 Release `-O3 -DNDEBUG` and MSVC 19.51
Release `/O2 /Ob2 /DNDEBUG`, allocation counters/failure hooks enabled in both
versions. Seven immediate fresh-process pairs per case, order reversed on even
pairs; the follow-up repeats `246000!` and adds controls/direct probes. No own
concurrent build, test or other benchmark. Host activity/frequency remain
uncontrolled; no affinity/frequency lock. Complete scientific output at ten
places is checked independently outside timing, with a matching 30-second
diagnostic deadline and the unchanged application allocation limits.

| Complete calculator request | GCC before / after ms | MSVC before / after ms |
| --- | ---: | ---: |
| `100000!` | 180.048 / 166.777 | 209.894 / 190.730 |
| `246000!`, first batch | 986.401 / 899.054 | 1129.069 / 987.452 |
| `246000!`, follow-up | 993.749 / 897.397 | 1173.471 / 984.282 |
| `10000!`, control | 2.663 / 2.718 | 3.074 / 2.964 |

Entries are medians of seven process durations, not the median paired ratio.
For `246000!`, median immediate-pair speedups are 1.095/1.098 (GCC) and
1.157/1.179 (MSVC), corresponding to about 9% and 14–15% shorter paired times.
The ratios of the displayed medians give about 9–10% and 13–16% shorter time.
`100000!` requests retain 9195 allocations/4,602,403 requested bytes;
`246000!` retains 17,310/13,581,463. These are cumulative allocation traffic,
not resident memory.

The direct `strip27` adapter over `100000!`, including its owned input copy,
changes from 82.716 to 67.322 ms (GCC) and 84.187 to 65.929 ms (MSVC): roughly
19%/22% shorter time. Each process uses seven calibrated samples and a separate
live-payload probe. Both versions request two allocations/451,736 bytes and
retain the same 451,744-byte baseline/final payload and 903,480-byte peak.
A 65,536-bit base with 1000 decimal zeros changes 188.193/156.481 microseconds
(GCC) and 178.625/143.886 (MSVC). Controls remain mixed: with zero zeros the
same adapter records 4.015/4.120 and 5.239/5.618 microseconds; with 27 zeros,
14.907/15.098 and 15.335/14.551. The 1000-place root control records
115.954/115.010 ms and 136.221/128.337 ms, with overlapping ranges. These
controls do not establish a general arithmetic speedup.

Validation uses an isolated `24e4d24` source snapshot plus only this change and
its tests, avoiding concurrent account/frontend work. All 119 GCC benchmark
tests pass. All 86 MSVC tests pass after creating the snapshot's missing `build/`
parent and rerunning the remote-helper test; the initial environment failure is
preserved. Each native compiler and the forced-portable library verifies 2520
complete Python-integer coefficient/count references, including signs and
aliases, and exhaustive allocation/deadline interruption of the factorial
normalization adapter. The new CI unit suite checks 5780 independent factor
valuation cases across 19/27-zero blocks, odd/even lengths and the 256-limb
checkpoint. Each native compiler additionally passes 23,614 Fraction/Decimal
references. Linux GCC 12.2 ASan/UBSan/leak checks pass that new suite, 26 decimal
tests and seven rational tests for both native and forced-portable libraries,
using non-PIE executables. The initial Linux diagnostic omitted required test
hooks; the corrected complete run is retained. Linux/portable performance was
not measured.

Raw ignored directory: `build/performance-normalization/`, including
`pair-data.json`, `followup-data.json`, `summary.json`, `metadata.json`, exact
fixtures, compiler/test/oracle logs and Linux diagnostics. An exploratory
deferred-shift normalization variant was not retained; its results are not the
two-word comparison. An intentionally stopped validation including the unchanged
slow classic adapter is separate from the complete 2520-case candidate checks.

## Allocation reuse

MSVC 19.51, medians of three fresh serial processes. GCD baseline `1b19e2c`,
decimal workspace baseline `19ed8f0`; complete calculator requests use 2000 output
places and the existing 5-second/64-MiB cumulative budget. GCD uses a direct
preconstructed `gcd(2^100000-1,3^63000)` fixture with `--validate`.

| Work | Before / after ms | Before / after requested bytes |
| --- | ---: | ---: |
| Direct GCD | 800.101 / 710.525 | 1,266,618,336 / 81,928 |
| Calculator ln(2) | 362.074 / 334.780 | 23,195,765 / 3,927,519 |
| Calculator exp(1) | 200.090 / 168.416 | 15,800,512 / 4,766,371 |
| Calculator ln(1+i) | 573.298 / 547.181 | 37,972,543 / 18,572,659 |

GCD allocation calls fall from 233,096 to six. Its tracked peak payload changes
from 123,296 to 114,696 bytes: requested traffic is not resident memory. Series
workspaces are per-call; no global numeric cache is introduced.

## Shared complex scalar work

MSVC Release, baseline `a7784ea`, three alternating fresh-process runs: sharing
sin/cos evaluation changes all six ordinary complex forward-function calculator
probes at 1+i and 2000 places from allocation failure to success. Failure durations
are not completed-operation timings. A later sinh/cosh sharing comparison against
`603450c`, three serial fresh processes, records:

| Expression | Before / after ms | Before / after requested bytes |
| --- | ---: | ---: |
| sin(1+i) | 798 / 598 | 58,936,247 / 43,081,818 |
| cos(1+i) | 783 / 594 | 58,938,361 / 43,083,932 |
| sinh(1+i) | 787 / 588 | 58,936,247 / 43,081,818 |
| cosh(1+i) | 790 / 594 | 58,936,245 / 43,081,816 |

For complex sine, a separate 2000-significant-digit C memory probe retains a
30,044-byte peak while traffic falls. Scaled tan/tanh use different formulas and
do not inherit this allocation reduction. Shared helpers retain transactional
pair outputs, alias support, guard precision and small-argument sinh series.

## Bounded-prefix formatting



Short scientific output now forms positive lower/upper bounds from the leading
binary limbs and omitted-bit interval. Powers of two and products use outward
FLOOR/CEILING rounding at a bounded working precision. Only when both endpoints
produce the same requested result and agree on the scientific exponent is that
text accepted. Ambiguous sticky/tie/exponent boundaries use the full conversion.
No floating-point digit estimate decides a result; full output retains its path.

The fast path currently accepts 0–128 mantissa places and a conservative size
threshold of `64 + places*places/32` coefficient limbs. The threshold avoids a
measured regression for 100-place output on 4,096-digit inputs. It is a local
selection policy, not a universal crossover claim.

Three matching before/after runs on the same Windows x64/Ryzen 7 7435HS/MSVC
19.51 Release setup are preserved in `build/format-prefix-comparison/`, including
CSV, compiler diagnostics, source patch and machine metadata. The before
executable contains the unchanged formatter from `fd248fc`; after is the recorded
development snapshot. No CPU affinity/frequency locking was used.

| Case | Before median range | After median range |
| --- | ---: | ---: |
| Auto, 10 places, 4,096 digits | 170–176 µs | 105–135 µs |
| Auto, 10 places, 16,384 digits | 2.894–3.066 ms | 0.124–0.193 ms |
| Scientific, 100 places, 4,096 digits (fallback) | 171–183 µs | 172–176 µs |
| Scientific, 100 places, 16,384 digits | 2.884–2.920 ms | 0.602–0.645 ms |
| Full scientific, 16,384 digits | 2.841–2.914 ms | 2.847–2.878 ms |

The 16,384-digit Auto result remains 19 bytes. Peak tracked payload in the
operation probe drops from 32,784 to 632 bytes; cumulative requested bytes drop
from 47,957 to 17,382, while allocation calls increase from 7 to 702. The method
trades many small temporary allocations for less conversion work and lower
peak payload; these figures are not process RSS or a general allocator claim.

Independent Node exact-integer rounding checks cover both signs, all six modes,
0/10/100/128 places, large exponents, carry/sticky tails and the 129-place fallback.
Every allocation in an exercised prefix path is failed in turn to verify output
preservation. GCC and MSVC correctness checks pass; comparable performance on
other compilers/platforms remains future measurement work.

## Reproduction and limits

Use the [BigInt](../protocols/BIGINT.md), [decimal](../protocols/DECIMAL.md) and
[complex](../protocols/COMPLEX.md) protocols with matching historical binaries
for a before/after comparison. `run_karatsuba.cjs --focused` records multiplication;
`check_karatsuba.cjs --full` verifies products. Keep generated fixtures and raw
recordings under ignored `build/`. No timing threshold is imposed in CI.

Exact digit counting, coefficient conversion and zero normalization are retained
implementation decisions; their intermediate tuning diaries are in Git history.
Current failure/oracle tests remain the regression authority. These measurements
do not imply equivalent speedups for complete expressions or arbitrary inputs.
