# Exact general-system verification — 19 October 2026

The [general-system API](../reference/LINEAR_SYSTEM_API.md) extends the initial
exact matrix module with rectangular rank and a complete particular/kernel
representation for a single RHS. Existing square solve and determinant use the
same forward elimination, and all solution construction shares back substitution.
No approximate backend, rank tolerance or performance optimization is introduced.

## Correctness and packaging

- Full MSVC 19.51 C17 Release regression: **66/66 CTests passed** after rebuilding
  the applications and library, including the old matrix contracts.
- The matrix and new linear-system suites (17 Unity cases total) also pass under
  GCC 16.2 Release with warnings as errors. The Python bridge passes strict GCC
  syntax/warning checks against the local Python headers, in addition to its
  working MSVC build.
- Nine new C cases cover skipped pivots, rectangular/zero rank, canonical free
  coordinates, exchanged rows, unique/infinite/inconsistent outcomes, kernels
  for inconsistent systems, aliasing/lifetime, 1000-digit cancellation, invalid
  queries and atomic allocation/deadline failure. Allocation sweeps include
  unique, nonunique, inconsistent and rank-zero construction paths.
- **36 Python tests passed** through an isolated source-archive/wheel installation.
  Seven new tests independently check rank by nonzero-minor expansion and
  consistency by augmented rank on 24 Fraction fixtures across six shapes.
  A*x=b, A*N=0, basis rank and canonical free identity rows are checked with
  Python Fraction arithmetic. Lifetime stress retains native matrix views after
  releasing inputs/analysis, and reuses them in native operations.
- A numeric-only build (`NUMFORGE_BUILD_ENGINE=OFF`, applications/testing off)
  includes the new API. Six installed C/C++ consumer checks and four SDK example
  checks pass; the general-system example displays (s,5-2*t,t).
- The Python checker builds a wheel from the extracted source archive, installs
  into an isolated target, asserts module origins and runs copied tests/example
  under `python -I`. It neither installs globally nor publishes artifacts.

Local execution is Windows x64, CPython 3.12.14. Remote Linux/macOS/Windows CI jobs
were not run here. Existing C/Python workflows automatically include the new
suite, source archive and expanded matrix benchmark target.

## Benchmark method

Use the [initial matrix benchmark commands](MATRIX_2026_10_19.md#reproduction-and-method)
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
