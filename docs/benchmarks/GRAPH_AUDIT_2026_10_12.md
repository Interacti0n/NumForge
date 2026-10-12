# Adaptive graph audit — 12 October 2026

`sin(3x)` on −10..10 exhausted the shared 64 MiB cumulative allocation budget.
This was temporary-object churn, not a mathematical size limit or a slow
five-second timeout. The optimized implementation plots that complete interval,
as well as mixed/nested functions, without lowering calculation precision.

## Reproduction and methodology

Build the opt-in `graph_benchmark` target with `NUMFORGE_BUILD_BENCHMARKS=ON`
in a Release configuration. Run `graph_benchmark 3`; its optional repeat count
is 1–10. The build needs no benchmark dependencies beyond C/libm.

These measurements use Windows x64, MSYS2 UCRT GCC 16.2.0, Ninja, `-O3 -DNDEBUG`,
warnings as errors, allocation statistics and test hooks enabled. Live-allocation
bookkeeping is **off**, because it would dominate the cost of small objects.
The baseline is the archived tracked tree at `a9554a2`, compiled separately with
the identical new harness and settings. The updated tree includes the changes
described here. Each row was measured three times; medians below are local
observations, not portable timing guarantees. There is no calibrated warm-up.

Raw data: [baseline CSV](graph_baseline_2026_10_12.csv),
[updated CSV](graph_after_2026_10_12.csv). Fifteen cases each run in three modes:

- Uniform C sampler with 129 inclusive points.
- Adaptive C sampler with automatically inferred parameter.
- Full HTTP **adapter call**, including request parsing and JSON serialization;
  this is not a socket/network/browser latency measurement.

Range parsing, result destruction and independent reference comparisons are
outside the timed window. Allocation calls/requested bytes are recorded before
reference checks and include transient work, not just retained data or RSS.
All operations retain their shared 5 s, 64 MiB cumulative and 512 KiB single
allocation bounds. Approximate calculation precision is 34 except `sine100`.
The original 12-place HTTP plotting projection is unchanged.

## Adaptive results

Times are median milliseconds; memory is cumulative requested MiB. Baseline
failure times are time to failure, not successful computation times.

| Case and x range | Before ms / MiB | After ms / MiB | After samples | Accepted x coverage | Limited |
| --- | ---: | ---: | ---: | ---: | --- |
| `3*x+2`, ±10 | 1.68 / 0.67 | 1.13 / 0.70 | 65 | 100% | no |
| `x*x`, ±10 | 1.06 / 0.60 | 0.85 / 0.62 | 65 | 100% | no |
| `sin(x)`, ±10 | 90.24 / 30.79 | 30.74 / 7.34 | 381 | 100% | no |
| `sin(3*x)`, ±10 | **memory failure** | 80.23 / 20.49 | 921 | 100% | no |
| `sin(3*x)+cos(2*x)`, ±10 | **memory failure** | 167.65 / 35.18 | 855 | 100% | no |
| `sin(3*x)*exp(-x*x/20)`, ±10 | 250.55 / 54.89 | 113.06 / 21.69 | 597 | 100% | no |
| `ln(1+x*x)`, ±1000 | 28.82 / 6.70 | 28.53 / 8.31 | 185 | 100% | no |
| `sin(3*x)`, ±1000 | **memory failure** | 123.13 / 21.96 | 1024 | 0% | yes |
| `sin(x)`, ±1000000 | **memory failure** | 125.73 / 18.38 | 1024 | 0% | yes |
| `sin(x*x)`, ±100 | **memory failure** | 139.03 / 21.46 | 1024 | 0% | yes |
| `1/(x-0.1)`, ±10 | 3.98 / 1.14 | 21.82 / 4.08 | 647 | 99.875% | yes |
| `tan(x)`, ±10 | 224.94 / 48.07 | 163.27 / 25.73 | 1024 | 89.795% | yes |
| `sin(3*x)`, ±1, 100 digits | 79.23 / 21.87 | 27.18 / 4.12 | 109 | 100% | no |
| `3*x+2`, ±10¹² | 1.24 / 0.62 | 0.84 / 0.65 | 65 | 100% | no |
| `ln(1+x*x)`, ±10¹² | 35.77 / 7.96 | 51.35 / 9.51 | 153 | 99.994% | yes |

The reciprocal/log cases spend more work resolving boundaries than the old local
quota allowed. Their higher cost buys substantially better coverage. This is
not a claim that every operation got faster. Full adapter median for `sin(3*x)`
is 101.82 ms / 21.26 MiB, versus an atomic memory failure before.

