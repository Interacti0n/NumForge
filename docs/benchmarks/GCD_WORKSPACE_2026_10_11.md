# Reusable GCD division scratch — 11 October 2026

This completes the first priority from the
[performance refresh](PERFORMANCE_REFRESH_2026_10_11.md).

## Implementation

The private Euclidean GCD loop now reuses its normalized numerator/divisor
buffers across all iterations and retains the existing rotating operand/remainder
ownership. Its private remainder-only path computes quotient digits needed by
Algorithm D but does not allocate or store an unused quotient. Single-limb and
zero/equal/smaller magnitude paths use the same arithmetic core.

Public division/modulo still stage both outputs and use their original temporary
allocation path. Their signed semantics, alias behavior and C API are unchanged.
GCD copies absolute inputs privately, publishes only a completed result, checks
the runtime budget at entry and retains cancellation checks throughout division.
Every scratch buffer is released on success and failure. Capacity growth uses
existing checked limb/byte arithmetic, including 32-bit overflow handling.
No production resource limits or release version change.

## Method and measurements

Windows x64, AMD Ryzen 7 7435HS, MSVC 19.51.36247.0, Release with the existing
benchmark counters/failure instrumentation. Before: `1b19e2c` production code,
recorded in `build/performance-refresh-2026-10-11` and its supplement. After:
that commit plus the accompanying GCD change. Raw results, exact fixtures,
expected output files, source revision/patch and a serial measurement script are
in ignored `build/performance-gcd-workspace-2026-10-11`.

Measurements ran serially after builds/tests finished; CPU frequency/affinity and
unrelated background processes were not controlled. Each median uses three fresh
processes. The large standalone GCD uses `--validate`: one verified API timing,
no warmup, and a separate memory probe. Calculator timings include the complete
pipeline and unchanged five-second, 64 MiB cumulative allocation, 512 KiB single
allocation and 65,536-byte output limits.

Standalone `gcd(2^100000 - 1, 3^63000)`, with preconstructed operands:

| Metric | Before | After |
| --- | ---: | ---: |
| API median | 800.101 ms | 710.525 ms |
| Allocation calls per API call | 233,096 | 6 |
| Cumulative requested bytes | 1,266,618,336 | 81,928 |
| Peak tracked live payload, including inputs | 123,296 | 114,696 |

The primary improvement is **cumulative allocation traffic**, not a gigabyte of
resident memory: the previous scratch was already freed after each iteration.
The live peak remains of the same order. API latency improves about 11% in this
recording; requested bytes fall by about 99.994%.

Complete calculator probes (Auto notation):

| Expression / output places | Previous outcome | Current median | Current requested bytes |
| --- | --- | ---: | ---: |
| `gcd(2^100000-1;3^63000)` / 10 | Allocation limit, ~54 ms | 743.536 ms | 704,281 |
| `2^100000/3^63000` / 10 | Allocation limit, ~53 ms | 3,668.249 ms | 1,563,797 |
| `(2^10000+i)/(3^6300-i)` / 2,000 | Allocation limit, ~49 ms | 76.132 ms | 387,166 |
| `lcm(2^10000-1;2^9999-1)` / 10 | Success, ~1.07 ms | 0.678 ms | 182,383 |
| `100000!` / 10 | Success, ~1.71 s | 1,682.668 ms | 1,332,155 |

All five probes succeed **3/3**, with `budget_reason=none`; every complete output
matches an independently generated Python integer reference. The complex probe
checks all **21,061 output bytes**, using separately reduced exact real/imaginary
fractions. Auto decimal references use exact integer quotient/remainder rounding
to half-even, rather than another NumForge path. Earlier failed-call latencies
are not speed comparisons with the now-successful full calculations.

Six public division fixtures (64/1,024 limbs; single-limb, similar, unequal,
exact and nonzero remainder) retain identical allocation counts/bytes and exact
outputs. Medians vary from about -1.8% to +4.3% versus the previous recording;
this is a portability/correctness and allocation control, not a claim that public
division was accelerated. Each uses the existing seven-sample API protocol in
three fresh processes.

## Verification

- MSVC x64: 25 affected arithmetic/calculator/application/web/reference suites pass.
- GCC native: nine corresponding integer/rational/complex/failure suites pass.
- MSVC Win32 and GCC portable division: eight corresponding suites each pass.
- Extended independent JS BigInt oracle adds 20 repeated GCD cases: Fibonacci
  chains, common factors, unequal sizes and a 16,000-bit power pair, both signs
  and reversed inputs. Updated oracle passes all four builds.
- Deterministic tests inject every allocation failure and deadline checkpoint
  for zero, equality, single/multiple limbs and repeated Euclidean remainders,
  including result/input aliases and identical inputs. Inputs and destinations
  remain unchanged on failure.
- A 100,000-bit GCD regression asserts exact result `3` under unchanged production
  memory caps, with no machine-dependent timing threshold.
- Benchmark file expectations are checked byte-for-byte outside timing. Harness
  tests reject mismatches, empty/truncated/trailing output and missing files;
  both MSVC/GCC protocols pass. `NUMFORGE_BENCH_EXPECTED_FILE` avoids platform
  environment-size limits for large exact results.

## Next priority

The large real rational probe now fits, but at roughly 3.67 seconds it has limited
five-second headroom on slower machines. This change does not remove Euclidean
arithmetic cost or guarantee arbitrary larger inputs succeed. If GCD latency
becomes the next bottleneck, benchmark a Lehmer/batched Euclidean prototype.

The next existing roadmap item remains per-call decimal series workspaces and
rounding/division/power-of-ten scratch, measured on complete 5,000-place requests.
High-precision decimal allocation failures, quadratic large multiplication and
synchronous HTTP queue behavior remain separate work.
