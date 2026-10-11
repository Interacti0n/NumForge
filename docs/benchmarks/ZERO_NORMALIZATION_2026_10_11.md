# Exact decimal-zero normalization, 11 October 2026

The [factorial stress diagnostics](FACTORIAL_STRESS_2026_10_11.md) identified
decimal coefficient normalization as roughly two thirds of successful large
factorial requests. Production normalization now uses exact inverse division
for magnitudes of at least 128 limbs, retaining scalar normalization for small
values, insufficient binary divisibility and constrained optional scratch.
Installed C APIs, canonical decimal representation, scale semantics and
calculator time/memory/output limits are unchanged.

## Algorithm and failure contract

`10^27 = 2^27 * 5^27`, with `5^27` fitting uint64. A low-bit mask first tests
divisibility by `2^27`. Because `2^64 = 1 (mod 5)`, a bounded limb sum then
rejects magnitudes without even one factor of five, including powers of two,
without requesting scratch. Other coefficients are tried in one reusable
buffer no larger than the original significant magnitude.

After a virtual 27-bit shift, an odd-divisor inverse modulo `2^64` fixes each
quotient limb from low to high. Six unsigned Newton steps compute that inverse.
For shifted word `w_i`, incoming carry `c_i` and odd divisor `d`,
`q_i = (w_i-c_i)*inverse mod 2^64`; the multiply-high and subtraction borrow
give `q_i*d+c_i = w_i + 2^64*c_(i+1)`. Starting carry is zero. Final carry zero
proves exact division; a rejected trial does not replace the coefficient.
Unsigned wraparound in the inverse/quotient arithmetic is intentional.

Successful trials swap coefficient and scratch buffers, preserving the sign,
and count 27 removed zeros. The last fewer than 27 zeros use the existing
scalar blocks of at most 19. No huge power of ten is constructed. Benchmark
adapters also retain the 19-zero inverse variant for comparison.

The optional scratch preflight consumes no budget and sets no failure. If its
one request would not fit, the scalar path remains available. An actual
allocation failure propagates normally. Deadline checks cover every 256 limbs
in both new and scalar scans; production dispatch also checks at entry/exit.
The private working coefficient may be partially reduced after cancellation,
but public decimal operations own a temporary and commit only after successful
normalization and checked scale adjustment. Scratch is freed on every exit.

The tradeoff is one additional magnitude-sized allocation for coefficients
with enough trailing zeros. At 230,000 factorial the complete request adds
470,600 cumulative bytes, bringing requests to 10,925,190 bytes, well within
64 MiB. No-zero cases and few-zero cases with fewer than 27 factors of two
keep their scalar allocation behavior. Surplus binary factors with a small
decimal valuation may still require one rejected trial buffer.

## Experiment

Windows x64, Ryzen 7 7435HS, MSVC 19.51 Release. Independent Python fixtures
were generated before timing. Three serial cold processes used
forward/reverse/forward order without concurrent builds/tests; parsing,
reference checks and input restoration were outside timing. The adapter copies
to a private destination and checks both full coefficient and removed count.
Memory tracking was a separate invocation. No affinity/frequency lock.

The initial experiment measured the shared kernels before production dispatch
and the final modulo-five fast rejection were added:

| Input coefficient | Scalar, ms | Inverse 19, ms | Inverse 27, ms |
| --- | ---: | ---: | ---: |
| `10000!` | 2.483 | 1.136 | 1.145 |
| `100000!` | 283.643 | 203.478 | 145.726 |
| `200000!` | 1,216.626 | 844.467 | 603.662 |
| `230000!` | 1,661.441 | 1,144.791 | 795.718 |
| 65,536-bit base, 1,000 trailing zeros | 0.506 | 0.278 | 0.228 |
| 65,536-bit base, 10,000 trailing zeros | 6.550 | 4.070 | 2.585 |

The 27-zero variant wins the large cases. The 128-limb entry is conservative;
the low-bit test immediately retains the original path for typical few-zero
inputs. This is not a claim of a universal compiler/CPU crossover.

## Complete calculator measurements

