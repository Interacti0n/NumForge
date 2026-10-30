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
