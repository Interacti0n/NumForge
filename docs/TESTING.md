# Testing and performance baselines

## Run the complete local suite

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build --config Debug --parallel 2
ctest --test-dir build -C Debug --output-on-failure
```

Use the configuration you built; `-C Debug` is needed by Visual Studio and
other multi-configuration generators. Node.js enables the UI and numerical
oracle suites with no npm install. Add `-DNUMFORGE_REQUIRE_NODE_TESTS=ON` to
require them instead of allowing a local skip; all current CI jobs do this.
Unity is a pinned, test-only dependency. Testing tools are not installed with
the production library.

## Independent numerical oracle

`tests/test_numeric_oracle.js` generates 5792 reproducible cases from seed
`0x12345678`, sends them to `numeric_oracle_driver`, and compares all output
lines against independent JavaScript BigInt and exact rational arithmetic.
It tests signed integer arithmetic, division/remainder, GCD, powers, decimal
arithmetic, rescaling, calculator exact-first division, and fixed-scale/significant division in all six rounding
modes. Cases have bounded coefficients of up to 140 digits and bounded scales.
The reference does not call NumForge to obtain expected values.

```sh
ctest --test-dir build -C Debug -R numeric_oracle --output-on-failure
```

Failures print the input, expected value and actual value. Preserve any newly
found bug as a small focused C regression as well. Generated numerical tests
do not replace the existing extreme-scale, aliasing and allocation-failure
tests. The driver is trusted test infrastructure, not an input-facing API.

## Parser fuzz smoke and precision contract

`parser_fuzz_tests` parses 3000 deterministic byte strings twice, including
malformed UTF-8, operators, numeric separators and whitespace. It checks
repeatable status/offset behavior and exercises tree cleanup. It never
evaluates random exponent/factorial expressions. This is a bounded smoke
test, not coverage-guided fuzzing; sanitizer CI makes it more useful.

`calculator_contract_tests` fixes the accepted adjacency rules and records
the current intermediate-rounding behavior, including cancellation. It does
not claim a certified bound on numerical error. The actual contract and
worked examples are in [CALCULATOR_DESIGN.md](CALCULATOR_DESIGN.md).

`cli_tests` drives the real executable with redirected UTF-8 input. It covers
precision commands, recovery after syntax/arithmetic/oversize errors, exact
input length boundaries, CRLF, EOF without a newline, and both exit commands.

`tests/fuzz/fuzz_parser.c` and `fuzz_numbers.c` are also libFuzzer entry points.
Their portable `fuzz_*_smoke` executables run seed cases and deterministic bytes
on every test platform. Inputs are limited to 256 bytes, a 100 ms cooperative
budget, 1 MiB cumulative allocation requests and 64 KiB per allocation.
The numeric harness tests public numeric parsing and decimal serialization
round trips; it does not yet test the calculator's scientific formatter.

For coverage-guided mutation on Unix with Clang:

```sh
cmake -S . -B build-fuzz -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_FUZZERS=ON -DNUMFORGE_ENABLE_SANITIZERS=ON
cmake --build build-fuzz --parallel 2
mkdir -p build-fuzz/corpus-parser
build-fuzz/fuzz_parser build-fuzz/corpus-parser -dict=tests/fuzz/numforge.dict -max_total_time=30 -timeout=5 -max_len=256
```

Run `fuzz_numbers` similarly with its own corpus directory. CI runs both for
30 seconds each, limits RSS to 512 MiB, and uploads corpora/findings as artifacts.
Replay a discovered input with the corresponding fuzzer executable and add
a focused C regression before fixing it. See [LLVM's libFuzzer documentation](https://llvm.org/docs/LibFuzzer.html).
Remaining gaps are HTTP framing fuzzing, the calculator formatter harness,
and real-browser interaction/navigation tests.

## Coverage and package CI

Use a fresh GCC build without sanitizers for coverage:

```sh
cmake -S . -B build-coverage -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug -DNUMFORGE_ENABLE_COVERAGE=ON
cmake --build build-coverage --parallel 2
ctest --test-dir build-coverage --output-on-failure
cmake --build build-coverage --target coverage_report
```

`build-coverage/coverage/summary.txt` contains per-file line/branch statistics;
the adjacent `.gcov` files annotate source lines and individual branch counts.
The report selects production `src/` objects, excluding Unity and test sources.
Use a gcov matching the compiler (`NUMFORGE_GCOV` can override discovery).
It reports measured coverage, not correctness, and has no percentage gate.
Fresh build directories avoid accumulated counts from earlier runs.
For compiler instrumentation details, see [GCC coverage options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html).

The HTTP smoke test terminates its server subprocess. Such termination can
prevent gcov counters from being flushed: server coverage may be missing/zero
even though socket tests passed. Do not interpret this as measured non-execution.
Coverage CI uploads the text report and annotated files as the `gcc-coverage`
artifact, retained for 14 days, using [upload-artifact](https://github.com/actions/upload-artifact).

The Clang job runs the standard suite with sanitizers before fuzzing. Linux and
Windows Release installation jobs build separate C and C++ consumers against
the installed package. To enable the latter locally, configure
`tests/package_consumer` with `-DNUMFORGE_TEST_CPP=ON` and a matching
`CMAKE_PREFIX_PATH`; on Windows also match architecture and configuration.
The ordinary library build still requires only a C compiler.

## Optional phase benchmarks

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_BENCHMARKS=ON
cmake --build build-bench --config Release --parallel 2
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
are enabled. Use a separate benchmark build, not an instrumented release package.

The timer is C `clock()`: its resolution and CPU/elapsed interpretation depend
on the platform. Small totals can be zero. Record compiler, architecture,
configuration and machine; compare repeated runs on the same setup. These
initial totals are not a microbenchmark framework or a cross-platform score.
Benchmarks are not installed and impose no absolute time threshold in CI.
Further workloads can be added when profiling new operations.
