# BigDecimal and higher-function measurements

This opt-in suite measures direct numeric API calls, independently checks their
entire results, and separates timing, live-memory and phase diagnostics. It
implements the measurement block that follows the formatting/cache and BigInt
suites. It does not select or change production algorithms.

## Coverage

- Exact add/subtract with equal scales and gaps of 1, 100 and 1,000 places;
  cancellation, signed multiplication, fresh/reused destinations and both aliases.
  Coefficients contain 16, 100, 500, 1,000 or 2,000 decimal digits.
- Normalization with 0, 1, 100 or 1,000 trailing coefficient zeros. This diagnostic
  deliberately constructs noncanonical private temporaries outside timing, then
  calls the actual normalization helper. Normal public inputs are canonical.
- Rescaling and fixed-scale, significant-digit and exact-first division, including
  terminating and recurring quotients, signs, ties, sticky digits and all six
  rounding modes. A separate large-division group covers every coefficient size
  against terminating, recurring and dense divisors with reused and aliased
  outputs. Division-by-zero cases verify destination preservation.
- exp/ln/log10/log, sqrt/cbrt/degree-7 root, forward/inverse trigonometry and
  hyperbolic functions at 10/100/500/1,000/2,000 significant digits. Inputs include
  tiny values, ln near one, a base near one, `1E100` angles, near-domain-boundary
  values and tangent near a pole. Selected higher-function aliases are included.
- pi/e/phi factory calls at the same matrix and 499/500/501 digits. Through 500,
  the factory rounds stored text; larger requests compute the constant. Repeated
  factory calls are not described as cache hits.

The full matrix has 1,067 cases; the smoke matrix has 738. The latter uses smaller
operands and 10/100-digit math, and excludes the known extreme tangent-pole
family from successful timing checks. That family is still checked separately
as a strict diagnostic, and retained at every precision in the full matrix.

## Independent references and numerical findings

`benchmarks/decimal_math_cases.js` derives arithmetic/division references with
Node BigInt rational arithmetic, without calling NumForge. The 177 frozen math
rows in `benchmarks/references/decimal_math_references.tsv` come from mpmath 1.3.0, evaluated at both
requested precision +240 and +400 decimal digits. The generator requires
identical HALF_EVEN results from both evaluations. The surplus precision covers
the configured large-angle reduction and cancellation cases; it is not a
general error bound for arbitrary future inputs.

Regenerate only with the pinned developer dependency in an isolated project
environment; runtime and CI read the frozen file and do not need Python/mpmath:

```sh
python -m venv build/reference-venv
# Activate the environment using the command for your shell, then:
python -m pip install mpmath==1.3.0
python benchmarks/references/generate_decimal_math_references.py
```

New math rows and nearest-mode results require exact numeric equality. The
suite also imports the existing low-precision directed oracle policy explicitly:
only those legacy rows can differ by up to one output ULP. Such rows report
`bounded_directed` separately. The smoke matrix currently encounters 16 of these
rows. No new high-precision mismatch receives that allowance.

