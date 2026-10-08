# Performance checks

## Optional performance benchmarks

The expanded formatting/cache/HTTP scenarios, live-memory tracker, phase
definitions and three-run baseline recorder are documented in
[Benchmark methodology](../benchmarks/BENCHMARKS.md). Use a separate opt-in build;
production builds have no live tracker or HTTP timing trace.

The exact-reference BigInt arithmetic suite, dense sizes, alias/copy cases,
factorial/power experiments and isolated three-run recorder are described in
[BigInt measurements](../benchmarks/BIGINT_BENCHMARKS.md). Build the additional
`bigint_arithmetic_benchmark` target; use `run_baseline.js --bigint` to record it.

Direct BigDecimal arithmetic, normalization, division, higher functions and
constants are covered in [Decimal/math measurements](../benchmarks/DECIMAL_MATH_BENCHMARKS.md).
Build `decimal_math_benchmark`; use `run_baseline.js --decimal` for three isolated
runs, including explicit timeout and numerical-mismatch outcomes.

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_APPS=OFF -DNUMFORGE_ENABLE_SANITIZERS=OFF -DNUMFORGE_ENABLE_COVERAGE=OFF -DNUMFORGE_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --target bigint_multiply_benchmark decimal_format_benchmark calculator_benchmark --parallel 2
```

Run `build-bench/calculator_benchmark` (single-config) or
`build-bench/Release/calculator_benchmark.exe` (Visual Studio).
The executable prints CSV totals, separating parse, evaluate and format, with
allocation request counts and requested bytes for each phase. Tiny division,
power, factorial and constants run 1000 iterations each; multiplication uses
16, 64, 256 and 1024-digit operands with 100 iterations per size.
AST cleanup and initial result creation are outside the measured phases;
each iteration starts with a fresh AST and result. Allocation volume includes
the full size of realloc requests, not live/peak memory or RSS. Counters are
thread-local, saturate at `SIZE_MAX`, and are compiled only when benchmarks
are enabled. The expanded harnesses additionally offer explicit live/peak
payload tracking in separate memory samples; see BENCHMARKS.md for its
coverage and ownership boundaries. Use a separate benchmark build, not an
instrumented release package.

Both executables use monotonic elapsed time: QueryPerformanceCounter on Windows
and CLOCK_MONOTONIC on POSIX. Phase totals remain diagnostic, not isolated
arithmetic timings. The direct multiplication benchmark below avoids that issue.

### Direct BigInt multiplication

Run `build-bench/bigint_multiply_benchmark` (single-config), or on Windows/MSVC:

```powershell
.\build-bench\Release\bigint_multiply_benchmark.exe --quick
.\build-bench\Release\bigint_multiply_benchmark.exe > .\build-bench\multiply-baseline.csv
```

The default run sweeps 1, 2, 4, ... 1024 64-bit limbs (up to 65536 bits per
operand), with deterministic random, all-one, sparse and alternating patterns.
It covers balanced multiplication, `x*x` using identical input objects and a
separate destination, a full-width one-limb factor in both operand orders,
8:1/1:8 size ratios, and a small factor 10000 relevant to sequential factorials.
Fixtures are built directly using the private BigInt representation; no parser,
decimal conversion or fixture generation is included in the timed region.
The arithmetic itself is the real public `bigint_mul` path, without a calculator
deadline or cache. The destination object is reused; allocations/frees inside
the multiplication still count. Signs, zero/one shortcuts, output/input aliasing
and complete factorial algorithms are not covered by this performance sweep.

Each case warms up three calls, doubles batch size until at least 10 ms (or
262144 calls), then measures seven batches. CSV gives minimum/median/maximum
nanoseconds per operation and batch/sample counts. These are batch averages,
not individual-call latency percentiles. `--quick` uses 1/8/64 limbs, three
samples and a 2 ms target; it checks the harness, not algorithm crossover points.
Two modular product checks and representation checks run outside each timing
window, also consuming the output; they are not a proof of correctness.
One extra untimed call supplies allocation counts and requested bytes per call.
Allocation instrumentation is enabled in these builds, including timed calls;
compare identical instrumentation settings. It is not a peak-memory measurement.

Compiler/pointer-width metadata goes to stderr, CSV to stdout. Save the commit,
compiler flags, Release configuration, CPU/OS, power mode and command with each
baseline. Run on an idle machine, repeat whole runs, and compare matching rows;
large min/max spreads warrant another run. Before choosing a Karatsuba threshold,
add denser size sampling around the candidate crossover and validate full
application workloads. One run on one CPU cannot establish a universal threshold.
Benchmarks are opt-in, not installed and impose no timing threshold in CI.

### Decimal conversion and formatting

Run `decimal_format_benchmark` with the same optional `--quick` argument. The
default sweep uses deterministic inputs from 16 through 16384 decimal digits
and separately measures full `BigInt` conversion, scientific output with 10 or
100 places, full scientific output, and fixed output rounded to 10 places.
Fixture parsing is outside timing; allocation and result destruction inside the
conversion call remain included. As with multiplication, the CSV reports
calibrated batch averages and seven-sample min/median/max values plus one-call
allocation statistics. Use it to decide whether the next work should be prefix
scientific conversion, divide-and-conquer full conversion, rescaling, or buffer
reuse. Short scientific output now uses bounded leading-limb conversion with
an exact fallback at rounding boundaries. Compare its scaling against
`scientific_full`; see the [formatting follow-up](../benchmarks/BENCHMARKS.md#bounded-prefix-formatting-follow-up)
for the measured optimization and its limits.
