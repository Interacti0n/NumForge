# Canonical rational arithmetic — 30 October 2026

Windows x64, Ryzen 7 7435HS (16 logical CPUs), approximately 32 GiB RAM,
GCC 16.2 Release (`-O3 -DNDEBUG`). Numeric baseline `c68f675`, checkout
`a424589`; optimized implementation `6d21fcb`. Both builds enable allocation
statistics and test failure hooks, without sanitizers or coverage. No own
build/test/other benchmark ran alongside the selected speed recordings. There
is no affinity or frequency lock; unrelated host activity is uncontrolled.
These are local observations, not portable promises or CI speed thresholds.

The before numeric archive was saved before changing the core; rebuilding only
the new square/selfdiv benchmark selectors left its hash unchanged. The before
root binary is byte-identical to the previous pass's verified after binary.
Metadata records source, fixture, archive and executable hashes. Earlier power
and square decisions now belong to [arithmetic evidence](ARITHMETIC.md#symmetric-squares-and-powers);
the previous complete report remains in Git at `a424589`.

## Changes and exactness

All public rational inputs are canonical: reduced numerator/denominator with a
positive denominator. This permits exact algebraic shortcuts:

- For `x/d * y/d`, both numerators are coprime to `d`, so the product is already
  reduced. Multiply the original numerators and square the denominator directly,
  avoiding two cross-GCDs and four divided copies. A same-object square also
  exposes identical integer operands to the existing symmetric-square kernel.
- For `(x/d)/(y/d)`, the common denominator cancels; reduce only `x/y`.
  A nonzero value divided by itself is exactly one. Zero-divisor rejection stays
  before the shortcut, including `0/0` and output/input aliases.
- Adding/subtracting an integer preserves coprimality:
  `gcd(x +/- k*d,d) = gcd(x,d) = 1`. Build that numerator and copy the denominator
  directly, including integer translation in either operand order. Two integer
  operands commit their already-owned sum/difference without a redundant copy.

Owned results remain detached until both parts succeed and the final budget
check passes. Aliases, signs, canonical zero, rounding, precision, tolerances,
iteration policy and application resource defaults are unchanged. No global
scratch, shared cache or public API was added. The same-input selectors are
benchmark adapters over the existing multiplication/division APIs.

## Direct arithmetic

Seven immediate fresh-process pairs per fixture, reversing order on even pairs;
five calibrated 2-ms samples per process. Table entries are medians of process
medians. Fixture construction, full canonical text/reference checks and aliases
are outside timing. Parameters identify generator shapes rather than every
component's exact bit length.

| Fixture | Before / after microseconds | Before / after allocation calls | Before / after requested bytes |
| --- | ---: | ---: | ---: |
| Integer multiplication, 256 parameter | 0.584 / 0.190 | 8 / 2 | 168 / 64 |
| Equal-denominator multiplication, 1024 parameter | 3.542 / 1.827 | 18 / 2 | 3,104 / 528 |
| Same-object square, 1024 parameter | 3.548 / 1.409 | 18 / 2 | 3,104 / 528 |
| Equal-denominator division, 1024 parameter | 1.377 / 0.811 | 15 / 8 | 2,488 / 1,800 |
| Same-object division, 1024 parameter | 0.930 / 0.161 | 12 / 2 | 1,216 / 16 |
| Fraction plus one, 1024 parameter | 0.609 / 0.289 | 7 / 3 | 816 / 520 |
| Fraction minus one, 1024 parameter | 0.699 / 0.390 | 8 / 4 | 944 / 648 |

These selected operations improve by about 1.7–5.8 times. This is not a uniform
arithmetic speedup: coprime general multiplication retained 18 calls/3624 bytes
and changed from 3.650 to 3.805 microseconds with overlapping ranges. General
division, cancellation and equal-denominator sums serve as retained controls.

At the 8192-bit boundary, multiplication initially measured 40.772/44.361
microseconds; seven focused follow-up pairs measured 40.404/38.638 microseconds,
with overlapping ranges and median paired speed ratio 1.025. No reliable timing
gain or slowdown is established there. Calls/traffic still fall from 10/17792
to 4/11632. Large same-object division improves from 1.259 to 0.159 microseconds
and from 10/4168 calls/bytes to 2/16. Preserve these distinctions rather than
extrapolating the small-operand speed ratios to every size.

## Complete public root search

Seven immediate before/after fresh-process pairs of
`numerical_benchmark --high-precision`, reversing order on even pairs. The exact
rational callback is `x*x-2` on `[0,2]`, with working precision equal to absolute
places plus 20 and width/residual tolerances `1E-places`. Independent Newton sqrt
and residual validation remain outside timing. Iterations/evaluations are
unchanged at 42/45, 335/338 and 3324/3327.

| Absolute places | Before median ms | After median ms | Before / after allocation calls | Before / after requested bytes |
| ---: | ---: | ---: | ---: | ---: |
| 12 | 0.2318 | 0.2498 | 2,581 / 2,132 | 33,139 / 28,635 |
| 100 | 2.3641 | 2.1685 | 24,444 / 21,065 | 804,627 / 714,315 |
| 1000 | 186.5920 | 176.2033 | 258,430 / 225,161 | 61,883,283 / 54,918,875 |

The 1000-place median is about 5.6% shorter, but timing ranges overlap
(174.91–197.43 ms before, 172.05–214.04 ms after). The earlier exploratory
product-only screen suggested a larger gain; it is not the selected final
comparison. This evidence does not establish a large whole-root speedup.

The resource saving is deterministic: 12.9% fewer allocation calls and 11.3%
less requested traffic at 1000 places. Requested bytes are cumulative, not
peak/live memory or RSS. Both versions pass separate 64 MiB cumulative-budget
runs, including independent validation inside the budget; the after solve
alone requests about 52.4 MiB. The 120-second harness deadline and 8 MiB
single-allocation cap remain unchanged. Separate profiles record GCD calls
falling from 33,232 to 23,253; their clocks are excluded from the speed table.

## Exact modules and graph observations

The larger end-to-end gain is in exact matrix multiplication: seven matched
pairs of the existing `(I+J)/11` fixture show about 2.1 times faster 8x8 and
1.7 times faster 16x16 multiplication, with every entry verified independently.
The [module baseline](MODULES.md#canonical-rational-follow-up)
owns those timings, allocation counts, matching-link method and limits. Other
matrix operations and polynomial solving show smaller or negligible changes.

The existing 14-case graph corpus checks the exact 129-point stage against fast
samples and independent libm references, with all original error checks active.
Three initial pairs and seven follow-up pairs show mixed timings. Wide sine
initially measured 7.413/8.958 ms; follow-up medians were 8.128/8.755 ms, with
overlapping ranges (7.43–10.38 / 7.40–12.54 ms). The cancellation fixture's
follow-up was 0.728/0.657 ms. These measurements establish no general graph or
browser-frame speedup; no browser timing was recorded in this arithmetic pass.

## Verification and reproduction

- Full final instrumented GCC CTest suite: **116/116 passed**. Full ordinary
  MSVC Release suite: **83/83 passed**, including existing independent numeric,
  division and complex oracles and graph/HTTP contracts.
- **23,614 independent Fraction/Decimal references** pass on both compilers,
  including both arithmetic destination aliases, true same-object squares and
  divisions, six conversion rounding modes, corrupt-reference rejection and
  zero tracked live payload after teardown. The generator retains 1000 original
  random pairs plus 750 structured same-denominator/integer-translation pairs,
  and exact boundaries around 16/128-limb square dispatch through 8193 bits.
- The native integer reference grid now checks 2916 small fraction pairs over
  denominators 1–6 and numerators -4–4. Allocation/deadline injection exhausts
  actual failure points through equal-denominator signs, integer translations,
  both ordinary aliases and 1024/8192-bit same-object products/divisions.
  Independent radix identities construct the expected large squares. Failures
  preserve inputs/outputs and, in instrumented builds, tracked live payload.

```sh
cmake --preset benchmarks -DPython3_EXECUTABLE=/path/to/python3
cmake --build --preset benchmarks
python benchmarks/tools/check_bigrational_benchmark.py BINARY_DIR/bigrational_benchmark --random 1000
BINARY_DIR/numerical_benchmark --high-precision
BINARY_DIR/numerical_benchmark --high-precision --allocation-limit-mib 64
BINARY_DIR/matrix_benchmark
BINARY_DIR/polynomial_benchmark
ctest --test-dir BINARY_DIR --output-on-failure
```

Use `.exe` and configuration subdirectories where applicable. Compare the same
harness against old numeric sources with matching compiler/instrumentation
settings; keep profile/live/resource probes separate from timing.

Raw evidence remains under ignored `build/performance-products/`.
`final-metadata.json` and `final-summary.json` identify selected root/direct/graph
recordings; `followup-summary.json` retains the large-multiply/graph follow-up.
`modules-selected-metadata.json`, `modules-selected-summary.json` and matching
CSVs own the module comparison. `screen-*` and the initial module link experiment
are exploratory, not selected results. Final test logs are
`ctest-gcc-final.log`, `ctest-msvc-final.log` and `check-{gcc,msvc}.log`.

This pass leaves HTTP scheduling unchanged. All-worker saturation remains a
separate limit; the [server baseline](SERVER.md) documents receive isolation and
its compute-saturation control.
