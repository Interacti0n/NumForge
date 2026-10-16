# Graph intersection refinement — 17 October 2026

Decision: use safeguarded Brent-Dekker refinement for sign-changing candidates.
It confirms more candidates with substantially fewer original-expression
evaluations in the measured dense graph, while retaining residual and budget
checks. This changes marker verification, not function sampling or curve geometry.

## Diagnosis

The baseline is `e714404`'s refinement with added diagnostic counters; its
instrumented MSVC snapshot is retained locally under
`build/graph-brent-legacy-2026-10-17`. Both modes use the existing request-local
cache and the same candidate generation, 32 rows/request, 512 actual evaluations,
24 refinements/candidate, HTTP 1-second budget and numerical tolerances.

In the four-curve browser scene `sin(33*x), cos(31*x), sin(17*x), cos(13*x)`,
legacy refinement used 1997 steps: 1758 bisections and 239 interpolation steps.
Its fixed 5%/95% rejection of near-endpoint secant steps repeatedly discarded
useful steps near roots. Across ten verification requests, 26 candidate rows
exhausted their refinement limit and ten were unfinished under the shared work
cap. Only 92 of the 128 selected candidates became markers.

The new method retains an opposite-sign bracket and uses secant or inverse
quadratic interpolation when the safeguarded step predicts useful progress;
otherwise it bisects. Every proposed coordinate goes through the original C
expressions, including rational subtraction before projection. The independent
implementation follows the algorithm described by [Netlib's Brent reference](https://www.netlib.org/go/zeroin.f)
and the [SciPy brentq documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.brentq.html).
No external numerical library is added.

Unlike an interval-width-only solver, this verifier **does not accept a small
bracket as a root**. The actual original-expression residual must still pass.
Non-real/unrepresentable samples, machine-coordinate exhaustion, poles and jumps
can leave a candidate unconfirmed. Same-sign tangency/cusp checks and discrete
point-pair checks keep their existing paths. These numerical checks do not prove
continuity, uniqueness or completeness of roots.

## Browser comparison

Windows, MSVC Release, Chromium. Five scenes × three repetitions × two modes =
30 performance checks, all passed. The runs were serial, with no simultaneous
builds or tests. Times include UI debounce, sampling, queueing and verification;
they are local observations, not portable latency promises. This run's controls
are slower than the previous day's measurements, so compare within this run.

| Scene | Legacy / Brent median ms | Markers | Actual verification evaluations |
| --- | ---: | ---: | ---: |
| `x!` | 122.68 / 115.26 | 1 / 1 | none |
| `ncr(x;3)` | 117.25 / 97.68 | 0 / 0 | none |
| Four dense sin/cos curves | 1794.37 / 1243.01 | 92 / 128 | 3889 / 1133 |
| Nested logarithm | 178.58 / 183.75 | 0 / 0 | none |
| Tiny positive minima + squared sin/cos | 1023.84 / 484.31 | 25 / 27 | 764 / 254 |

The dense scene uses about 71% fewer actual evaluations and settles about 31%
sooner while confirming all 128 selected candidates. Its steps fall from 1997
to 323 (one bisection, 322 interpolations), with no iteration-limit rejection or
unfinished row. Its marker state changes from limited to ready. This means all
selected candidates were confirmed, **not** that every mathematical intersection
in the interval has been enumerated.

The final scene uses `(x-0.1234)^2+1E-12`, `abs(x-0.1234)+1E-12`, `sin(x)^2`
and `cos(x)^2` (the actual fixture writes the small constants as decimals).
Both methods reject 12 candidate rows by residual/contact checks. The two extra
confirmations replace two legacy refinement-limit rejections; positive minima
are not silently snapped to zero. Unchanged scenes perform no marker checks,
so their small timing differences are not solver speedups.

Full [30-row browser data](GRAPH_BRENT_BROWSER_2026_10_17.csv). Work counts are
summed across requests; each request still has its own unchanged limits.

Cached redraw was also checked serially: four scenes per mode, five warmups and
20 measured redraws per scene. More confirmed markers increase drawing work.

| Scene | Legacy / Brent drawing median ms | Marker count | Brent maximum ms |
| --- | ---: | ---: | ---: |
| `sin(x)` | 1.7 / 1.9 | 8 / 8 | 2.2 |
| `sin(33*x)` | 3.1 / 3.1 | 43 / 65 | 4.0 |
| `x, 2-x, sin(3*x), cos(5*x)` | 3.6 / 4.0 | 68 / 99 | 5.8 |
| `abs(x*sin(x)), sin(33*x), cos(17*x), x*x` | 4.5 / 5.5 | 86 / 128 | 9.3 |

All eight redraw checks passed. These timings isolate synchronous drawing.
Scheduler/frame-wait medians fluctuate (the last candidate scene measured
29.6 ms including its frame wait); they are not a guarantee of 60 FPS or a
measurement on a physical phone. Raw results: `build/graph-brent-warm-*.log`.

## Controlled native comparison

GCC 16.2, normal Release libraries without live allocation bookkeeping, three
repetitions. The same private diagnostic entry point selects legacy/Brent, with
cache enabled in both modes. Reference checks use independent binary math and
require every legacy confirmation to remain confirmed at the same root.

| Batch | Legacy / Brent median ms | Actual evaluations | Confirmed candidate rows |
| --- | ---: | ---: | ---: |
| 32 distinct sine roots | 55.632 / 16.855 | 512 / 160 | 22 / 32 |
| 32 distinct sine/cosine intersections | 76.636 / 48.075 | 512 / 320 | 8 / 32 |
| Repeated shifted pole | 0.866 / 0.596 | 22 / 14 | 0 / 0 |
| Repeated step jump | 1.011 / 1.097 | 27 / 27 | 0 / 0 |
| Repeated skew cubic bracket | 2.211 / 0.573 | 27 / 8 | 0 / 32 |
| Repeated logarithm bracket | 7.134 / 1.729 | 27 / 7 | 0 / 32 |

Repeated tangent contacts remain confirmed and repeated positive minima remain
rejected. Repeated batches count rows asking about the same root, not distinct
roots; only the first two scenes use distinct roots. Cached steps and actual
evaluations are different units. Counts, rather than noisy tiny-batch timings,
are the primary evidence. Full [48-row native data](GRAPH_BRENT_NATIVE_2026_10_17.csv).

## Validation, reproduction and remaining work

Native checks cover distinct roots, opposite orientations, skew brackets, tiny
and large coordinates, non-real holes, poles, floor jumps, strict residuals and
allocation-failure atomicity. One initial fixture used lowercase `e` where the
project syntax requires uppercase `E` for a scientific exponent; the fixture was
corrected without changing the parser. Five native groups are covered in both
MSVC and GCC. Browser regressions include independent marker references at mobile
and desktop widths, repeated two-axis camera changes and retained sessions.
Final selected regressions: 41 Chromium checks and 56 Firefox/WebKit checks
passed. The existing allocation-failure suite and the new refinement-failure
checks passed in both compilers. No version bump or release publication is part
of this change.

The opt-in CMake target is `graph_brent_benchmark`; run it with repeat count 1–10.
`application_graph_verify_experiment` is private diagnostic infrastructure for
controlled comparisons. Production uses Brent with caching enabled.
The browser reproduction is `graph-analysis-performance.spec.js --repeat-each=3`;
raw logs/snapshots remain under ignored `build/graph-brent-*`.

Remaining work includes marker crowding on narrow screens, physical-phone touch
review and final CI/release validation. Arbitrary tangencies and unobserved roots
remain finite-sampling limits; this change does not introduce a general equation
solver or a symbolic continuity proof.