The original baseline's strict references exposed a limitation at
`tan(1.5707963267948966192313216916397514420985)`, extremely close to pi/2.
All five requested precisions produced a mismatch in that diagnostic validation.
At 10 digits, the independent result rounds to `1.180641900E40`; the baseline returned
`1.183431953E40`. Fixed internal guards lose accuracy in this ill-conditioned
division. The [accuracy findings](#historical-accuracy-findings) document the
implemented fix. Historical mismatches remain part of the original measurement
record. The better-resolved `tan(1.570796326794)` case is also measured.

The C harness exits unsuccessfully on any mismatched reference. The full baseline
collector records a `reference_mismatch` outcome and preserves the diagnostic;
it emits no timing or memory CSV for that case. Collection finishing means all
scheduled cases were attempted, not that every mathematical result was correct.
Unexpected harness/process failures still stop collection. Never omit these
outcomes when presenting the successful measurements.

## Method and metrics

Parsing, reference generation/comparison and input restoration are outside the
timer. Aliased inputs are restored before every invocation. Fresh destinations
are created outside timing; their initially allocated object is part of the
baseline memory, not the operation's allocation count. Reuse destinations are
warmed separately, including before each sample. There are five timed samples
in full mode and three in quick mode, accumulating at least 1 ms / 0.2 ms of
API time per sample where possible, capped at 4,096 invocations. `--validate`
uses one invocation instead of accumulating a timing sample.

Allocation-request counters remain enabled in timing samples, but the live
allocation ledger and phase clock are inactive. One separate memory invocation
records allocation calls, cumulative requested bytes, baseline live payload,
live payload after the call and absolute peak payload. Its ledger must be
complete and empty after cleanup. These numbers exclude allocator metadata.
Process peak is the fresh child process's peak working set on Windows or peak
RSS on Unix, including fixtures/setup and all samples; it is not live payload.

A separate invocation measures exclusive diagnostic scopes:

| Scope | Meaning |
| --- | --- |
| Alignment | `multiply_power_of_ten` preparation/copying, excluding nested integer arithmetic. |
| Normalization | Coefficient zero stripping and scale adjustment. |
| Integer core | BigDecimal calls to add/sub/mul/div/mod/div_mod/GCD/pow, including work inside those calls. |
| Other | Remaining decimal logic, conversions, lifetime operations and diagnostic overhead. |

This is a separate profiler; the formatter/HTTP phase contract is unchanged.
Scopes are taken in one invocation, not estimated by subtracting independent
benchmark runs. They are diagnostic samples, not subdivisions of the reported
timing median. Small phases may be below timer resolution and report zero.
Normal builds compile without these hooks or the allocation ledger.

## Build, check and record

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release \
  -DNUMFORGE_BUILD_BENCHMARKS=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON \
  -DNUMFORGE_REQUIRE_NODE_TESTS=ON
cmake --build build-bench --config Release --parallel 2
ctest --test-dir build-bench -C Release -R decimal_math_benchmark_smoke --output-on-failure
node benchmarks/run_baseline.js build-bench build/baseline-decimal --decimal --case-timeout-ms 5000
node benchmarks/summarize_decimal_baseline.js build/baseline-decimal
```

For Visual Studio, give the recorder `build-bench/Release` as the binary directory.
Add `--quick` for a smaller harness exercise. All outputs require an empty
directory. The recorder saves the exact fixture set, source commit/working-tree
patch, new benchmark/profiler sources, CMake cache/compiler metadata, CPU/OS and
power-plan information. Each case gets a separate process in each of three runs.
The read-only summary validates that all three attempts and artifacts are present,
checks metric consistency, and emits a CSV range for every case including the
timeout/mismatch counts. It rejects an incomplete collection.

The per-case timeout bounds the whole child process, including startup, input
parsing, validation, samples and memory/phase probes. A 5-second collection
timeout is not a claim that one API call takes five seconds. Timeout outcomes
are explicit, with no fabricated CSV or successful reference validation. Increase
`--case-timeout-ms` for dedicated investigations of expensive cases. CI runs
smoke correctness/metric checks on Linux, Windows and macOS without performance
thresholds; a smoke pass is not certification of the full stress matrix.

## Historical accuracy findings

The 7 October 2026 matrix (MSVC 19.51, Windows x64, `89e6d2f` plus its saved
patch) had five near-pole tangent reference mismatches per run and 119 process
timeouts over three runs of the initial and added-division sets. A process timeout
includes setup/probes and is not a completed API timing. Failed rows were never
accepted as performance results.

Subsequent input-sensitive pi/2 reduction made all five frozen pole cases match
at 10/100/500/1000/2000 digits in MSVC validation. Ninety additional frozen
references cover signs, both sides, odd multiples, aliases and six rounding modes.
This fixes those observed failures; it is not universal correct rounding. Original
recordings remain in Git history. See selected [arithmetic](../baselines/ARITHMETIC.md)
and [logarithm](../baselines/LOGARITHM.md) comparisons for retained optimizations.

For a one-call diagnostic without warmups or memory/profile passes use
`--validate --timing-only`; independent reference checking remains outside timing.
