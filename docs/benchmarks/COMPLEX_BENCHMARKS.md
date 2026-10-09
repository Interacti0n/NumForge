# Complex performance diagnostics

The independent oracle driver also measures each public unary BigComplex call
with the monotonic platform clock. Input parsing and output conversion are
outside the measured interval. Every timed result must pass the same independent
component error bound as the ordinary test; incorrect results fail the run.

Build `complex_oracle_driver` in an ordinary Release testing build, without
sanitizers, coverage or allocation instrumentation, then run on a quiet machine:

```powershell
node tests/test_complex_oracle.js build/remote-tests/Release/complex_oracle_driver.exe --benchmark
```

The command repeats the 744-case corpus in five fresh processes, emitting
minimum, median and maximum milliseconds for each identical input, precision
and rounding mode. Cases are not pooled across inputs. There is no CI timing
threshold. The command has a 120-second timeout per process and an 8 MiB output
bound. CTest executes one pass without performance assertions.

Keep raw output in the ignored build directory. Record the commit and pending
patch, CMake cache, compiler and machine information alongside it when comparing
changes. These are operation timings, not application latency or memory
benchmarks. Decimal allocation, resource limits and adaptive precision remain
separate follow-up work.

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
