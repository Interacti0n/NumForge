# Numerical component extraction baselines — 19 October 2026

Selected extraction measurements and HTTP parity evidence. Input shapes,
timings and revisions below are preserved; they do not describe every later revision. See [benchmark tools](../../../benchmarks/README.md)
for current entry points.

The four original executables are now selectors of one tool. Their fixtures,
timing windows, budgets and independent checks are retained. New CSV rows use
`kernel,case,repeat,operations,points,evaluations,milliseconds,allocation_calls,requested_bytes`.
`operations` is visits, complete refinement calls, assessments or phase searches;
`points` applies to grids/refinement and `evaluations` to refinement probes/phase
callbacks. Unused fields are zero. Historical CSVs keep their original schemas
and recorded timings; consolidation does not claim a speedup.

## Exact grid baseline

`numerical_sampling_benchmark grid` visits exact coordinates and accumulates their rational
sum in the callback. It checks `sum = count*(lower+upper)/2` outside timing and
runs three repetitions per case. Build with `NUMFORGE_BUILD_BENCHMARKS=ON`, then
run the executable; no engine or server is involved. The measurement includes
callback arithmetic, but excludes setup, expected-value construction and checking.

On Windows x64, GCC 16.2 Release with allocation instrumentation (19 October 2026),
the median was 6.310 ms for 1024 points on `[-10,10]`, 0.368 ms for 33 points on
`[1E1000,1E1000+1]`, and 0.486 ms for 33 points on `[0,1E-1000]`.
[Raw repetitions](../data/NUMERICAL_GRID_2026_10_19.csv) also record
allocation calls and cumulative requested bytes, not peak live memory. These are
initial observations; this extraction does not establish a speed improvement.

## Adaptive refinement baseline

`numerical_sampling_benchmark refinement` uses the public scheduler with caller-owned
exact rational samples of `x*x` and a midpoint chord-error criterion. It checks
closed-form point counts and exact spacing outside timing, with three repeats
per case and a capacity of 257 points. Setup and validation are excluded;
callback evaluation, point insertion and queue allocation are included. This
does not measure expression parsing or the complete graph pipeline.

On Windows x64, GCC 16.2 Release with allocation instrumentation (19 October 2026),
medians were 3.324 ms for 129 points on `[0,1]`, 0.819 ms for nine points on
`[1E1000,1E1000+1]`, and 1.970 ms for 17 points on `[0,1E-1000]`.
The absolute midpoint tolerances were respectively `1/16384`, `1/64` and `1E-2002`.
[Raw repetitions](../data/NUMERICAL_REFINEMENT_2026_10_19.csv) include
allocation calls and cumulative requested bytes, not peak live memory. This is
an initial baseline; no before/after speedup is established. Scheduler index
updates retain the original worst-case quadratic work in the point limit.

## Curve acceptance baseline

`numerical_sampling_benchmark curve` measures 1000 calls to the reusable public curve
classifier, with three repetitions per case. It checks the expected decision
outside timing. Setup, classifier creation and validation are excluded; decimal
arithmetic and phase checks are included. The cases accept a linear triple,
reject an unsafe phase and reject a `1E-1000` midpoint deviation at a `1E1000`
offset under tolerance `1E-1001`. No expression parsing or rendering is measured.

On Windows x64, GCC 16.2 Release with allocation instrumentation (19 October 2026),
median times for 1000 assessments were 6.228 ms, 4.110 ms and 38.239 ms respectively.
[Raw repetitions](../data/NUMERICAL_CURVE_2026_10_19.csv) record allocation
calls and cumulative requested bytes, not peak memory. This is an initial baseline,
not evidence of a speedup from extraction.

The MSVC Release server at `5759ea5` and the extracted implementation also returned
identical complete parsed JSON for these `POST /api/graph/plot` requests, including
every coordinate, connection, classification and diagnostic field:

| Expression | x range | Extra query | Points |
| --- | --- | --- | --- |
| `x*x` | `[-1,1]` | defaults | 65 |
| `sin(33x)` | `[-10,10]` | defaults | 723 |
| `1/(x-0.1)` | `[-1,1]` | defaults | 771 |
| `floor(x)` | `[-2,2]` | defaults | 153 |
| `sqrt(x)` | `[-1,1]` | defaults | 127 |
| `sin(x)` | `[-4.71238798038469,-4.71238098038469]` | defaults | 65 |
| `sinh(10x)` | `[-2,2]` | fast mode, precision 40, 800x440, y `[-2,2]` | 265 |
| `floor(x)` | `[-2,2]` | fast mode, precision 40, 800x440, y `[-1,1]` | 153 |

Precision 40 selects the decimal fallback for those camera cases. This finite
comparison complements the C and browser graph regressions; it is not a proof
for all expressions or resource-limited executions.

## Phase anchor baseline

`numerical_sampling_benchmark phase` measures public callback phase location with exact
rational intermediates and three repetitions per case. Setup, locator creation
and validation are excluded. Timed work includes target selection, interpolation,
callback evaluation and phase-residual verification. The linear and large-offset
cases run 32 searches per repetition; the nonlinear case runs one. Coordinates
are checked outside timing against a linear closed form or an independent decimal
square root, and the accepted coordinate must match the last actual callback.

On Windows x64, GCC 16.2 Release with allocation instrumentation (19 October 2026):

| Case | Searches | Evaluations | Median ms |
| --- | --- | --- | --- |
| Linear phase, default precision/tolerance | 32 | 32 | 1.055 |
| Quadratic phase, 64 proposal digits, `1E-30` phase tolerance | 1 | 40 | 1082.772 |
| `1E1000` coordinate offset, `1E-1000` span, 1010 proposal digits | 32 | 32 | 20.904 |

[Raw repetitions](../data/NUMERICAL_PHASE_2026_10_19.csv) record allocation
calls and cumulative requested bytes, not peak memory. A trial batch of 32
quadratic searches did not complete under the configured 30-second/512-MiB scope;
the recorded protocol measures individual searches instead. Exact rational
interpolation can produce expensive growing intermediates. The graph adapter
retains eight callback evaluations per anchor and phase tolerance `1E-6`.
These measurements establish no before/after speedup.

The MSVC Release server at `e6750f4` and the extracted implementation also returned
identical complete parsed JSON for the same eight requests in the
[curve parity table](#curve-acceptance-baseline), including all actual anchor
coordinates, classifications, connections and diagnostics. The separate C/browser
regressions cover sine/cosine anchors, decreasing phases and degree/radian modes.
The new low-precision progress guard separately rejects rounded proposals on or
outside the current coordinate bracket; no callback evaluates those proposals.
