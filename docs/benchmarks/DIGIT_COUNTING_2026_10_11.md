# Exact digit counting without serialization — 11 October 2026

This follows [decimal allocation reduction](DECIMAL_ALLOCATIONS_2026_10_11.md).
Significant rounding and division previously serialized whole integer
coefficients just to count or compare decimal digits. The replacement keeps
these operations numeric. Precision, rounding, public APIs and application
resource limits are unchanged.

## Implementation

The private `bigint_decimal_digits` helper handles a single 64-bit limb directly.
For larger coefficients it obtains conservative integer bounds from bit length
using `0.30102 < log10(2) < 0.30103`. Products are split into quotient/remainder
parts to avoid size overflow on 32-bit targets. When both bounds agree, their
count is exact and no allocation is needed. Otherwise one private magnitude is
divided into base-10^19 chunks and the remaining limb is counted directly. There
is no string or chunk-array allocation. Signs do not affect magnitude length;
zero has one digit. Cancellation/failure leaves the output count unchanged.

Significant rounding uses this count to choose the same rescaling as before.
Significant division counts both coefficients and compares their mantissas by
aligning the shorter coefficient with decimal powers. Alignment multiplies a
private magnitude by 10^19 chunks and a final 64-bit decimal power, avoiding
text construction/parsing of the power of ten. Equal lengths compare magnitudes
directly. Cancellation and allocation failures discard private scratch storage;
the original inputs and output remain unchanged, including aliases. External
decimal scales are applied after coefficient division as before.

The bit bounds never approximate a mathematical result: ambiguous bounds always
take the exact fallback. No floating-point estimate or shared mutable cache is
introduced. Counting very large ambiguous coefficients still performs chunk
division; this is not a claim of constant-time digit counting.

## Measurement

Baseline is `477b5b7`, measured before editing using the existing MSVC x64 Release
build with benchmark and failure instrumentation. Three fresh-process timing-only
calls before and after are serial, with no overlapping builds/tests. Each C API
`log10(2)` call uses 2,000 significant digits and validates its full result against
the frozen independent reference. Allocation diagnostics are separate calls.
Calculator cases use Auto notation and 2,000 decimal places, including guarded
working precision and the normal 5-second/64 MiB cumulative budget.

The prior report's calculator allocation totals are comparable deterministic
counts from the same pre-change implementation. Its timings are older samples,
not interleaved baseline measurements for this change. Requested bytes are
cumulative allocation volume rather than live physical RAM.

## Results

| C API `log10(2)`, 2,000 significant digits | Before | After |
| --- | ---: | ---: |
| Fresh-process median | 0.978 s | 0.724 s |
| Fresh-process range | 0.962–1.005 s | 0.705–0.738 s |
| Allocation calls, separate diagnostic | 129,613 | 124,998 |
| Cumulative requested bytes | 66,018,871 | 44,843,035 |
| Peak tracked live payload | 21,507 bytes | 21,507 bytes |

Median latency decreases about **26%** and requested volume about **32%** versus
the previous optimization. Fewer large text buffers matter more than the number
of allocations here; allocation-call count decreases only about 3.6%.

| Complete calculator, 2,000 places | Three-run median | Requested bytes | Outcome |
| --- | ---: | ---: | --- |
| `ln(2)` | 0.367 s | 23,195,797 | Success |
| `log(2;10)` | 0.699 s | 45,281,988 | Success |
| `ln(2)+ln(3)` | 0.710 s | 44,714,599 | Success |
| `sin(1E100)` | 0.428 s | 26,255,405 | Success |
| `tan(1+i)` | 0.999 s until exhaustion | 67,108,946 | Allocation limit |

The prior implementation requested 66,651,865 bytes for `log(2;10)` and
65,930,583 for the compound logarithm. The new calls have about **20.8 and
21.4 MiB** headroom below the unchanged 64 MiB cumulative budget. They previously
had only 0.44 and 1.12 MiB headroom. This remains expression/precision dependent,
not a guarantee that arbitrary complex expressions fit.

The separate phase probe records roughly 415 ms integer core and 256 ms other
decimal work, versus 424 ms and 544 ms before. These instrumented aggregate
categories are separate calls rather than components of the headline samples.
Complex tangent still exhausts the budget; its later failure allows more work
before exhaustion and is not a successful result or a computation-time regression.
The next candidate is temporary churn within complex/trigonometric evaluation,
with no budget increase implied by this change.

## Verification

- Expanded independent native JavaScript BigInt/exact-rational oracle: **12,748
  cases**, including 4,353 new checks. Decimal power boundaries from 1 through
  256 digits and selected 500/2,000/10,000/19,000 powers, both signs, binary
  boundaries through 1,024 bits, significant-rounding carries and all six
  rounding modes, and near-equal mantissas in both alignment directions.
  The previous 16 bounded directed transcendental results remain unchanged.
- New digit-count fault tests exhaust every allocation and deterministic deadline
  position while asserting unchanged input/count on failure. Significant-division
  fault tests cover both alignment directions and equal lengths, with separate
  output or either input as output.
- MSVC x64: 16 rebuilt numeric/property, calculator/contract, root, transcendental,
  trigonometric/hyperbolic, complex, session/application, unit and API suites.
- GCC x64 and actual MSVC Win32: decimal, allocation-failure and numeric oracle
  suites pass. The 32-bit build exercises the overflow-safe bound calculation.
- The full 1,067-row decimal/math corpus passes: 1,058 expected successes,
  six expected invalid-domain rows and three expected division-by-zero rows;
  the existing 16 bounded directed results remain unchanged.
- Three complete runs of `ln(2)`, `log(2;10)` and the compound logarithm match
  the independent full Python Decimal output references from the previous
  report at 2,000 places. Failed tangent rows are explicitly retained as failures.

Raw measurements are under ignored `build/performance-digit-count-2026-10-11`.
`count-only-profile`, `ratio-profile` and `alignment-profile` are exploratory
probes; `final-after-*` records the final implementation, including immediate
return after a failed alignment multiplication, and complete reference checks.
No server restart,
push or release is included.
