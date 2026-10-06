# BigInt arithmetic measurements

The opt-in `bigint_arithmetic_benchmark` measures public arithmetic and two
private experiments. Production algorithms, public API and version are unchanged.
The older `bigint_multiply_benchmark` remains available for continuity with earlier
measurements; its modular sanity checks do not replace this suite's exact oracle.

## Build and run

Use the Release benchmark build described in [BENCHMARKS.md](BENCHMARKS.md).
Node.js generates deterministic exact input/result fixtures using its native
BigInt implementation, independently of NumForge:

```powershell
node benchmarks/bigint_cases.js build/bigint-cases.tsv
.\build\bench\Release\bigint_arithmetic_benchmark.exe build/bigint-cases.tsv
.\build\bench\Release\bigint_arithmetic_benchmark.exe build/bigint-cases.tsv --case mul-balanced-64-reuse
node benchmarks/check_bigint_benchmark.js build/bench/Release/bigint_arithmetic_benchmark.exe
node benchmarks/run_baseline.js build/bench/Release build/baseline-bigint --bigint
```

For GCC/Clang single-configuration builds use the executables directly in the
build directory. Add `--quick` to fixture generation and execution for a smoke
check, or to the baseline recorder for three short diagnostic runs. The recorder
requires an empty output directory and selects only the BigInt suite with
`--bigint`; without that flag it records the formatting/cache/HTTP suite.

CTest includes `bigint_arithmetic_benchmark_smoke` when benchmarks, tests and
Node.js are available. `NUMFORGE_REQUIRE_NODE_TESTS=ON` rejects a missing Node.
The smoke driver also deliberately corrupts an expected result and requires the
benchmark to fail. `--check` in a test-enabled build exhausts every allocation
failure in the private square/product-tree/power experiments, tests output/input
aliasing and verifies that failure preserves the destination. These checks run
outside benchmarks and remain active under Release/NDEBUG. Baseline recording
does not run them inside the measured process.

## Scenarios

- Multiplication: 1–1024 64-bit limbs, with dense points at 24, 32, 40, 48, 56,
  64, 80, 96, 112, 128, 160 and 192. Balanced, one-limb, approximately 8:1 and
  2:1 shapes run in both input orders. Random, maximal, sparse and alternating
  words exercise carries and value patterns. Include negative values, zero/one,
  separate output, aliases to either input and a single object for both inputs
  and output. Compare fresh and warmed output payloads, and measure input copies
  separately.
- Squaring: generic multiplication with identical inputs versus a private
  symmetric schoolbook square. The experiment computes diagonal terms once and
  off-diagonal products once, then adds each off-diagonal term twice with explicit
  carry propagation. Random, maximal, sparse, alternating, zero, sign and alias
  cases have exact references. This is a simple experimental kernel, not a
  production recommendation.
- Factorial: 0, 1, 20, 100, 500, 1000, 5000 and 10000. Compare the public
  implementation, which multiplies by a small integer in place, against a private
  recursive balanced product tree composed from public multiplication.
- Powers: 1, 4, 16 and 64-limb bases with exponents 0, 1, 2, 3, 16, 17 and 64.
  Compare public binary exponentiation with an experimental equivalent using the
  private square. Include zero, unit, negative bases and aliases.
- Division, modulo and GCD: 1–64-limb base operands (constructed exact/residual
  dividends reach 96 limbs); one-limb, similar-sized and unequal
  divisors; smaller dividend; exact and residual divisions; all sign combinations.
  Measure combined quotient/remainder and each public operation separately.
  Include quotient/remainder aliases to the two inputs, GCD with a common factor,
  consecutive Fibonacci numbers and zero/unit boundaries.

Division/GCD and factorial sizes are intentionally bounded separately from
multiplication. Their existing algorithms can be much more expensive at the
largest sizes. These ranges describe the recorded workloads, not API limits.

## Timing and memory methodology

Input decimal parsing, independent-reference generation, alias-input restoration,
result comparisons and CSV serialization are outside the timed region. Every
operation's complete result is compared with its independently generated exact
BigInt reference; combined division also checks the complete remainder.
Formatting is never needed for a measured operation or its result check.

Three discarded warm-up invocations precede seven latency samples (three in quick
mode). Each sample sums individual operation timings until at least 5 ms of
operation time (1 ms quick), with a 65536-invocation cap. Every invocation restores
the same operands outside timing, preventing growing chained products in alias
cases. Min/median/max columns are averages per invocation within a sample, not
percentiles of individual requests. This harness uses two timer reads per
invocation: small zero/copy operations can be dominated by timer overhead. Do not
use those rows to claim tiny speedups. No timer-overhead subtraction is applied.

`reuse` warms the output outside timing; `cold` starts with an empty output limb
buffer each invocation. These are stack BigInt objects: object creation/destruction
is excluded. Public multiplication currently allocates a fresh product even for
a warmed output, so `reuse` does not imply in-place reuse by its implementation.
Input copy cost is reported as its own operation, not included in alias timings.

After timing, a separate one-operation sample enables the private live-allocation
ledger. CSV records requested allocation calls/bytes, live payload before/after
the operation, and its absolute peak payload. Live baseline includes working
operand copies and any warmed output, but excludes reference/fixture objects
allocated before enabling tracking. Cleanup must return tracked live payload to
zero with complete bookkeeping. Allocator metadata and overhead are excluded.
Requested-allocation counters remain enabled in timing; live tracking is disabled.

