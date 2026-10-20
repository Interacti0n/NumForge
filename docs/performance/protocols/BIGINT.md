# BigInt arithmetic measurements

The opt-in `bigint_arithmetic_benchmark` measures public arithmetic and two
private comparison paths. Run them with independently generated full references.
This full-reference suite replaces the older modular-check multiplication sweep.

## Build and run

Use the Release benchmark build described in [application benchmark protocol](APPLICATION.md).
Node.js generates deterministic exact input/result fixtures using its native
BigInt implementation, independently of NumForge:

```powershell
node benchmarks/tools/bigint_cases.js build/bigint-cases.tsv
.\build\bench\Release\bigint_arithmetic_benchmark.exe build/bigint-cases.tsv
.\build\bench\Release\bigint_arithmetic_benchmark.exe build/bigint-cases.tsv --case mul-balanced-64-reuse
node benchmarks/tools/check_bigint_benchmark.js build/bench/Release/bigint_arithmetic_benchmark.exe
node benchmarks/tools/run_baseline.js build/bench/Release build/baseline-bigint --bigint
node benchmarks/tools/run_baseline.js build/bench/Release build/baseline-factorial --bigint --factorial-only --case-timeout-ms 120000
.\build\bench\Release\calculator_benchmark.exe --factorial-only
```

For GCC/Clang single-configuration builds use the executables directly in the
build directory. Add `--quick` to fixture generation and execution for a smoke
check, or to the baseline recorder for three short diagnostic runs. The recorder
requires an empty output directory and selects only the BigInt suite with
`--bigint`; without that flag it records the formatting/cache/HTTP suite.
Full BigInt recording defaults to a 120-second timeout per isolated case;
`--case-timeout-ms` overrides it. Quick runs keep the 15-second default.

`calculator_benchmark --factorial-only` separately measures parse, scalar
evaluation and scientific/auto formatting for 10000!, 20000!, 50000! and
100000!, one iteration per case, checking independently derived output text.
Repeat the process for comparison ranges. This diagnostic disables phase time
limits while retaining memory and output budgets; it does not change the
combined CLI/HTTP calculation's five-second budget. It measures allocation traffic,
not peak live storage. The arithmetic recorder remains isolated and records
three runs of exact factorial/product-tree values and live-memory metrics.

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
- Factorial: 0, 1, 20, 100, 500, 1000, 5000, 10000, 20000, 50000, 100000,
  100001 and 150000. Compare the public
  implementation, which multiplies by a small integer in place, against a private
  recursive balanced product tree composed from public multiplication.
  `--factorial-only` generates/records just this sweep; `--quick` retains
  the bounded 0/1/20/100 smoke cases. Exact references through 100000! fit
  the harness's 1 MiB record buffer. Inputs/references are parsed before
  timing; decimal conversion and calculator/HTTP budgets are not measured.
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

Production uses budget-aware Karatsuba and bounded factorial trees. Selected
[arithmetic comparisons](../baselines/ARITHMETIC.md#karatsuba) and the
[final factorial baseline](../baselines/FACTORIAL.md) record their decisions and
resource boundaries. Older schoolbook-only and pre-admission factorial timings
are superseded and remain in Git history. Private square/tree adapters support
controlled comparisons; they do not change the public API.

CI smoke checks validate exact references and failure contracts without imposing
latency thresholds. A small smoke corpus does not establish a universal crossover.
