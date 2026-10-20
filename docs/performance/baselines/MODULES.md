# Exact module baselines

Historical Windows x64 / GCC 16.2 / C17 Release measurements, 19–20 October 2026,
with warnings as errors and opt-in allocation instrumentation. No speedup over a
previous implementation is claimed. Each subsection retains its independent
input/reference protocol. Requested bytes mean cumulative allocation traffic,
not peak live memory or RSS. Verification and input setup are outside timing.

Configure with `NUMFORGE_BUILD_BENCHMARKS=ON`, `NUMFORGE_BUILD_APPS=OFF` and
`BUILD_TESTING=OFF`; build the named executable. Visual Studio executables use
the configuration subdirectory and `.exe`. Results are checked after timing;
smoke tests check correctness and completion rather than a speed threshold.

## Polynomials

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

## Matrices

Windows x64, GCC 16.2.0, C17 Release, warnings as errors, allocation instrumentation.
Use a separate uninstrumented build for ordinary application use.

```sh
cmake -S . -B build/matrix-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_APPS=OFF -DNUMFORGE_BUILD_BENCHMARKS=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/matrix-bench --config Release --target matrix_benchmark
```

Run `build/matrix-bench/matrix_benchmark` (on Visual Studio,
`build/matrix-bench/Release/matrix_benchmark.exe`). Three complete local invocations
were recorded, each with three repetitions per operation/shape. Each operation
starts a fresh 5000-ms / 128-MiB cumulative / 2-MiB single-allocation scope.
Timing includes output creation and numeric work, but excludes input/reference
construction, verification and output destruction. Allocation counters are read
before verification: calls and requested bytes describe cumulative traffic, not
peak/live memory or process RSS. These are short local timings on a shared machine.

Fixtures use A=(I+J)/d, where J is all ones and d=10^p+1. The exact scalar
references are det(A)=(n+1)/d^n, A*ones=(n+1)/d and
A*A=((n+2)J+I)/d^2. Every result/entry is compared as a reduced rational after
timing; no matrix operation is used to construct these reference values.

### Results

Ranges are minimum/maximum of the three invocation means (ms per operation).
Allocation counts/bytes were identical across invocations.

| n | p | Operation | Mean ms range | Allocations | Requested bytes |
| ---: | ---: | --- | ---: | ---: | ---: |
| 2 | 1 | multiply | 0.045233–0.049333 | 502 | 8,848 |
| 2 | 1 | determinant | 0.021600–0.026767 | 249 | 4,528 |
| 2 | 1 | solve | 0.031900–0.057033 | 394 | 7,168 |
| 8 | 1 | multiply | 3.040833–3.769867 | 30,982 | 542,320 |
| 8 | 1 | determinant | 0.927533–1.652533 | 10,637 | 184,312 |
| 8 | 1 | solve | 1.204533–1.625267 | 14,163 | 245,680 |
| 16 | 1 | multiply | 22.669133–24.559867 | 247,015 | 4,317,048 |
| 16 | 1 | determinant | 8.067100–8.646967 | 84,220 | 1,446,800 |
| 16 | 1 | solve | 9.309733–11.262567 | 99,143 | 1,704,304 |
| 2 | 1000 | multiply | 0.289000–0.415467 | 502 | 90,928 |
| 2 | 1000 | determinant | 0.062167–0.090333 | 259 | 26,648 |
| 2 | 1000 | solve | 0.061800–0.121233 | 412 | 39,632 |

All 108 timed operations passed exact verification within their scopes. The
1000-exponent denominators have 1001 digits. These structured fixtures exercise
dimensions and large rational inputs; they do not characterize arbitrary
denominator growth, sparse matrices, singular-case throughput or ill-conditioned
approximate solvers. The separate correctness suite tests row exchanges,
singularity, unrelated rational entries and large cancellation.

The allocation volume supports investigating reusable scalar workspaces and
fraction-free elimination later. Neither change is included here, and no
performance threshold is enforced in CI. The opt-in `matrix_benchmark_smoke`
checks completion and exact results; its CTest timeout is 120 seconds.

## General linear systems

