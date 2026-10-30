# Rational arithmetic and repeated sampling — 30 October 2026

Windows x64, Ryzen 7 7435HS (16 logical CPUs), approximately 32 GiB RAM,
GCC 16.2 Release (`-O3 -DNDEBUG`). Numeric source baseline `69abc00`; optimized
implementation `3845eca`. Matching benchmark harnesses were used against both
versions. No CPU affinity or frequency lock. Timings below are local observations,
not portable performance promises or CI thresholds.
Both numeric builds enable allocation counters and test failure hooks; live
tracking and profile clocks run only in separate diagnostic passes. The initial
direct recording with unmatched test-hook settings was superseded before the
tables below were selected. Recorder metadata now includes those build switches.

## Changes and safety boundaries

- Rational arithmetic uses detached stack descriptors for temporary integers.
  Their owned limb buffers move to the destination only after success; there is
  no process-wide scratch pool. Equal denominators, zero and unit operands avoid
  general arithmetic. Cross-cancelled products are already reduced; sums need
  only reduce against the common denominator factor.
- GCD with a power-of-two magnitude uses the exact trailing-bit identity. Signs,
  zero, aliases and cancellation still follow the ordinary public contract.
- Exact-first decimal division removes factors of two with one bit shift, and
  factors of five in blocks of 27 plus a scalar tail. It retains every digit of
  terminating quotients, including when the requested significant precision is
  smaller. Recurring quotients retain all six rounding modes.
- Prepared expressions that do not reference their parameter evaluate once per
  sample batch. Returned typed values are independently owned copies. Snapshot
  binding, random-call rejection, domain errors and graph-phase checks remain
  active. Parameter-dependent subtrees are not folded or approximated.

Public bisection, interpolation policy, tolerances, precision and application
resource defaults were not changed.

## Complete public root search

Three alternating before/after fresh-process runs of
`numerical_benchmark --high-precision`, with no other build/test/benchmark from
this review running alongside them. This is the exact-rational callback for
`x*x-2` on `[0,2]`, not the direct decimal `sqrt` operation. Independent Newton
sqrt and original residual checks run outside timing. Iterations/evaluations
are unchanged: 42/45, 335/338 and 3324/3327 respectively.

| Absolute places | Before median ms | After median ms | Before / after allocation calls | Before / after requested bytes |
| ---: | ---: | ---: | ---: | ---: |
| 12 | 1.3643 | 0.7985 | 11,705 / 4,059 | 202,371 / 71,571 |
| 100 | 66.5103 | 7.3807 | 199,435 / 36,069 | 8,034,623 / 1,757,607 |
| 1000 | 22,832.7797 | 2,392.3473 | 11,940,195 / 373,794 | 4,708,937,497 / 139,837,913 |

The 1000-place median is approximately 9.5 times faster, with 96.9% fewer
allocation requests and 97.0% less requested traffic. Requested bytes are
cumulative, not peak/live memory or RSS. This stress uses the harness's
120-second budget, unlimited cumulative requests and 8 MiB single-allocation
cap; it does not establish success under smaller application budgets.

## Direct arithmetic and conversion

Medians of three process medians, five calibrated samples per row. Fixture
construction, full text checks and alias checks are outside timing. Each row
uses the same canonical operands in both versions; bits identify the generator
parameter rather than a promise that every component has that exact bit count.

| Fixture | Before / after microseconds | Before / after allocation calls |
| --- | ---: | ---: |
| Equal-denominator addition, 1024 bits | 3.659 / 2.608 | 29 / 9 |
| Coprime multiplication, 1024 bits | 11.422 / 6.827 | 39 / 18 |
| Cross-cancelling multiplication, 1024 bits | 2.524 / 1.418 | 30 / 12 |
| Integer addition, 256 bits | 2.290 / 0.354 | 26 / 3 |
| Dyadic conversion, 1024 parameter, HALF_EVEN | 282.337 / 24.055 | 1068 / 37 |
| Power-of-five conversion, 1024 parameter, HALF_EVEN | 547.600 / 138.360 | 1069 / 104 |

Separate profile passes record GCD, division and multiplication counts/time;
their clocks are excluded from speed recordings. Several short conversion rows
initially appeared 24–59% slower in the broad corpus. Seven alternating focused
processes per version, covering all five flagged rows plus six negative recurring
rounding modes, produced before/after speed ratios of 0.99–1.13. No repeatable
slowdown was established. Retain those noisy observations instead of claiming
every row improves.

## Repeated expression samples

