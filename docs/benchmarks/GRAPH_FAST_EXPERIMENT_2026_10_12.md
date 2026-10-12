# Experimental C double graph evaluator, 2026-10-12

This records the initial unconnected experiment. The subsequent
[screen-space rollout](GRAPH_SCREEN_ROLLOUT_2026_10_13.md) integrates it into the
web through an explicit approximate mode and precision fallback.

The internal `ApplicationFastExpression` compiles the existing prepared parser
tree into owned stack bytecode. Constants and referenced session values are
copied once; evaluation never borrows the AST or mutates the session. There is
no alternate parser and no JavaScript arithmetic path. The experiment is not
connected to the graph HTTP adapter: production graphs still use the existing
BigDecimal/BigRational sampler and its 2048-point contract.

Supported operations are unary signs, square/cube postfixes, +, -, *, /, powers,
sin/cos/tan, exp, ln, sqrt, cbrt and abs. Calls use the existing function registry,
including its aliases and angle-unit settings. Unsupported calls, factorials,
imaginary literals and complex/quantity snapshots reject compilation so a
future adapter can retain the exact path. Requested arbitrary precision is not
implemented by this double evaluator.

## Measurements and references

Run the opt-in `graph_fast_benchmark [1..10]`. The
[CSV](graph_fast_experiment_2026_10_12.csv) contains 12 cases, three repetitions.
Measurements use GCC Release with allocation statistics and without live-memory
tracking. The exact baseline is a complete 129-point uniform C sample, including
AST preparation and exact coordinate creation. Fast compilation includes normal
AST preparation plus bytecode creation; fast evaluation timings exclude compilation.
These are point-evaluation measurements, not adaptive sampling/HTTP/render times.

| Expression | Exact 129 points, median ms | Fast 8192 points, median ms | Compile, median ms |
| --- | ---: | ---: | ---: |
| `sin(x)` | 14.59 | 0.49 | 0.046 |
| `sin(33x)` | 16.36 | 0.63 | 0.056 |
| `sin(3x)+cos(2x)` | 31.01 | 1.13 | 0.021 |
| `sin(3x)*exp(-x*x/20)` | 48.28 | 2.11 | 0.115 |
| `tan(x)` | 29.29 | 0.91 | 0.090 |
| `ln(x)` | 27.54 | 0.41 | 0.100 |

Fast results on the same 129 projected coordinates are checked against the
BigDecimal/BigRational evaluator. Maximum observed relative error among accepted
real points is below 2e-16. The 8192-point batch is additionally compared with
libm expressions; this checks expression composition but is not an independent
transcendental oracle because the fast evaluator itself uses libm. Half of the
sqrt/ln dense samples are non-real; the table does not imply they all produce
real values. The dense grid has an even point count and does not include zero.

Each 8192-point batch uses one atomic-output scratch allocation, 131072 bytes,
and no per-point heap allocations. Caller-owned input/output each use 65536 /
131072 bytes respectively, outside that scratch counter. Compilation currently
reserves 4096 instructions rather than an optimized compact instruction count.

## Conservative fallback and safety

Samples report real, non-real, or uncertain. Heuristic error propagation marks
division near an uncertain zero, tangent near a pole, large trig arguments,
non-finite/underflowed intermediates and severe cancellation uncertain. Trig
arguments beyond 1e6 radians request fallback; this is a conservative experiment
bound, not a claim that all libm values outside it are inaccurate. Composed
complex intermediates also request fallback because later operations can return
real values, e.g. `abs(sqrt(-1))`.

All 8192 samples in the +/-1e12 sine and `(1e16+x)-1e16` cases are uncertain;
their zero error columns must not be read as successful approximate evaluation.
No connecting edges are produced or implied by this evaluator. Pole/gap decisions
still require a sampler. The estimates are not certified interval arithmetic or
a general guarantee against aliasing, missed discontinuities or ill conditioning.

Compilation and each batch inherit the caller's budget, or start the configured
operation budget. Invalid coordinates/caps and allocation/deadline failures leave
the whole output untouched. MSVC and GCC tests check values, degree mode,
domains/poles, cancellation, unsupported/underflowed literals, independent
snapshot ownership, every observed compilation allocation failure and batch
allocation/shared-budget failures. GCC tracked allocation cleanup is zero-live.

## Next integration

Build a separate compact adaptive sampler using screen-space chord error and
bounded phase/domain probes. Keep coarse segments visible while refining the
current camera, and cancel obsolete work. Connect this through an explicitly
approximate graph API mode with an exact fallback for unsupported/uncertain
expressions; do not silently change calculator precision or report uncertain
values as zero. Measure complete HTTP payload, browser frame time and cache
memory before making this the default graph path.