Use the [initial matrix benchmark commands](MODULES.md#matrices)
to build/run `matrix_benchmark`. The expanded harness still verifies all original
product/determinant/square-solve fixtures, then adds rank and solve_general to
each. Four full-rank fixtures use A=(I+J)/(10^p+1), with exact rank=n and known
ones solution. Additional 2-by-3 fixtures use A=[[0,2,4],[0,3,6]], b=[10,15] or
[10,16], exact rank=1 and the known canonical basis [[1,0],[0,-2],[0,1]].

Three complete invocations, three repetitions per operation/fixture, GCC 16.2
C17 Release on the same local Windows machine. Each call has a fresh 5000-ms /
128-MiB cumulative / 2-MiB single-allocation scope. Timed work includes input
copies, elimination, classification and complete output construction. Input/
reference setup, exact verification and destruction are outside timing. Counters
are read before verification; requested bytes are allocation traffic, not live/
peak memory or RSS. Short shared-machine timing is informational, not a CI speed
threshold or an optimization comparison.

| Fixture | Operation | Mean ms range across 3 invocations | Allocations | Requested bytes |
| --- | --- | ---: | ---: | ---: |
| n=2, p=1 | rank | 0.014100–0.015233 | 168 | 3,112 |
| n=2, p=1 | solve_general | 0.032567–0.040833 | 400 | 7,328 |
| n=8, p=1 | rank | 1.018167–1.773833 | 10,358 | 179,584 |
| n=8, p=1 | solve_general | 1.565800–2.270167 | 14,169 | 245,888 |
| n=16, p=1 | rank | 9.348500–11.138233 | 83,678 | 1,437,664 |
| n=16, p=1 | solve_general | 10.856267–14.183500 | 99,149 | 1,704,576 |
| n=2, p=1000 | rank | 0.026167–0.048900 | 177 | 16,920 |
| n=2, p=1000 | solve_general | 0.060733–0.119400 | 418 | 39,792 |
| 2x3 consistent | rank | 0.017333–0.034767 | 193 | 3,736 |
| 2x3 consistent | solve_general | 0.050400–0.069467 | 582 | 11,264 |
| 2x3 inconsistent | rank | 0.016533–0.022600 | 193 | 3,736 |
| 2x3 inconsistent | solve_general | 0.040567–0.056133 | 484 | 9,336 |

All **216 timed operations** completed within their scopes and passed exact
verification, including 108 rank/general calls. Allocation counters match across
invocations. These are structured baselines, not measurements of arbitrary
rational growth or large sparse/approximate systems. The benchmark smoke test
retains its 120-second timeout and checks correctness rather than speed.

Further work can investigate reusable factorizations, fraction-free elimination
or approximate domain contracts after defining the use case. General multiple-RHS
classification and numerical least squares remain separate capabilities.

## Statistics

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

### Results

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

## Covariance

Use the existing [statistics benchmark setup](MODULES.md#statistics).
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

### Results

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

## Linear regression

The existing `statistics_benchmark` adds five `reg_*` rows without changing CSV
columns or the twelve summary/covariance fixtures. Inputs are x_j=offset+j/d,
y_j=x_(n-1-j), for j=0..n-1. Independent closed-form references are slope=-1 and
intercept=2*offset+(n-1)/d, constructed outside timing without a regression or
covariance call. Both coefficients and the reported count are checked after every
call. Each row runs three repetitions in the same fresh scopes as covariance.

Timing includes validation, the reused exact summaries/covariance, coefficient
construction and internal cleanup. Input/reference creation and returned-fit
destruction are outside timing. Allocation counters are read before validation;
requested bytes are cumulative traffic. This measures the current composed
implementation, not a specialized new moments algorithm or a speedup.

### Local baseline — 20 October 2026

Source `b6a02e4`, Windows x64, GCC 16.2.0, C17 Release/Ninja with opt-in allocation
instrumentation. Three isolated executable invocations give nine checked calls
per fixture, with no concurrent build/test job. Each call uses the covariance
scope: 5000 ms, 128 MiB cumulative bytes and 2 MiB single allocation.

| Fixture | Pairs | Mean ms range | Allocations | Requested bytes |
| --- | ---: | ---: | ---: | ---: |
| two pairs | 2 | 0.131–0.201 | 1,479 | 27,200 |
| ordinary | 128 | 5.138–5.708 | 55,529 | 1,022,064 |
| ordinary large | 1024 | 40.471–43.737 | 439,917 | 8,096,912 |
| offset 10^1000 | 128 | 5.798–6.800 | 56,173 | 2,513,360 |
| spread 10^-1000 | 128 | 23.458–26.384 | 66,308 | 13,833,568 |

All 153 calls passed exact checks, including 45 regression fits. Allocation
counts/bytes agreed across invocations. These are short shared-machine baselines,
not Python/HTTP timings, arbitrary denominator-growth coverage or speed claims.
