# Per-call decimal series workspaces — 11 October 2026

This follows [GCD scratch reuse](GCD_WORKSPACE_2026_10_11.md) and the second
priority from the [performance refresh](PERFORMANCE_REFRESH_2026_10_11.md).

## Implementation

The logarithm and exponential series now own one private workspace for their
whole call. It retains coefficient alignment, raw arithmetic, quotient,
remainder, rounding-comparison and normalized division buffers. Repeated
significant rounding caches its last large power of ten; powers through 10^19
use a local one-limb divisor and do not evict that cache. Decimal alignment and
cached powers use 19-digit integer chunks, avoiding repeated decimal text
construction. Very large alignments retain the existing power-building path so
compact extreme exponents encounter allocation/budget rejection promptly.

Magnitude division shares the tested Algorithm D core with public division and
GCD. Significant precision selection is shared with the public decimal helper.
Public scalar addition/multiplication/division/rescaling keep their existing
paths. Rounded multiplication still normalizes cross-factor zeros before
combining scales, including overflow-cancelling and zero cases.

Only normalized, fully checked workspace coefficients are copied to a result.
An output can alias either input; failures preserve inputs and destinations.
Partial scratch remains reusable after a failure, and every buffer is freed at
series cleanup. Workspaces are per-call, with no global or thread-local cache.
The existing 24 series guard digits, final rounding, branch behavior, public C
API and calculator resource limits are unchanged. Functions composed from ln/exp
inherit the improvement; scalar sine/cosine series are not switched in this step.

## Measurements

Windows x64, AMD Ryzen 7 7435HS, MSVC 19.51.36247.0, Release with the existing
benchmark/fault instrumentation. Fresh baseline: `19ed8f0`. Current: that commit
plus this workspace change. Six complete calculator requests were measured in
three fresh processes before and after, serially after builds/tests finished.
CPU frequency/affinity and unrelated background work were not controlled.

Raw CSV, exact expected files, source revision/patch, the new workspace source,
measurement scripts and independent fixtures are retained in ignored
`build/performance-decimal-workspace-2026-10-11`.

| Complete calculator request | Before median | After median | Requested bytes before → after |
| --- | ---: | ---: | ---: |
| `ln(2)`, 2,000 output places | 362.074 ms | 334.780 ms | 23,195,765 → 3,927,519 |
| `exp(1)`, 2,000 places | 200.090 ms | 168.416 ms | 15,800,512 → 4,766,371 |
| `ln(1+i)`, 2,000 places | 573.298 ms | 547.181 ms | 37,972,543 → 18,572,659 |

All these requests succeed 3/3. Allocation calls fall from 64,474 to 6,070 for
real ln, 53,660 to 6,735 for exp, and 104,712 to 45,965 for complex ln. Requested
bytes are cumulative allocation traffic, not process resident memory. Retaining
scratch can slightly increase the instantaneous live payload while removing
repeated requests. Separate 2,000-digit C API probes record peaks of 29,046 bytes
for ln and 22,630 bytes for exp, including tracked inputs/result.

High-precision production outcomes:

| Request | Before | After, unchanged five-second / 64 MiB limits |
| --- | --- | --- |
| `exp(1)`, 5,000 output places | Allocation limit ~1.60 s | Success 3/3, median 2.499 s, 29,335,490 requested bytes |
| `ln(2)`, 5,000 places | Allocation limit ~2.13 s | Time limit 3/3, ~4.99 s, ~27–28 MB requested before interruption |
| `sin(1+i)`, 5,000 places | Allocation limit ~1.90 s | Still allocation limit 3/3; same allocation calls/bytes |

Failed-call times are not speed comparisons with completed calculations.
A separate diagnostic ln request, with only its harness deadline extended to
60 seconds, completes in **6.023 seconds**, requests **31,292,374 bytes**, and
matches all **5,004 output bytes** of the independent reference. The production
default remains five seconds: this request still does not fit it.

Every successful measured request matches the entire independently generated
Python Decimal result, evaluated at two larger precisions (80 and 160 extra
places). Complex ln uses ln(2)/2 and an independently computed Chudnovsky pi/4.
Exp at 5,000 output places checks all 5,005 bytes; no prefix-only validation.

Separate C API timing, live-memory and phase probes retain exact references.
Benchmark operation hooks also include private magnitude add/sub and reusable
magnitude division; direct coefficient alignment and cached power generation
are charged to the alignment phase. Profiling clocks remain disabled for normal
timing samples and are omitted entirely from ordinary builds.

One separate unrestricted 5,000-digit C API ln profile also matches the full
independent reference and verifies memory cleanup. Its timed API call is 6.548 s;
the separate profiled call totals 6.863 s. That diagnostic attributes about 32%
to divmod, 18% to multiplication, 18% to coefficient alignment/power preparation
and 31% to other precision/series work. These are separate probes, not fractions
of the calculator timer. Its peak tracked payload is 103,306 bytes and cumulative
requests are 30,955,993 bytes. This supports targeting repeated mantissa alignment
and rounding/division next rather than increasing the allocation budget.

## Verification

- MSVC x64: 27 affected arithmetic, calculator, application/web, independent
  complex and benchmark suites pass.
- GCC native: 19 corresponding arithmetic/reference/benchmark suites pass.
- MSVC Win32 and GCC portable division: seven corresponding suites each pass;
  normal builds do not include benchmark profiler code.
- Subsequent boundary/diagnostic updates pass focused reference, allocation,
  transcendental and benchmark suites. The final diagnostic wrappers pass both
  MSVC/GCC benchmark protocols, independent references and failure suites.
- Independent numeric oracle now checks **15,024 cases**, including **2,256 new
  workspace cases** for significant division/rounding and rounded addition/
  multiplication, all six rounding modes, signs, carries, zero, cancellation,
  varying scales/precision and one persistent workspace. The 16 pre-existing
  directed transcendental references retain their earlier one-ulp policy;
  new workspace references are exact.
- Deterministic tests inject every allocation failure/deadline checkpoint with
  fresh, differently warmed and identically warmed workspaces, both output/input
  aliases, and retry after each failure. Results/inputs remain unchanged on
  failure; retry and repeated cleanup succeed.
- Explicit tests cover cross-factor scale-overflow cancellation, equal extreme
  division scales, zero product despite extreme scales, invalid precision/null
  workspace/zero divisor and prompt memory rejection of a billion-place alignment.
- Separate memory probes verify complete cleanup of tracked allocations.

## Next work

This removes the ln/exp allocation obstruction without changing their series.
For the 5,000-place ln request, CPU cost is now the limiting factor. Measure
repeated mantissa alignment and rounding/division before changing its series;
consider bounded cached alignment powers or a specialized small-order division
path with exact rounding. A different reduction/binary-splitting algorithm would
need separate precision/error and cancellation checks.

Extend the same tested workspace foundation to scalar sine/cosine series next,
then recheck complete complex trigonometric requests at 5,000 places. Their
remaining allocation failure is observed directly above. Thresholded Karatsuba
and synchronous HTTP queue behavior remain separate priorities.
