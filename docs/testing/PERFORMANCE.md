# Performance checks

Use the [benchmark index](../../benchmarks/README.md) to select a tool and the
[performance evidence](../performance/README.md) for dated comparisons. Protocols
own fixture, calibration and memory details; this page owns the common procedure.

## Build and run

Use a separate Release build, with sanitizers and coverage disabled:

```sh
cmake -S . -B build/bench -DCMAKE_BUILD_TYPE=Release -DNUMFORGE_BUILD_BENCHMARKS=ON -DBUILD_TESTING=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/bench --config Release --parallel 2
ctest --test-dir build/bench -C Release --output-on-failure
node benchmarks/tools/run_baseline.js build/bench build/baseline --quick --bigint
```

Visual Studio binaries are in `build/bench/Release`; pass that directory to
recorders. Single-configuration builds use `build/bench`. Do not record speed
while builds, test suites or another benchmark are running. Three complete serial
runs are preferable to a single favorable row; quick mode checks the harness.

## Interpret the result

- Record source revision/patch, exact fixtures, compiler/flags, OS/CPU and power
  conditions. Compare matching inputs, precision, output settings and instrumentation.
- Validate complete results independently outside timing. Keep numerical mismatches,
  resource stops and timeouts visible; none is a successful operation timing.
- Batch averages, fresh-request latency, parse/evaluate/format phases, loopback HTTP
  and browser settling measure different work. Do not combine their numbers.
- Requested bytes count cumulative allocation traffic. Live/peak payload probes
  exclude allocator overhead and are separate from process RSS/working set.
- Instrumentation changes cost. Keep live-memory/profile passes separate from
  speed timings; ordinary builds omit optional hooks.
- Check statistical spread and repeat noisy runs before selecting an algorithm.
  One host does not establish a portable crossover, concurrent capacity or an FPS guarantee.

Timers use monotonic platform clocks. Default benchmark smoke checks enforce
independent results, bounded work and completion timeouts; they do not enforce
speed thresholds. Expensive stress modes require an explicitly suitable budget.

## Graph and interaction diagnostics

Install [browser dependencies](WEB_AND_CI.md), then use built server paths:

```sh
node benchmarks/application/benchmark_graph_audit.cjs --server build/bench/numforge_web --repeats 3 --output build/graph-audit.json --csv build/graph-audit.csv
node benchmarks/application/benchmark_graph_multi.cjs --before build/before/numforge_web --after build/bench/numforge_web --repeats 3 --output build/graph-multi.json
node benchmarks/application/benchmark_graph_stress.cjs --server build/bench/numforge_web --cycles 16 --output build/graph-stress.json
```

Use `Release/` and `.exe` on Windows as applicable. These tools own their temporary
servers. HTTP graph checks use independent reference coordinates/anchors; finite
grids do not prove that every narrow feature is present. Zero chord error with no
connected chords also means zero coverage. Browser timings include scheduling and
debounce; physical-phone touch behavior requires separate review.

Selected [graph corpus results](../performance/baselines/GRAPHS.md) and
[marker comparisons](../performance/baselines/GRAPH_REFINEMENT.md) retain the useful
results and limits. Save new routine recordings under ignored `build/`.
