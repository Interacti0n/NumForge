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

The command repeats the 900-case corpus in five fresh processes, emitting
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
benchmarks. Decimal allocation, resource limits and adaptive precision remain
separate follow-up work.

## Allocation and live-memory probes

Configure a separate Release testing build with `NUMFORGE_BUILD_BENCHMARKS=ON`
and build `complex_oracle_driver` and `allocation_stats_check`. The normal
driver rejects memory mode when this instrumentation is absent.

```powershell
.\build\factorial-bench\Release\allocation_stats_check.exe
node tests/test_complex_oracle.js build/factorial-bench/Release/complex_oracle_driver.exe --memory
ctest --test-dir build/factorial-bench -C Release -R complex_memory_smoke --output-on-failure
```

The full probe validates 520 half-even cases across all four precisions. The
smoke test uses the 130 cases at 34 digits. Each case creates fresh owned objects,
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

On the same Windows/MSVC machine, all 520 before and after probes completed with
zero tracked allocations left after each case. The largest observed live peak
was 12,838 bytes (acosh at 250 digits). The largest cumulative request before
optimization was 13,455,105 bytes for tan(1+i) at 250 digits, despite a live peak
of only 6,200 bytes. This identified repeated scalar evaluations as a useful
first target; it does not establish bounds for arbitrary inputs or precisions.

The tangent quotient and the small-real-part tanh quotient now share the guarded
scalar evaluations used to form both numerator and denominator. Final component
rounding and division are unchanged. Exact request counts from matching probes:

| Operation/input | Digits | Calls before → after | Requested bytes before → after | Live peak before → after |
| --- | ---: | ---: | ---: | ---: |
| tan(1+i) | 34 | 53,983 → 28,116 | 2,078,416 → 1,068,315 | 3,784 → 3,768 |
| tan(1+i) | 250 | 157,989 → 87,267 | 13,455,105 → 7,550,462 | 6,200 → 5,848 |
| tanh(-0.3+0.7i) | 34 | 45,914 → 24,187 | 1,576,769 → 820,685 | 3,896 → 3,880 |
| tanh(-0.3+0.7i) | 250 | 139,411 → 77,694 | 10,454,209 → 6,009,246 | 6,312 → 5,960 |

Raw memory outputs are `build/complex-memory-baseline.txt` and
`build/complex-memory-after.txt`; source snapshots, compiler/CPU metadata and
CMake cache use matching before/after prefixes. This reduces allocation churn
and consumption of cumulative request budgets; the live-memory improvement is
much smaller. Numerical conditioning, poles, exact intermediate growth and
certified rounding remain separate concerns.

Five uninstrumented Release processes per version measured the same 80 tan/tanh
cases, with mandatory reference validation and matching case order. Selected
half-even median milliseconds were:

| Operation/input | Digits | Before | After |
| --- | ---: | ---: | ---: |
| tan(1+i) | 34 | 21.133 | 11.278 |
| tan(1+i) | 250 | 240.883 | 141.571 |
| tanh(-0.3+0.7i) | 34 | 16.062 | 8.035 |
| tanh(-0.3+0.7i) | 250 | 176.019 | 107.757 |

Raw per-case timing outputs are `build/complex-tangent-before.txt` and
`build/complex-tangent-after.txt`. The compiler and machine match the local
baselines below; no builds/tests ran concurrently with the timing commands.
An idle web server remained running and CPU clock frequency was not fixed.
The improvement applies to the shared quotient paths; scaled and real-axis
tanh paths keep their existing algorithms. These timings are observations on
this machine, not portable speed guarantees.

## Local baseline, 9 October 2026

Windows, AMD Ryzen 7 7435HS, MSVC 19.51.36247.0, Release, warnings as errors,
allocation instrumentation off. Five sequential processes validated all 744
cases each. An idle local web server was running; this is a development-machine
baseline, not an isolated hardware comparison. No numeric algorithm changed.
For z=1+i and half-even rounding, median milliseconds across five identical
calls were:

| Function | 34 digits | 250 digits |
| --- | ---: | ---: |
| sqrt | 0.579 | 2.232 |
| ln | 3.740 | 62.416 |
| sin | 8.328 | 94.433 |
| tan | 25.222 | 242.880 |
| asinh | 9.961 | 74.797 |
| acosh | 11.453 | 111.633 |
| atanh | 10.284 | 59.310 |

Raw per-case results are in `build/complex-baseline-cases.txt`; environment and
source hashes are in `build/complex-baseline-metadata.json`, alongside the saved
CMake cache. Timings identify expensive paths for future investigation; they do
not justify an algorithm change without additional profiling.

## Binary baseline after logarithm cancellation protection

The same machine/compiler/Release setup ran all 156 binary cases in five
sequential processes, validating every result. Median milliseconds for ordinary
half-even cases (z=1+i) were:

| Operation | Second operand | 34 digits | 250 digits |
| --- | --- | ---: | ---: |
| div | 3-2i | 0.016 | 0.038 |
| log | 2-0.5i | 10.997 | 143.399 |
| pow | 0.3-0.2i | 9.264 | 124.273 |
| log, close-input regression | 1+(1+1e-60)i | 29.876 | 231.219 |

The close-input logarithm now spends additional precision to retain its tiny
imaginary component. Raw per-case output is `build/complex-binary-baseline.txt`;
matching metadata, CMake cache, patch and source snapshots use the same prefix.
These are separate operation timings, not a before/after comparison or a
memory measurement. An idle local web server remained running.
