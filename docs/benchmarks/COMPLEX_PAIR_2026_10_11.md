# Shared scalar evaluation in complex functions — 11 October 2026

This follows [exact digit counting](DIGIT_COUNTING_2026_10_11.md). At 2,000 output
places, complex tangent still exhausted the calculator's cumulative allocation
budget. Inspection identified repeated full trigonometric evaluations rather
than a need to raise the budget.

## Implementation

Public scalar sine and cosine each internally compute the same guarded pair,
then select and round one output. Complex forward functions called both APIs,
duplicating argument reduction, the sine/cosine series and their allocations.

The private `bigdecimal_sin_cos_pair` computes that pair once using exactly the
same non-adaptive reduction and 24 guard digits as the two public calls. It
rounds each component to the same requested precision and rounding mode. Both
outputs are staged privately; neither changes until all fallible work is done.
Outputs must be distinct, and either can alias the input. Zero and validation
paths follow the same precision/mode rules and preserve outputs on error.

The ordinary complex forward helper and scaled complex `tanh` now use this pair.
Consequently complex `sin`, `cos`, `tan`, `sinh`, `cosh` and `tanh` all avoid the
duplicated scalar work. The scaled tangent identity, complex formulas, guard
precision, directed rounding and public scalar functions are unchanged. No
public C API, cache policy, deadline or memory/output limit is changed.

## Measurement method

Baseline is `a7784ea`, rebuilt from an isolated `git archive` using MSVC x64
Release with benchmark and allocation-failure instrumentation enabled in both
builds. Three fresh-process runs alternate baseline/changed binaries, serially
after builds/tests finish. Calculator probes evaluate each function at `1+i`,
with Auto notation, 2,000 decimal places and the existing 5-second/64 MiB budget.
Failure timings describe resource exhaustion, not a completed calculation.

C API timing uses `complex_oracle_driver` at 2,000 significant digits per
component, without the calculator budget; output conversion is outside its
timed operation. The allocation/live-payload probes are separate `--memory`
calls. These different precision contracts and scopes must not be conflated.

An initial scalar calculator diagnostic at 2,000 places requests approximately
27.1 MB for each separate `sin(1)` and `cos(1)` call, illustrating the duplicated
series work. These independent stage probes are not additive components of
the complex pipeline or a substitute for the full measurements.

## Results

All six calculator probes previously exhausted the cumulative allocation budget
in all three samples. All six now succeed in all three samples, under the same
precision, notation and limits:

| Expression, 2,000 places | Before | After median | After requested bytes |
| --- | --- | ---: | ---: |
| `sin(1+i)` | Allocation limit | 0.795 s | 58,936,247 |
| `cos(1+i)` | Allocation limit | 0.783 s | 58,938,361 |
| `tan(1+i)` | Allocation limit | 0.614 s | 43,431,881 |
| `sinh(1+i)` | Allocation limit | 0.792 s | 58,936,247 |
| `cosh(1+i)` | Allocation limit | 0.784 s | 58,936,245 |
| `tanh(1+i)` | Allocation limit | 0.569 s | 42,368,604 |

Successful tangent has about **22.6 MiB** headroom below the unchanged 64 MiB
cumulative budget, while sine/cosine/sinh/cosh have about 7.8 MiB. These are
specific expressions, not a guarantee that arbitrary 2,000-place complex
expressions fit. Successful tangent's range is 0.607–0.622 s. Baseline failure
timings cannot be used as completed-operation speed comparisons.

The complete unbudgeted C API tangent permits a valid before/after comparison:

| C API `tan(1+i)`, 2,000 significant digits | Before | After |
| --- | ---: | ---: |
| Three-run median | 1.049 s | 0.604 s |
| Three-run range | 1.047–1.088 s | 0.591–0.617 s |
| Separate probe allocation calls | 204,165 | 129,201 |
| Separate probe requested bytes | 71,088,747 | 43,251,828 |
| Separate probe peak tracked live payload | 31,714 bytes | 30,642 bytes |

This is approximately **42.4% lower latency** and **39.2% lower requested
allocation volume**. Peak live payload changes little: the optimization avoids
repeating work, rather than addressing excessive live RAM. Requested volume is
not physical memory consumption, and free() does not refund the application
budget.

Other unbudgeted C API probes request about 87.1 MB before / 59.4 MB after for
sin/cos/sinh/cosh, and 70.4 MB before / 42.7 MB after for tanh. All six functions'
full real/imaginary output texts at 2,000 significant digits match exactly
between baseline and changed builds, in addition to the independent tangent
reference. Component formatting is outside the timed operation.

The next potential saving is shared hyperbolic scalar evaluation: ordinary
complex composition still calls sinh and cosh separately. That extension must
retain the small-argument sinh path and separate rounding contracts; it is not
included here. This change alone resolves the measured tangent failure without
raising limits or changing its numerical formula.

## Verification

- Paired scalar results equal the separate public results for 10 inputs, all
  six rounding modes and three output/input alias configurations (180 cases).
  Inputs cover zero, signs, all quadrants, a large angle and a near-pole value.
- New failure tests exhaust allocation and deterministic deadline positions
  for zero/nonzero input and all three alias configurations. Both outputs and
  the original input remain unchanged on failure; invalid pointer, same-output,
  precision overflow and invalid rounding contracts are checked too.
- MSVC x64: 21 rebuilt CTest suites, including numeric and decimal properties,
  calculator/session/application/API, trigonometry/hyperbolic, paired failures,
  all four complex oracle precisions and complex memory smoke.
- GCC x64 and actual MSVC Win32: eight suites each, including paired functional/
  failure tests, the 12,748-case numeric oracle and all complex oracle precisions.
  The existing 16 bounded directed scalar reference results remain unchanged.
- MSVC/GCC additionally pass `bigcomplex_tests --trig-only`, which exhausts
  every allocation position in the eight existing forward-function scenarios,
  including scaled and unscaled tangent/hyperbolic-tangent paths. The new test
  selection leaves ordinary CI's exhaustive allocation shards intact.
- Complete calculator `tan(1+i)` output at 2,000 places and both C API components
  at 2,000 significant digits match independent Python Decimal references.
  Direct Taylor sine/cosine at 2 and Decimal exponential provide the independent
  formula; results agree at 2,240 and 2,400 working digits before final HALF_EVEN
  rounding. This reference does not use NumForge's reduction or scaled formula.
- The full 1,067-row decimal/math reference corpus passes, including its nine
  expected domain/division failures and 16 existing bounded directed results.

Raw recordings and the independent reference generator are under ignored
`build/performance-complex-pair-2026-10-11`. `final-*` files record the final
implementation and rebuilt baseline. Initial exploratory probes are separate.
No server restart, push or release is included.
