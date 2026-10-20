# Exact paired covariance — 20 October 2026

The [public statistics API](../reference/STATISTICS_API.md#paired-covariance)
computes exact population/sample covariance in C; Python delegates to that same
kernel. Anchored products avoid multiplying large common offsets. This is a new
capability, with no claim of an algorithmic speedup over a previous covariance
implementation. Existing summaries retain their contracts.

## Reproduction and method

Use the existing [statistics benchmark setup](STATISTICS_2026_10_19.md#reproduction-and-method).
The same `statistics_benchmark` executable now emits six additional `cov_*` rows
without changing its CSV columns. Each row performs three calls. Run the complete
executable three times to obtain nine checked calls per fixture.

Local measurements use Windows x64, GCC 16.2.0, C17 Release, warnings as errors,
with opt-in allocation instrumentation. Every call inherits a fresh 5000-ms,
128-MiB cumulative, 2-MiB single-allocation scope. Timing includes validation,
arithmetic and the final owned scalar copy. Inputs, initialized destination,
reference construction, result verification and destruction are outside timing.
Counters are read before verification. Requested bytes are allocation traffic,
not peak/live storage or process RSS.

Inputs are x_j=offset+j/d and y_j=x_(n-1-j), for j=0..n-1. The independent
closed-form references are population covariance `-(n*n-1)/(12*d*d)` and sample
covariance `-n*(n+1)/(12*d*d)`. Each scalar result is compared exactly outside
timing. The original six summary rows and their checks also run.

## Results

Ranges show the minimum/maximum of three invocation means, in milliseconds.
Allocation counts and requested bytes agree across all invocations.

| Fixture | Pairs | Offset | d | Mean ms range | Allocations | Requested bytes |
| --- | ---: | --- | --- | ---: | ---: | ---: |
| population singleton | 1 | 0 | 1 | 0.014967–0.027567 | 175 | 3,504 |
| population small | 16 | 0 | 1 | 0.223333–0.383933 | 2,600 | 47,976 |
| sample medium | 128 | 0 | 1 | 2.031833–2.995467 | 20,518 | 378,584 |
| population large | 1024 | 0 | 1 | 17.575733–19.262333 | 163,880 | 3,023,592 |
| sample offset1000 | 128 | 10^1000 | 1 | 2.355067–2.963633 | 20,773 | 843,928 |
| population tiny1000 | 128 | 0 | 10^1000 | 9.270233–10.548533 | 24,062 | 4,772,872 |

All 54 covariance calls and 54 summary calls completed within their scopes and
passed exact checks. These are local shared-machine measurements, not a server
capacity estimate, Python end-to-end benchmark, streaming throughput claim or
coverage of arbitrary denominator growth. The benchmark smoke test checks results
and its existing timeout, with no speed threshold.

## Correctness and integration

- All 74 project CTest checks pass across the full run and focused reruns. The
  first sandboxed run passed 72 checks; two server-start checks passed when rerun
  with permission to launch the local test server. No server code changed.
- Twelve statistics C cases pass in MSVC Release and GCC Release with warnings
  as errors. New cases check negative covariance, pairing, exact fractions,
  singleton/duplicate inputs, output aliases in either dataset, thousand-digit
  offsets and mixed enormous/tiny scales, NULL/count/overflow validation,
  allocation failure at every allocation (including the final copy), deterministic
  deadline and memory exhaustion, and recovery. Instrumented tests check complete
  tracking and zero live allocations after cleanup.
- Four new Python tests add 40 population/sample rational fixtures against the
  independent pairwise-product identity, symmetry, paired permutation,
  translation/scaling and equality with variance, plus lossless iterables,
  high precision, type validation, budget/serialization failures and recovery.
- The source archive builds an isolated installed wheel; all 68 Python tests and
  the standalone example pass. Documentation examples execute against that wheel.
- A numeric-only build excludes the engine and applications. Ten installed C/C++
  consumer checks and eleven installed SDK example checks pass, including the
  updated statistics example. No publication or remote CI run was performed.
