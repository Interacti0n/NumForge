# Exact module baselines

Historical Windows x64/GCC 16.2/C17 Release, 19–20 October 2026, warnings as errors
and allocation instrumentation. These are shared-machine baselines, not speedups,
Python/HTTP timings or portable capacity claims.

```sh
cmake -S . -B build/module-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_APPS=OFF -DNUMFORGE_BUILD_BENCHMARKS=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/module-bench --config Release --target polynomial_benchmark matrix_benchmark statistics_benchmark
```

Run each named executable (Visual Studio: `Release/` and `.exe`). Except for the
polynomial scope below, each call uses 5000 ms / 128 MiB cumulative / 2 MiB per
allocation. Three complete invocations × three calls/fixture; ranges are min/max
invocation means in ms. Allocation counts/bytes matched across invocations and
were read before verification. Bytes count cumulative traffic, not live/peak/RSS.
Input/reference setup, independent checking and returned-result destruction are
outside timing. Result tables retain their original values.

Smoke tests validate correctness/completion with 120-second timeouts, no speed
threshold. Ordinary builds omit instrumentation. Wider growth/streaming domains
need separate measurements; [method](../../testing/PERFORMANCE.md).

## Polynomials

`polynomial_benchmark`: three invocations × ten solves/fixture, fresh
5000 ms / 64 MiB cumulative / 512 KiB single-allocation scopes. Time includes
solve/output construction; setup, exact descriptor checks and destruction are
excluded. Root sums/products are checked independently. All 120 solves passed.

| Case: (a, b, c) | Run 1 mean ms | Run 2 mean ms | Run 3 mean ms | Allocation calls/solve | Requested bytes/solve |
| --- | ---: | ---: | ---: | ---: | ---: |
| Rational: (15, 16, -7) | 0.038250 | 0.036460 | 0.066600 | 388 | 7,072 |
| Irrational: (1, 0, -2) | 0.038670 | 0.028730 | 0.027600 | 342 | 6,512 |
| Complex: (9, 0, 1) | 0.030580 | 0.030350 | 0.029650 | 368 | 6,872 |
| Large coefficients: (1, 10^1000, 1) | 0.319910 | 0.292740 | 0.306780 | 413 | 60,504 |

## Matrices

`matrix_benchmark`: three invocations × three repetitions/shape/operation.
Fixtures `A=(I+J)/d`, `d=10^p+1`; independent references are
`det(A)=(n+1)/d^n`, `A*ones=(n+1)/d`, `A*A=((n+2)J+I)/d^2`.
All 108 calls passed reduced-rational checks. p=1000 means 1001-digit
denominators. Structured dense fixtures do not cover arbitrary denominator
growth, sparse systems or approximate conditioning.

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

## General linear systems

The expanded matrix harness adds rank/general solve to the original fixtures.
Full-rank references use rank=n and an all-ones solution. Rank-one fixtures use
`A=[[0,2,4],[0,3,6]]`, b=[10,15] or [10,16], with canonical nullspace
`[[1,0],[0,-2],[0,1]]`. All 216 calls passed, including 108 rank/general calls.
Time includes input copies, elimination, classification and complete output.
This does not measure general multiple-RHS or numerical least squares.

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

## Statistics

`statistics_benchmark`: `x_j=offset+j/d`, j=0..n−1, d=10^p.
Independent closed forms: mean=`offset+(n-1)/(2d)`, population variance=
`(n*n-1)/(12*d*d)`, sample variance=`n*(n+1)/(12*d*d)`. Count/sum/extrema and
unavailable singleton sample variance are checked too. All 54 summaries passed.
Time includes validation, arithmetic and complete output fields.

| Fixture | Count | Offset | d | Mean ms range | Allocations | Requested bytes |
| --- | ---: | --- | --- | ---: | ---: | ---: |
| singleton | 1 | 0 | 1 | 0.023000–0.036767 | 254 | 5,144 |
| small | 16 | 0 | 1 | 0.220733–0.283933 | 2,366 | 43,680 |
| medium | 128 | 0 | 1 | 1.442833–1.698700 | 17,260 | 318,736 |
| large | 1024 | 0 | 1 | 12.302033–13.929333 | 136,430 | 2,519,328 |
| offset1000 | 128 | 10^1000 | 1 | 1.924933–2.125633 | 17,645 | 831,112 |
| tiny1000 | 128 | 0 | 10^1000 | 7.330600–7.475800 | 20,417 | 4,237,176 |

## Covariance

The statistics harness adds six `cov_*` fixtures, each with three repetitions
per invocation. `x_j=offset+j/d`, `y_j=x_(n-1-j)`; independent population/sample
references are `-(n*n-1)/(12*d*d)` and `-n*(n+1)/(12*d*d)`.
All 54 covariance and 54 summary calls passed. Timing includes validation,
arithmetic and the owned scalar copy; initialized destination is outside timing.

| Fixture | Pairs | Offset | d | Mean ms range | Allocations | Requested bytes |
| --- | ---: | --- | --- | ---: | ---: | ---: |
| population singleton | 1 | 0 | 1 | 0.014967–0.027567 | 175 | 3,504 |
| population small | 16 | 0 | 1 | 0.223333–0.383933 | 2,600 | 47,976 |
| sample medium | 128 | 0 | 1 | 2.031833–2.995467 | 20,518 | 378,584 |
| population large | 1024 | 0 | 1 | 17.575733–19.262333 | 163,880 | 3,023,592 |
| sample offset1000 | 128 | 10^1000 | 1 | 2.355067–2.963633 | 20,773 | 843,928 |
| population tiny1000 | 128 | 0 | 10^1000 | 9.270233–10.548533 | 24,062 | 4,772,872 |

## Linear regression

### Local baseline — 20 October 2026

Source `b6a02e4`, Windows x64/GCC 16.2/C17 Release/Ninja, no concurrent job.
Five `reg_*` fixtures extend the unchanged summary/covariance rows. With the same
reversed pairs, independent slope=−1 and intercept=`2*offset+(n-1)/d`.
Time includes validation, reused summaries/covariance, fit construction and
internal cleanup. All 153 calls passed, including 45 regression fits.
This measures the composed implementation, not a specialized moments algorithm.

| Fixture | Pairs | Mean ms range | Allocations | Requested bytes |
| --- | ---: | ---: | ---: | ---: |
| two pairs | 2 | 0.131–0.201 | 1,479 | 27,200 |
| ordinary | 128 | 5.138–5.708 | 55,529 | 1,022,064 |
| ordinary large | 1024 | 40.471–43.737 | 439,917 | 8,096,912 |
| offset 10^1000 | 128 | 5.798–6.800 | 56,173 | 2,513,360 |
| spread 10^-1000 | 128 | 23.458–26.384 | 66,308 | 13,833,568 |
