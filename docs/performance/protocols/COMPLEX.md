# Complex performance diagnostics

The independent oracle driver also measures each tested public BigComplex call
with the monotonic platform clock. Input parsing and output conversion are
outside the measured interval. Every timed result must pass the same independent
component error bound as the ordinary test; incorrect results fail the run.

Build `complex_oracle_driver` in an ordinary Release testing build, without
sanitizers, coverage or allocation instrumentation, then run on a quiet machine:

```powershell
node tests/test_complex_oracle.js build/remote-tests/Release/complex_oracle_driver.exe --benchmark
```

The command repeats the 1284-case corpus in five fresh processes, emitting
minimum, median and maximum milliseconds for each identical input, precision
and rounding mode. Cases are not pooled across inputs. There is no CI timing
threshold. The command has a 120-second timeout per process and an 8 MiB output
bound. CTest executes one pass without performance assertions.

Add `--binary` to run only the 156 div/log/pow cases. Binary CSV rows include
the second operand's real and imaginary components, so timings can be compared
only for matching complete inputs.

Keep raw output in the ignored build directory. Record the commit and pending
patch, CMake cache, compiler and machine information alongside it when comparing
changes. These are operation timings, not application latency or memory
benchmarks. Check input conditioning and precision rules in the complex API
reference before interpreting a timing difference.

## Allocation and live-memory probes

Configure a separate Release testing build with `NUMFORGE_BUILD_BENCHMARKS=ON`
and build `complex_oracle_driver` and `allocation_stats_check`. The normal
driver rejects memory mode when this instrumentation is absent.

```powershell
.\build\factorial-bench\Release\allocation_stats_check.exe
node tests/test_complex_oracle.js build/factorial-bench/Release/complex_oracle_driver.exe --memory
ctest --test-dir build/factorial-bench -C Release -R complex_memory_smoke --output-on-failure
```

The full probe validates 584 half-even cases across all four precisions. The
smoke test uses the 146 cases at 34 digits. Each case creates fresh owned objects,
resets counters after input parsing, captures counters immediately after the
public C operation, and verifies zero tracked live bytes after destroying all
objects and formatting buffers. Reference comparison remains mandatory.
CSV reports allocation calls, cumulative requested bytes, baseline/end live
bytes, peak live bytes and additional peak above the baseline. Baseline includes
the owned inputs and empty result/component containers. Output conversion is
outside the captured operation counters. Tracking and allocator overhead, RSS,
input parsing and output formatting are not measured operation memory.

Run timing and memory separately: `--memory --benchmark` is rejected because
instrumentation distorts timing. `--operation=tan,tanh` selects matching cases
for a focused before/after run; `--memory --quick` selects 34 digits.

## Focused comparisons

`--stability` selects extreme signed-exponent, magnitude, argument, root and
logarithm cases. Scalar timings exclude boxing/formatting. Full independent
references remain required; a fast mismatched result fails the run.

Selected [shared scalar comparisons](../baselines/ARITHMETIC.md#shared-complex-scalar-work)
record complete calculator improvements. Calculator output places and direct API
significant digits have different scopes. Initial unary/binary timing tables and
intermediate memory diaries remain in Git history; compare current measurements
only with matching inputs, settings and instrumentation.
