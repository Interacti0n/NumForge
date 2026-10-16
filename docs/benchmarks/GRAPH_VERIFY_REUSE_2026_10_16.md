# Reusing graph verification checks — 16 October 2026

Baseline browser/server snapshot: `3c049c7` (also the executable reviewed by
`59d9c6b`). The candidate adds a bounded request-local cache to original-expression
verification. Sampling, root tolerances, 512 actual evaluations, 24 refinements,
32 candidates and the HTTP 1-second budget remain unchanged.

## Implementation and correctness

The cache has 128 entries in 32 four-way buckets with bounded FIFO replacement.
Its key is the decimal coordinate actually passed to C (`%.16g`) and single/pair
mode. It retains the complete checked sample, including the original difference
sign and the rational difference calculated before projection. It does not
subtract two rounded ordinates or reuse data between expressions/requests.

Successful non-real classifications are reusable too; allocation/domain handling
continues through the original evaluation contract. Failed operations are not
stored. Hits still check cancellation and time/memory reserves, and preserve the
current caller's approximate x when binary coordinates share one decimal input.
Storage is fixed stack data with no additional owned typed numbers. Collisions
can cause recomputation, never acceptance of a different coordinate.

The private profiling entry point permits the same verifier to run with reuse
disabled/enabled and reports actual evaluations, sample lookups and cache hits.
Production always enables reuse. Optional output counters are atomic on failure.
HTTP responses add the same diagnostic counters without changing schema version.
See the [protocol reference](../reference/GRAPH_ANALYSIS_HTTP_API.md).

## Controlled benchmark

The opt-in `graph_verify_benchmark` compares both modes on identical 32-candidate
batches. Duplicate/shared-endpoint scenes intentionally expose cache opportunity;
distinct sine/pair scenes expose low-reuse work. A pole must never become a root,
and a large rational-cancellation pair checks subtraction before projection.
Every baseline confirmation must remain confirmed with matching coordinates.
Additional confirmations are allowed when reuse saves evaluation budget.

Build with `NUMFORGE_BUILD_BENCHMARKS=ON`, then build and run
`graph_verify_benchmark 3`. This is a private application benchmark, not the
installed public C API. The diagnostic cache-off path shares counter/hash setup
with the cache-on path, so it is a controlled algorithm comparison, not a
byte-identical performance measurement of the old executable.

Windows x64 GCC 16.2 Release, three repetitions, serial after browser regressions.
The final benchmark links the normal Release libraries without live allocation
bookkeeping. Medians below are local observations, not portable guarantees.

| Batch | Cache off / on ms | Actual evaluations off / on | Confirmed rows off / on |
| --- | ---: | ---: | ---: |
| Duplicate sine candidates | 21.872 / 1.252 | 512 / 27 | 18 / 32 |
| Distinct sine candidates | 27.440 / 26.662 | 512 / 512 | 22 / 22 |
| Shared sine bracket | 20.500 / 20.817 | 512 / 512 | 22 / 24 |
| Distinct sine/cosine pairs | 36.574 / 36.777 | 512 / 512 | 8 / 8 |
| Repeated pole checks | 8.056 / 0.898 | 512 / 27 | 0 / 0 |
| Repeated rational pair | 0.412 / 0.020 | 64 / 2 | 32 / 32 |

Duplicate sine/pole batches reuse 837 of 864 lookups (96.875%). The former is
about 17 times faster in this deliberately favorable case. Confirmed rows are
candidate rows, **not distinct mathematical roots**: duplicate batches repeatedly
ask about the same root. Shared brackets save enough work to process more rows
at the same actual-evaluation cap, without necessarily becoming faster overall.
Differences in low-reuse timings are small and noisy; no general speedup is
claimed. Full [36-row data](GRAPH_VERIFY_REUSE_NATIVE_2026_10_16.csv).

## Browser comparison

Chromium/MSVC, five cold scenes, three repetitions per snapshot, serial runs.
This comparison uses the actual old executable, including its original request
scheduling. Times include debounce, plotting and asynchronous marker checks.

| Scene | Before / after median ms | Markers before / after | New cache hits / lookups |
| --- | ---: | ---: | ---: |
| `x!` | 86.37 / 89.94 | 1 / 1 | no verification |
| `ncr(x;3)` | 85.00 / 80.78 | 0 / 0 | no verification |
| Four dense sin/cos curves | 1083.23 / 1091.39 | 92 / 92 | 0 / 2359 |
| Nested logarithm | 110.28 / 114.38 | 0 / 0 | no verification |
| Positive tiny minima plus squared sin/cos | 441.35 / 431.56 | 25 / 25 | 11 / 420 |

The last scene uses `(x-0.1234)^2+1e-12`, `abs(x-0.1234)+1e-12`, `sin(x)^2`
and `cos(x)^2`. It demonstrates actual reuse in browser workloads, but only
2.62% of lookups. Small time differences are noise-scale, not evidence of a
general rendering speedup. The dense scene has **zero cache hits**, retains its
limited state and does not gain markers. Its 3889 actual evaluations are summed
across ten requests; each individual request still has its own 512 cap.

Full [30-row browser data](GRAPH_VERIFY_REUSE_BROWSER_2026_10_16.csv). Baseline
work counters are blank because that executable did not expose them. All 30
performance checks passed. Raw logs remain under ignored `build/graph-reuse-*`.

**Decision:** retain bounded reuse for repeated API candidates, common endpoints
and rounding retries, with truthful diagnostics. It is not the fix for cold dense
graphs. The next investigation should count refinement iterations and rejected
residuals at distinct coordinates, then compare a safeguarded bracket solver
against the current refinement under unchanged original-expression checks.
Cross-request/global caching and larger budgets are not justified by these data.

## Validation and limits

Five native groups passed with MSVC and GCC. Native regressions cover repeated
samples, separate expression/pair keys, request isolation, successful non-real
checks, canonical-coordinate aliases, exact rational cancellation, unchanged
results and atomic deadline failure. Existing allocation-failure checks remain.
Chromium verifies discrete curves, markers, retained sessions and four-function
camera cycles; Firefox/WebKit verify markers and the camera cycles as well.
Final results: 41 Chromium and 56 Firefox/WebKit checks passed; five native test
groups passed in each compiler. One initial test incorrectly compared an
unprocessed baseline row with a newly confirmed cached row; it now compares
all baseline confirmations and separately requires the additional rows.

Reuse helps repeated coordinates. It cannot eliminate work at genuinely distinct
coordinates, share values between separate HTTP pair requests, or prove that all
intersections were discovered. Limited marker sets remain possible. A larger
cache or a larger evaluation cap is not justified without workload evidence.
