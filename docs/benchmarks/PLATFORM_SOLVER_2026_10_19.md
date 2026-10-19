# Public polynomial solver baseline — 19 October 2026

This is the first baseline for the standalone exact coefficient solver, not an
algorithm speedup comparison. The implementation was extracted from application
equations and now shares its rational-square-root proof with the calculator.

## Environment and method

Windows x64, GCC 16.2.0, C17 Release, warnings as errors, opt-in allocation
instrumentation. `NUMFORGE_BUILD_APPS=OFF`, `BUILD_TESTING=OFF`,
`NUMFORGE_BUILD_BENCHMARKS=ON`. Build target: `polynomial_benchmark`.

Three complete invocations, ten solves per case per invocation. Each solve gets
a fresh 5000-ms / 64-MiB cumulative / 512-KiB single-allocation budget. Timing
covers solving and numeric allocations; setup, descriptor verification and result
destruction are outside timing. Exact root-sum/product invariants are verified
after every result. Allocation counters are read before verification. Requested
bytes are cumulative allocation traffic, not live/peak heap usage or process RSS.

| Case: (a, b, c) | Run 1 mean ms | Run 2 mean ms | Run 3 mean ms | Allocation calls/solve | Requested bytes/solve |
| --- | ---: | ---: | ---: | ---: | ---: |
| Rational: (15, 16, -7) | 0.038250 | 0.036460 | 0.066600 | 388 | 7,072 |
| Irrational: (1, 0, -2) | 0.038670 | 0.028730 | 0.027600 | 342 | 6,512 |
| Complex: (9, 0, 1) | 0.030580 | 0.030350 | 0.029650 | 368 | 6,872 |
| Large coefficients: (1, 10^1000, 1) | 0.319910 | 0.292740 | 0.306780 | 413 | 60,504 |

All 120 solves completed and passed exact verification. Small timing differences
include machine scheduling/measurement noise; these data establish a local
baseline rather than a universal performance guarantee. No peak-memory
measurement or before/after acceleration claim is made.

## Correctness and boundary evidence

- Full MSVC Release CTest regression: 64/64 suites passed, including existing
  equation/HTTP, numerical roots, high-precision math and failure injection.
- Public engine/parameter/polynomial tests also passed under GCC Release.
- `polynomial_tests` covers exact root metadata, multiplicity, irrational and
  complex roots, linear/constant reduction, 1000-digit coefficients, aliases,
  atomic allocation failures and deterministic cancellation.
- The numeric-only build with `NUMFORGE_BUILD_ENGINE=OFF` compiles the solver
  without a parser or application target; installed C/C++ consumers exercise it.

The public [polynomial API](../reference/POLYNOMIAL_API.md) describes exactness,
ownership, runtime scopes and supported interfaces. The independent high-precision
sqrt reference and parameterized evaluation tests remain separate from this exact
solver timing corpus.