The exact 129-point stage in `graph_fast_benchmark 3` compares typed samples
outside timing with independent numerical references. For a parameter-independent
horizontal `ln(2)` curve, median time changed from 21.852 to 0.732 ms; `1/3`
changed from 2.549 to 0.654 ms. This combines batch-local reuse with arithmetic
improvements, not a speedup for arbitrary variable-dependent functions. The
wide-sine and million-range-sine rows were nearly unchanged; these observations
do not support a claim that every graph or browser frame is faster.

## Browser observations

Ordinary uninstrumented GCC Release servers were measured in headless Chromium
at 1280x900 and 390x844, three repetitions of mixed, dense/pole and precision
camera scenes for each source version (36 completed scenes). Initial-frame
medians in milliseconds include interaction/scheduling overhead:

| Scene | Desktop before / after | Mobile-layout before / after |
| --- | ---: | ---: |
| Mixed four functions | 203.9 / 202.7 | 143.2 / 170.7 |
| Dense functions and poles | 192.1 / 194.7 | 190.7 / 191.9 |
| Precision hyperbolic camera | 414.8 / 329.5 | 247.2 / 277.4 |

No initial curve errors were reported. These observations move in both
directions and do not establish a general UI speedup. The harness runs versions
in order and does not lock frequency; mobile layout is desktop emulation, not a
physical phone. Initial-frame and complete settling/quiet-interval timings are
different scopes. Detailed frame/input/request/cache/heap observations remain
in `browser-multi.json` and `browser-precision.json`.

The old diagnostic incorrectly assumed every post-zoom visibility toggle was
a cache hit. The original server reproduced that failure: bounded caches and
unresolved ranges can require work after camera changes. The tool now measures
toggles in both versions, records hits/misses and checkpoints partial/failing
runs. A trial requirement that every curve retain a visible path also failed
on the original precision-camera scene; the camera can put curves off-screen.
Selected-curve restoration and curve errors remain checked. The failed
reproduction and partial recordings are retained; controlled no-request cache
reuse remains a dedicated browser regression rather than a blanket benchmark
assumption.

Selected regressions passed: 29 Chromium checks for camera/precision,
reconciliation, release scenes and controlled visibility reuse, plus 26 Firefox
and WebKit checks for precision/release scenes. Missing matching Firefox/WebKit
test binaries were installed under ignored `build/` before the successful rerun.
No physical-phone measurement was available.

## Verification and reproduction

The new Python checker validated 11,362 independent `Fraction`/`Decimal` results
in both versions: fixed operand shapes plus 1000 deterministic random pairs,
arithmetic aliases, six conversion rounding modes, corrupt-reference rejection
and a separate zero-live-payload teardown pass. Exact finite text is checked in
full, rather than through floating-point projections.

Native tests cover power-of-two boundaries at 0/1/63/64/65/127/128/1000/4096 bits,
both GCD input aliases and signs/zero. Rational allocation and deadline tests
exercise each actual failure point on canonical fast/general paths. Constant
batches check snapshot lifetime, independent ownership, domain errors and atomic
copy failures. The statistics and general-linear-system deadline tests now walk
every actual checkpoint to completion; their former fixed late indices became
obsolete when the operations required fewer checks.

MSVC Release: 83/83 CTest checks passed. Instrumented GCC Release: 113/115 passed
initially; after strengthening those two deadline fixtures, both passed on
rerun, completing the 115-check set. This includes the existing independent
numeric/division/complex oracles, allocation suites and benchmark checks.

```sh
cmake --preset benchmarks -DPython3_EXECUTABLE=/path/to/python3
cmake --build --preset benchmarks
python benchmarks/tools/check_bigrational_benchmark.py BINARY_DIR/bigrational_benchmark --random 1000 --record build/rational-recording
BINARY_DIR/numerical_benchmark --high-precision
BINARY_DIR/graph_fast_benchmark 3
```

Use `.exe` and configuration subdirectories where applicable. To compare old
numeric sources, compile the same current benchmark harness against their
snapshot, and identify that snapshot separately from the driver checkout.

Raw recordings, binaries, patches, the original source archive and metadata
remain under ignored `build/performance-20261030/`. `rational-verified-before/`
and `rational-verified-after/` contain the direct recordings; `paired-roots-*` contains
the alternating root runs; `graph-verified-before.csv` and `graph-verified-after.csv`
contain the complete native graph rows. `followup-*` retains the noisy-row
follow-up. Browser and server observations are separate timing scopes.