`process_peak_bytes` is the platform process peak described in BENCHMARKS.md. It
includes file buffers, fixture/reference parsing and runtime overhead. The recorder
runs one selected case per fresh process, so previous cases cannot contaminate
that peak. Direct whole-suite invocation has a cumulative process peak and must
not be interpreted as independent peaks. Live payload is the more useful metric
for attributing an operation's allocations.

Some experimental fixtures omit a redundant second operand. Compare
`peak_live_bytes - baseline_live_bytes` when assessing additional operation
payload, and inspect both baselines before comparing absolute peaks. Requested
bytes describe allocation traffic; fewer requests or faster execution need not
mean lower peak memory.

The recorder preserves three full runs, exact fixture TSV, CSV/stderr per case,
commit/status/patch/source snapshots, CMake cache and machine/power metadata.
No CPU affinity or frequency lock is applied. Compare repeated runs on the same
hardware/compiler/settings and avoid other builds/tests while recording.

## Algorithm decisions

Karatsuba is not implemented, so these measurements establish the schoolbook
baseline and dense sampling points for a future comparison. They cannot identify
a Karatsuba crossover alone. The private tree and square experiments allow exact
comparisons now, but production adoption belongs to review item 1.4 and requires
evidence across relevant sizes, compilers and architectures. No CI timing
thresholds are introduced.

A dedicated Release smoke job checks this harness on Linux, Windows and macOS
in CI. It validates exact references and failure contracts, without imposing
latency thresholds or changing the ordinary production-build test jobs.

## Initial baseline: 7 October 2026

Three complete runs were recorded on Windows x64, AMD Ryzen 7 7435HS, MSVC
19.51.36247 Release. The measured working tree is based on `90fa792`; the
fixture TSV, source snapshot, patch, compiler description and all raw rows are
preserved locally in `build/baseline-1-2-full`. There are 1084 scenarios per run
and 3253 successful recorder commands including the allocation check. Each
scenario runs in its own process and checks the complete independent reference.

The ranges below are the minimum and maximum of the three per-run medians:

| Scenario | Median range | Allocation requests | Requested bytes |
| --- | ---: | ---: | ---: |
| Balanced multiplication, 64 limbs | 26.8–28.3 µs | 1 | 1024 |
| Balanced multiplication, 1024 limbs | 6.67–7.14 ms | 1 | 16384 |
| Generic square, 1024 limbs | 6.53–7.51 ms | 1 | 16384 |
| Experimental symmetric square, 1024 limbs | 17.40–17.71 ms | 1 | 16384 |
| Public factorial, 1000 | 221–237 µs | 9 | 4088 |
| Product tree, 1000 | 232–238 µs | 1997 | 30616 |
| Public factorial, 5000 | 6.83–8.94 ms | 11 | 16376 |
| Product tree, 5000 | 3.20–3.46 ms | 9997 | 176152 |
| Public factorial, 10000 | 31.82–32.50 ms | 12 | 32760 |
| Product tree, 10000 | 13.09–14.07 ms | 19997 | 382856 |
| Public power, 64-limb base ^64 | 35.96–37.49 ms | 9 | 97808 |
| Power with experimental square, same base/exponent | 92.35–95.52 ms | 9 | 97808 |
| Combined division, 64-limb dividend / 97 | 83.5–85.8 µs | 4 | 1040 |
| Combined division, similar 64-limb operands | 156–179 µs | 10 | 2552 |

The product tree is a candidate for larger factorials, with a clear cost: at
10000, absolute tracked peak payload is 44464 bytes versus 32776 for the public
algorithm, despite the tree's lower warmed destination baseline. The extra
operation peaks are 29640 versus 16384 bytes. It also creates far more allocation
traffic. At 1000 its timing ranges overlap the public implementation. These
data do not select a universal threshold; compilers, input sizes and architecture
must be considered before production adoption.

The simple symmetric square reduces the number of wide multiplications but its
carry/addition implementation is slower for these large random operands. The
corresponding power experiment is slower too. This experiment should not replace
production multiplication; a different square kernel needs its own measurements.

Warmed and fresh multiplication both request a new product allocation. Dense
sampling establishes a reproducible schoolbook baseline but provides no Karatsuba
crossover. Division and difficult GCD workloads are now measured directly; for
example the 2048-step consecutive-Fibonacci fixture takes 29.1–31.0 ms and makes
15589 requests. A normalized-division or GCD optimization would need comparison
against these same exact inputs rather than a general speed claim.

No affinity/frequency lock was applied. These findings describe one machine and
compiler. Platform smoke jobs are configured, but Linux/macOS CI results were
not obtained locally. Production arithmetic is unchanged.

Local validation passed the 28 ordinary MSVC Release tests, all 32 instrumented
MSVC tests and all 32 GCC 16.2 Release tests with warnings-as-errors. The 269-case
smoke oracle includes exhaustive private-experiment allocation failures and
corrupt-reference rejection.
All 1084 full-size references also passed on GCC with short timing samples;
these were correctness checks, not a GCC performance baseline. A separate GCC
Release build with `BUILD_TESTING=OFF` built and ran the standalone harness.