## Accuracy and resolution

The harness independently compares successful real samples with libm; relative
point errors above 1e-9 fail the harness. All measured successful cases pass.
For accepted chords, additional independent probes at 1/4, 1/2 and 3/4 measure
both relative function error and error normalized by `max(1, seeded y span)`.
The largest updated normalized chord error is approximately 0.0005934
(`sin(3*x)`), below the sampler's 0.001 scale criterion in these cases. These
finite checks do not certify all possible inputs or high-precision digits.
The existing C numeric/complex oracle suites separately validate high precision.

Zero chord error at **zero accepted coverage** does not mean a perfect curve.
Wide oscillatory examples reach the 1024-point resolution bound and leave
unresolved edges disconnected. Millions of periods cannot be represented
accurately by a few hundred display segments; narrow the x view. Smooth very
wide functions still work. New phase checks prevent the obvious aliasing that
would otherwise join samples hiding whole periods. They are sampled safeguards,
not mathematical continuity or domain proofs.

## Implementation

1. Forward sin/cos evaluate only the reduced series needed by their quadrant;
   tan and the paired C operation retain both. Already-converged series stop.
2. Taylor arithmetic reuses the existing per-call decimal workspace. Private
   sum objects swap owned coefficients instead of allocating transaction copies.
   Guard digits, rounding, argument reduction and public atomic publication stay
   intact. No global mutable scratch or alternate double evaluator is introduced.
3. Adaptive intervals are processed breadth-first over the whole view. The old
   per-seed quota is removed; total 1024 samples/depth 12 remain enforced.
4. Prepared expressions optionally capture forward trig arguments, including
   nested calls. Corresponding probes must differ by at most 0.5 rad / 30 deg;
   unavailable phases never authorize an otherwise misleading coarse chord.
5. Non-consuming private budget telemetry lets optional refinement stop early,
   keeping 2 MiB/200 ms for finishing plus twice the largest observed probe cost.
   Unexpected resource failures remain atomic and never clear/reset a caller's
   budget. Early admission is an estimate rather than a per-expression guarantee.
6. Empty input, edits, errors and all-nonreal results keep default −10..10 axes.
   Empty submit makes no HTTP request. SK/EN status recommends a smaller x view
   when unresolved intervals are omitted.

## Validation

- All 58 MSVC Release C/CLI/HTTP/oracle/fault tests pass, including public aliases,
  paired-operation failure atomicity and high-precision complex references.
- GCC allocation-instrumented application-expression/graph tests pass with zero
  tracked live allocations after cleanup. New checks cover phase ownership and
  injected faults, wide smooth ranges and non-consuming inherited-budget queries.
- All 21 focused browser graph/guide tests pass, including independent chord
  checks, `sin(3x)`, mixed/nested expressions, 100-digit requests, ±10¹² ranges,
  tangent poles, nested high-frequency phases and default axes in both languages.
- The 135-row updated benchmark sweep passes independent point checks. Guide,
  adaptive API contract and testing documentation describe resolution limits.

The next useful work is viewport pan/zoom and a display-resolution contract.
For dense oscillations, a separately defined envelope/overview mode could be
useful; it must not pretend to be an accurately sampled individual curve.

## Follow-up: coarser connected display

At the user's request, the line now prioritizes a connected overview for
recognized continuous expressions instead of strict interpolation everywhere.
The table above records the earlier optimization; the subsequent
[coarse display CSV](graph_coarse_after_2026_10_12.csv) records the same three-mode
sweep with an additional `sin(33*x)` case (144 rows, three repetitions).

The restricted continuous-tree classifier relaxes ordinary display tolerance to
0.005 of the seeded y span and permits a coarse fallback below 1/256 of the x view.
Those short intervals use a wider sampled phase check, 3 rad / 170 deg; larger
intervals retain the original strict phase guard. Unknown/domain/pole/jump trees
keep strict sampling. No C point precision or operation budget changed.

`sin(33*x)` at ±10 now returns 513 samples, full connected coverage and no limit
flag. Independent point errors remain below 1e-9. Its worst probed chord error
is about 0.201 in function value (0.101 of the seeded y span): this is a visibly
coarse overview of 105 periods, **not** a high-accuracy interpolant. The HTTP
regression checks every connection and bounds independent chord error by 0.21.
Ordinary `sin(x)` drops from 381 to 177 points; `sin(3*x)` drops from 921 to 457.
Pole, tangent, floor, mixed-domain and nested fast-phase checks remain covered.
Truly excessive oscillations still retain unresolved gaps and the smaller-range
message rather than becoming a misleading flat line.