The saved baseline executable uses numeric code from `2aff198` (the same
production core as `b95f8f6`). Ten scientific places, half-even rounding and
all standard application budgets are active. Complete independent rounded
outputs were checked after timed work. Each series contains three serial
paired cold processes per input, alternating old/new ordering on the reverse
run. Separate profile invocations measure normalization rather than replacing
the whole-request timer.

Two initial series retained the following medians. Series one had substantial
host timing variation, so a second complete series was recorded; neither was
discarded or treated as a hardware-independent speed guarantee:

| n | Series one before/after, ms | Series two before/after, ms |
| ---: | ---: | ---: |
| 100,000 | 819.393 / 564.016 | 466.024 / 325.875 |
| 200,000 | 3,158.144 / 1,306.905 | 1,815.915 / 1,188.737 |
| 225,000 | 2,327.848 / 1,498.520 | 2,303.392 / 1,525.984 |
| 230,000 | 2,407.246 / 1,640.218 | 2,397.109 / 1,582.319 |

Separate series-two profile probes measured normalization at 1,208.860 →
570.795 ms for 200,000 and 1,618.771 → 782.086 ms for 230,000. Final short
formatting remained below one millisecond. These probes precede the final
allocation-free modulo-five rejection; final measurements are recorded below.

After that rejection was added and final regression/reference checks finished,
the full paired recording was repeated once more on the final executable:

| n | Final baseline median, ms | Final new median, ms |
| ---: | ---: | ---: |
| 100,000 | 469.442 | 320.811 |
| 200,000 | 1,816.601 | 1,183.276 |
| 225,000 | 2,314.654 | 1,506.742 |
| 230,000 | 2,404.057 | 1,579.904 |

Separate final profile probes recorded normalization at 1,202.990 → 572.044 ms
for 200,000 and 1,624.402 → 790.739 ms for 230,000. Final formatting remained
below one millisecond. The three full series and executable hashes are retained.

The factorial admission rule is unchanged. Inputs previously falling back to
scalar factorial, including 233,000, are not claimed to complete simply because
later decimal projection is faster. Tighter checked admission remains next work.

## Correctness and portability

- 3,360 full Python references on MSVC and GCC: zero, signs, destination aliases,
  limb/block boundaries, no/few/many zeros, surplus factors of two or five,
  carry-heavy/nondivisible trials and complete factorials through 230,000.
  Both the normalized coefficient and removed count are compared exactly.
- 3,040 smaller complete cases on Win32 and forced portable GCC division; the
  largest factorial/timing stress rows are excluded from these portable checks.
- A permanent `normalization_fault_checks` benchmark CTest exhausts allocations
  and deadline checkpoints for fresh/aliased destinations, checks exact success,
  and requires tracked live allocations to return to baseline on every failure.
- Public `bigdecimal_from_bigint` exhausts allocation/deadline failures with
  4,096 trailing zeros, including an input alias of the destination coefficient.
  It verifies input/destination preservation, constrained scratch fallback and
  true final scale overflow preserving the destination.
- Final builds passed on MSVC/GCC/Win32/portable division. MSVC's 58 ordinary
  CTest suites, 15 selected GCC suites, and ten Win32/portable suites passed.
  The ordinary app build without benchmark instrumentation also passed.
- A temporary loopback HTTP server matched independent rounded factorial
  outputs, committed 200,000 factorial into a fresh session, previewed `ans`
  and returned its compact history. Ordinary CLI process regressions passed;
  the existing user's running server/tunnel was not changed.

## Reproduce

```powershell
python benchmarks/generate_zero_normalization.py build/normalization-full.tsv
build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/normalization-full.tsv --validate
python benchmarks/generate_zero_normalization.py build/normalization-timing.tsv --timing
node benchmarks/run_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/normalization-recording --factorials build/normalization-timing.tsv
ctest --test-dir build/factorial-bench -C Release -R 'normalization_fault|allocation_failure' --output-on-failure
build/factorial-bench/Release/calculator_limit_benchmark.exe '230000!' 10 2 5000 --phases-profile
```

Build opt-in benchmarks/tests first and use an empty recording directory. Local
ignored `build/zero-normalization` retains complete fixtures, individual CSV/JSON
samples, executable/fixture hashes and all three calculator series. The reference
generator uses standard-library integer arithmetic; it is not needed by the
library or ordinary builds.
