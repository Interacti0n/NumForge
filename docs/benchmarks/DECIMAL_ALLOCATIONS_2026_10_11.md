# Decimal temporary allocation reduction — 11 October 2026

This follows the [limb division change](DIVISION_2026_10_11.md). Faster integer
division exposed cumulative allocation volume as the next production constraint.
The calculator still uses its existing 5-second deadline and 64 MiB cumulative
allocation budget. This change does not raise limits or reduce precision.

## Implementation

- Addition/subtraction borrow coefficients already at the common scale. Only the
  operand needing multiplication by a power of ten receives a scaled temporary.
- Fixed-scale division borrows the unscaled coefficient, or both coefficients
  when no scaling is required. Borrowed inputs stay intact until final commit.
- Half-up/even quotient rounding copies and doubles just the remainder, then
  compares magnitudes with the original divisor. It no longer creates absolute
  copies of both operands or a heap-allocated multiplier. The adjustment uses
  a read-only stack representation of one with the existing signed arithmetic.
- Equal-scale comparison uses exact signed integer comparison, with a budget
  checkpoint before updating its output; it does not serialize either integer.
- Logarithmic/exponential series write the next sum into their other private
  result and exchange owners after comparison, avoiding a deep copy per step.

All output mutations retain private temporaries and final atomic commit. There
is no global pool, shared mutable scratch storage or public API change.

## Measurement method

Baseline is commit `5d8a1e8`, built from an isolated `git archive` using MSVC x64
Release with benchmarks and allocation-failure instrumentation enabled in both
builds. Three fresh-process runs
alternate baseline and changed binaries, serially after builds/tests complete.
The C API `log10(2)` case uses 2,000 significant digits and a frozen independent
reference; `--validate --timing-only` performs one call without warmup or phase
instrumentation. The calculator cases use Auto notation and 2,000 output places,
including their normal guarded working precision and complete pipeline.

Allocation and phase diagnostics are separate calls. Requested bytes count
cumulative allocation requests, not peak live RAM; freeing a temporary does not
refund the calculator's request budget. Failure timings measure the point of
budget exhaustion, not successful computation latency.

## Results

| Measurement | Before | After |
| --- | ---: | ---: |
| C API `log10(2)`, 2,000 significant digits, median | 1.088 s | 0.964 s |
| C API allocation calls, separate probe | 177,304 | 129,613 |
| C API cumulative requested bytes | 83,342,069 | 66,018,871 |
| C API peak tracked live payload | 21,664 bytes | 21,507 bytes |
| Complete `ln(2)`, 2,000 places, median | 0.577 s | 0.507 s |
| Complete `ln(2)` requested bytes | 42,875,862 | 34,168,452 |
| Complete `sin(1E100)`, 2,000 places, median | 0.579 s | 0.515 s |
| Complete `sin(1E100)` requested bytes | 44,161,836 | 36,455,452 |

The log10 probe reduces allocation calls by **26.9%** and requested volume by
**20.8%**, with about **11.3%** lower median latency. Peak live payload changes
little: this targets repeated allocation volume rather than excessive live RAM.
Its fresh-process time range is 1.078–1.092 s before, 0.964–1.042 s after.

The following calculator rows retain the default deadline/allocation budget:

| Expression, 2,000 places | Before, three runs | After, three runs |
| --- | --- | --- |
| `log(2;10)` | Allocation limit | Success, 0.980–1.039 s |
| `ln(2)+ln(3)` | Allocation limit | Success, 0.964–1.029 s |
| `tan(1+i)` | Allocation limit | Allocation limit |

Successful `log(2;10)` requests 66,651,865 bytes; the compound logarithm requests
65,930,583 bytes. Both now fit, but their headroom below 67,108,864 bytes remains
small (about 0.44 and 1.12 MiB). Higher precision or larger expressions may still
reach the unchanged limit. This does not establish support for every 2,000-place
complex expression. The tangent's later failure is not a successful result or
a latency regression: reduced churn allows more work before budget exhaustion.

The separate log10 phase probe records approximately 436 ms integer core and
557 ms other decimal work, versus 427 ms and 666 ms before. Phase instrumentation
has its own overhead; these are aggregate categories, not isolated allocator
measurements or subdivisions of the headline timing.

The next candidate is the remaining coefficient-to-text work used to count
significant digits and select division precision, plus temporary churn in
complex/trigonometric paths. These require exact digit-boundary and rounding
tests before changing their algorithms; raising the budget is not part of this
optimization.

## Verification

- MSVC x64: 16 rebuilt decimal/property, root/transcendental/trigonometric/
  hyperbolic, complex, calculator/contract, session/application, unit and API
  suites, including allocation failures and the independent numeric oracle.
- GCC x64 and actual MSVC Win32: decimal, allocation-failure and numeric oracle
  suites pass. The numeric oracle checks 8,395 cases per backend, retaining the
  existing 16 bounded directed-rounding allowances.
- New tests exhaust allocation failures and deterministic deadline checkpoints
  for 14 addition/subtraction/division cases with separate output or either
  input as output. They cover both scale-alignment directions, positive/negative
  division scaling, both divisor signs, all six rounding modes and halfway ties.
  Failure preserves both inputs and original output; successful results match
  explicit independent expectations.
- Additional tests verify zero allocations in equal-scale comparison and an
  unchanged comparison output on cancellation; logarithm/exponential tests
  exhaust allocation/deadline positions while overwriting their own input.
- All 1,067 decimal/math reference rows pass: 1,058 expected successes and nine
  expected domain/scale failures, with the 16 existing bounded directed results.
- Full calculator output texts for `ln(2)`, `log(2;10)` and `ln(2)+ln(3)` at
  2,000 places match independent Python `decimal` calculations. References agree
  at 2,240 and 2,400 working digits, rounded HALF_EVEN to 2,000 decimal places
  before scientific notation; they are not generated by NumForge.

Raw recordings are under ignored `build/performance-decimal-copies-2026-10-11`.
`matched-*` files record the final implementation with matching instrumentation;
`complete-*` used the final code but initially omitted failure instrumentation
from the archived baseline, so those timings are excluded. `after-*`, `final-*`
and `series-after-*` are intermediate exploratory measurements. An initial
calculator reference check used significant rather than decimal places; the
corrected `validated-*` checks above pass with Auto's documented semantics.
No server restart,
push or release is included.
