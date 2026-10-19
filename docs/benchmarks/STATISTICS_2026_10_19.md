# Exact descriptive statistics baseline — 19 October 2026

The first [rational dataset summary](../reference/STATISTICS_API.md) computes
count, sum, extrema, arithmetic mean and population/sample variance in the numeric
library. All fields remain exact fractions, with undefined singleton sample
variance represented by NULL/None. Existing decimal statistics are unchanged.

## Verification

- Windows x64, MSVC 19.51, C17 Release: **67/67 CTests passed** after rebuilding
  the library and applications. Existing expression, matrix, equation, decimal
  statistics, HTTP and failure suites remain compatible.
- Nine new statistics cases also pass under GCC 16.2 Release with warnings as
  errors: signed fractions, known population/sample values, recurring moments,
  singletons, repeated/borrowed handles, owned lifetime, 1000-digit offsets/tiny
  spreads, validation, allocation failure at each allocation and deterministic
  budget exhaustion. Failure preserves the destination and input values.
- **43 Python tests passed** through the source-archive/wheel installation check.
  Seven new statistics tests include 25 Fraction fixtures checked with the
  independent pairwise variance identity, plus translation/scaling/permutation
  properties, lossless inputs, lifetime, 1000-digit cases and resource failure
  recovery through native field serialization.
- A numeric-only build with engine/applications/testing disabled includes the
  module. Eight installed C/C++ consumer checks and five SDK example checks pass.
  The statistics example reports mean=1/3, population variance=2/9 and sample
  variance=1/3 for [0,0,1].
- CPython 3.12.14 source archive extraction, wheel build, isolated target install,
  module-origin assertions and standalone example pass. No global installation
  or publication is performed. The native bridge also passes strict GCC syntax/
  warning checks against local Python headers, alongside its working MSVC build.

Local runtime verification is Windows x64. Existing C/Python CI workflows include
the new suite and archive tests; the benchmark job now builds/runs
statistics_benchmark_smoke on its platform matrix. Remote jobs were not run here.

## Reproduction and method

```sh
cmake -S . -B build/statistics-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_APPS=OFF -DNUMFORGE_BUILD_BENCHMARKS=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/statistics-bench --config Release --target statistics_benchmark
```

Run `build/statistics-bench/statistics_benchmark` (Visual Studio:
`build/statistics-bench/Release/statistics_benchmark.exe`). Local measurements use
GCC 16.2.0 C17 Release with opt-in allocation instrumentation. Each summary has a
fresh 5000-ms / 128-MiB cumulative / 2-MiB single-allocation scope. Three complete
invocations each perform three summaries per fixture. Timing includes validation,
arithmetic and all output field construction. Input/reference construction,
verification and destruction lie outside timing. Counters are read before
verification. Requested bytes are allocation traffic, not peak/live memory or RSS.

Fixtures are x_j=offset+j/d for j=0..n-1, d=10^p. References use exact closed
forms outside timing: mean=offset+(n-1)/(2*d), population variance=(n*n-1)/(12*d*d),
sample variance=n*(n+1)/(12*d*d) for n>1. Sum and extrema are checked too, as are
the count and unavailable singleton sample field. Reference values are built
with scalar arithmetic without calling the summary implementation.

## Results

Ranges are minimum/maximum of three invocation means (ms per complete summary).
Allocation counts/bytes match across invocations.

| Fixture | Count | Offset | d | Mean ms range | Allocations | Requested bytes |
| --- | ---: | --- | --- | ---: | ---: | ---: |
| singleton | 1 | 0 | 1 | 0.023000–0.036767 | 254 | 5,144 |
| small | 16 | 0 | 1 | 0.220733–0.283933 | 2,366 | 43,680 |
| medium | 128 | 0 | 1 | 1.442833–1.698700 | 17,260 | 318,736 |
| large | 1024 | 0 | 1 | 12.302033–13.929333 | 136,430 | 2,519,328 |
| offset1000 | 128 | 10^1000 | 1 | 1.924933–2.125633 | 17,645 | 831,112 |
| tiny1000 | 128 | 0 | 10^1000 | 7.330600–7.475800 | 20,417 | 4,237,176 |

All **54 summaries** completed within their scopes and passed exact field checks.
These short shared-machine timings establish a local baseline; they do not claim
an algorithmic speedup, Python end-to-end timing, arbitrary denominator-growth
coverage or streaming throughput. The smoke check enforces exact results and a
120-second CTest timeout rather than a speed threshold. Ordinary builds omit the
instrumentation, and using the C library does not require Python or the engine.
