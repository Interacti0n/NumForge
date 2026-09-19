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
require them instead of allowing a local skip; CI CTest jobs do this.
Unity is a pinned, test-only dependency. Testing tools are not installed with
the production library.

## Independent numerical oracle

`tests/test_numeric_oracle.js` generates 7012 reproducible cases from seed
`0x12345678`, sends them to `numeric_oracle_driver`, and compares all output
lines against independent JavaScript BigInt and exact rational arithmetic.
It tests signed integer arithmetic, division/remainder, GCD, powers, decimal
arithmetic, rescaling, calculator exact-first division, and fixed-scale/significant division in all six rounding
modes. Cases have bounded coefficients of up to 140 digits and bounded scales.
The reference does not call NumForge to obtain expected values. This includes
700 calculator-level gcd/lcm/mod/isqrt cases with signed inputs and roots of
up to 280-digit numbers, including square ±1 boundaries. The root reference
uses binary search rather than the implementation's Newton iteration.
Unit tests additionally exhaust roots from 0 through 1024 and exercise invalid
domains, zero, decimal integer spellings and 64-bit limb boundaries. Allocation
and deterministic-deadline injection cover the integer-call pipeline.

Another 520 cases cover real roots: 480 rounded results in all six modes and
40 exact finite roots. The independent reference compares rational powers
and exact midpoints using binary search. `roots_tests` covers domains, aliasing,
extreme int64 scales, rounding boundaries, exact large coefficients and Unicode
aliases. Allocation injection visits every allocation on representative exact
and irrational paths; deadline injection samples early and deep checkpoints.

`transcendental_tests` covers known exponential and logarithmic values,
directed rounding, compact magnitudes, arbitrary bases, aliases, invalid
domains and the strong destination-preservation contract. Allocation injection
exhausts representative `exp`/`ln` paths and samples the composed logarithm.

`constants_tests` checks stored-value rounding, directed rounding, invalid
arguments, destination preservation, and dynamically calculated 520-digit π,
e, and φ against the complete stored prefixes. Calculator contract tests also
exercise dynamic precision and repeated use of a constant in one evaluation;
allocation injection samples early, middle, and late failures on the dynamic π
path without making the CI suite repeat every expensive high-precision step.

`trigonometric_tests` checks known forward and inverse radian values, directed
rounding, domains, aliasing, large-angle reduction using dynamically extended
π, and destination preservation. Calculator and browser tests cover RAD/DEG,
explicit conversions, exact degree tangent poles, symbolic `sin(π)`, and
retaining a small angle beside a `1E50*π` multiple. Allocation injection
samples early, middle, and late sine/atan failures.

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

`tests/fuzz/fuzz_parser.c`, `fuzz_numbers.c`, `fuzz_formatter.c` and
`fuzz_http.c` are also libFuzzer entry points.
Their portable `fuzz_*_smoke` executables run seed cases and deterministic bytes
on every test platform. Numeric/parser/formatter inputs are limited to 256 bytes, a 100 ms cooperative
budget, 1 MiB cumulative allocation requests and 64 KiB per allocation.
The numeric harness tests public numeric parsing and decimal serialization
round trips. The formatter harness checks deterministic, non-mutating output
and exact full-mode round trips when the output exponent is representable by
the numeric parser. Extreme display exponents may exceed that parser's range;
this is not treated as a formatting failure. Resource exhaustion is allowed.

The HTTP harness accepts up to 8191 bytes and exercises the bounded,
allocation-free framing parser without opening sockets. It checks repeatable
status, frame bounds and incomplete prefixes. `http_request_tests` adds focused
header/body limits, partial requests, duplicate headers and origin regressions.
Socket deadlines and routing remain covered by `web_server_smoke_tests`.

For coverage-guided mutation on Unix with Clang:

```sh
cmake -S . -B build-fuzz -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF -DNUMFORGE_BUILD_FUZZERS=ON -DNUMFORGE_ENABLE_SANITIZERS=ON
cmake --build build-fuzz --parallel 2
mkdir -p build-fuzz/corpus-parser
build-fuzz/fuzz_parser build-fuzz/corpus-parser -dict=tests/fuzz/numforge.dict -max_total_time=30 -timeout=5 -max_len=256
```

Run `fuzz_numbers` and `fuzz_formatter` similarly with separate corpus
directories. For `fuzz_http`, use `tests/fuzz/http.dict` and `-max_len=8191`.
CI runs all four for 30 seconds each, limits RSS to 512 MiB, and uploads
corpora/findings as artifacts.
Replay a discovered input with the corresponding fuzzer executable and add
a focused C regression before fixing it. See [LLVM's libFuzzer documentation](https://llvm.org/docs/LibFuzzer.html).
Short campaigns are regression smoke coverage, not exhaustive fuzzing.

## Real-browser tests (optional locally)

`tests/browser` pins Playwright and its Chromium revision through the committed
npm lockfile. It is separate from CTest and adds no application runtime
dependency. Build `numforge_web` first, then run from `tests/browser`:

```sh
npm ci --ignore-scripts --no-audit --no-fund
npx playwright install chromium
NUMFORGE_WEB_EXECUTABLE=/absolute/path/to/build/numforge_web npm test
```

In PowerShell, use the same install commands, then:

```powershell
$env:NUMFORGE_WEB_EXECUTABLE = 'C:/path/to/NumForge/build/Debug/numforge_web.exe'
npm test
```

Adjust the executable path for your generator/configuration. Playwright starts
and stops its own loopback server on port 18765; set `NUMFORGE_TEST_PORT` to
another free port if necessary. It refuses to reuse an existing server.
Eighteen Chromium scenarios cover both languages: real C calculations and
precision, keypad entry, clipboard, help/navigation, arithmetic errors,
transport failures and stale-response protection. Network-failure and delayed
response cases use controlled interception; ordinary calculations reach C.
Function-group tests also cover keyboard expansion, arity errors, the RAD/DEG
selector, active integer/root/exponential/logarithmic/trigonometric functions, five-line result
expansion and mobile layout.
`function_calls_tests` covers all
registered names, syntax/depth/argument limits and e/E boundaries; allocation
failure tests exercise partial nested calls and argument-array growth.

The dedicated browser CI job installs Chromium with OS dependencies using
`npx playwright install --with-deps chromium`. Failures upload the HTML report,
screenshots and traces for 14 days. Local reports, dependencies and test results
are ignored by Git. Coverage is Chromium-only, not a cross-browser guarantee.

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

## Optional performance benchmarks

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
are enabled. Use a separate benchmark build, not an instrumented release package.

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
reuse. Short scientific output currently still requires full coefficient
conversion, so compare its scaling against `scientific_full` before prioritizing
a prefix implementation.
