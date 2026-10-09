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
