# Performance refresh — 11 October 2026

The optimizations through `8c254e8` remove many previous five-second failures.
The strongest remaining obstruction is cumulative allocation traffic in GCD,
exact rational normalization and high-precision decimal series. Large
multiplication is still quadratic, but Karatsuba alone would not remove these
allocation-budget failures.

## Coverage and method

The audit adds 128/256/512/1,024-limb division shapes (single-word, unequal,
exact/nonzero remainder), balanced/8:1/reversed multiplication at
2,048/4,096/8,192 limbs, same-operand squaring, complex forward precision sweeps
through 5,000/10,000 output places, inverse/root/power and tiny/branch/scaled
complex cases. Separate decimal core operation time/call counts now distinguish
add/sub/mul/div/mod/pow/GCD/divmod. Dedicated HTTP queue and unrestricted large
GCD probes complete the coverage.

Windows x64, AMD Ryzen 7 7435HS, MSVC 19.51.36247.0, Release, request counters and
failure testing enabled. Numeric algorithms are from `8c254e8`; pending changes
are diagnostics only. Metadata preserves source/patches, CMake/compiler/machine
and power information. Measurements run serially after builds/tests finish;
CPU affinity/frequency and unrelated background processes are not controlled.

**4,861 primary commands completed**, without process timeouts, reference
mismatches or unexpected harness failures. Fixtures: 1,067 decimal/math and 400
integer cases, each in three fresh processes; 117 calculator scenarios in three
processes; 36 separate profiles; formatting/cache/HTTP and complex probes.
**90 calculator scenarios succeed 3/3; 27 reach a size/resource limit 3/3.**
A completed recorder command is not proof that its expression succeeded.

A supplement completes **22 commands**: nine same-operand squares, three large
GCD calls and ten single reason probes. The updated recorder includes the four
extra integer fixtures inline (404 total) and a `budget_reason` column, so a new
recording has different counts. Reason probes own the same production budget
scope for observability; nested compute/format stages reuse it. `allocation`
covers cumulative or single-allocation exhaustion. `none` can accompany a
separate domain/output/size failure. Production limits are unchanged.

All 3,201 decimal checks pass retained oracle policies, including 16 existing
bounded directed cases per run (48 total). Nearest/new arithmetic expectations
remain strict. Complex shards verify 321 cases at each of 34/250 digits over five
passes: 3,210 checked calls. Integer samples check complete native Node BigInt
results. New high-precision calculator scenarios are resource diagnostics,
not independent full-output mathematical oracles.

## Complete calculator pipeline compared with the original audit

Three fresh processes per case, Auto, five seconds, 64 MiB cumulative allocation
requests, 512 KiB per allocation and 65,536 output bytes. Places here are output
decimal places; direct API precision is significant digits. Old failed calls
are not successful completion baselines or a basis for speedup ratios.
Comparisons use matching expressions/settings and valid retained follow-ups;
old invalid conversion/ASCII constant fixtures are excluded.

| Expression | Places | Original audit | Current median | Current requested bytes |
| --- | ---: | --- | ---: | ---: |
| `100000!` | 10 | Time limit | 1.707 s | 1,332,163 |
| `2^1000000` | 10 | Time limit | 0.687 s | 2,210,237 |
| `ncr(100000;50000)` | 10 | Size/resource limit | 0.596 s | 96,402 |
| `ln(2)` | 2,000 | 4.491 s | 0.350 s | 23,195,797 |
| `ln(1+i)` | 2,000 | Time limit | 0.570 s | 37,972,615 |
| `tan(1+i)` | 2,000 | Time limit | 0.610 s | 43,431,881 |
| `isqrt(2^100000)` | 10 | 2.619 s | 0.081 s | 1,048,377 |
| `sqrt(2)` | 10,000 | 1.481 s | 0.052 s | 1,085,826 |
| `root(2;7)` | 10,000 | Time limit | 0.674 s | 3,638,638 |
| `π` | 10,000 | Time limit | 1.126 s | 24,750,971 |

Direct C API at 2,000 significant digits: medians are 0.685 s for `log10(2)`,
0.355 s for `ln(2)` and 0.386 s for `sin(1E100)`. Original matched-fixture ranges
were 8.795–8.979 s, 4.512–4.575 s and 4.908–5.040 s. Older warmed tables are
contextual. Some random power operands differ after extending the generator;
matching names alone is insufficient, so those comparisons are excluded.

## Remaining failures and exact diagnosis

`gcd(2^100000-1;3^63000)` fails at a median 53.6 ms; `2^100000/3^63000` at 53.1 ms.
Requests reach about 64 MiB. The new reason probe explicitly confirms allocation
exhaustion for GCD.

With operands already constructed, unrestricted BigInt GCD returns the native
reference **3** in a median **0.800 s**, with **233,096 allocations requesting
1,266,618,336 bytes** and only **123,296 bytes peak live tracked payload**.
Parsing/references/output/process startup are outside its timer. Its three
single-call times are 0.778–0.820 s; memory is a separate call. Cumulative requests,
tracked payload and process RSS are different metrics. The issue is repeated
allocation, not a requirement to retain 1.27 GB at once.

At 5,000 places `ln(2)` fails around 2.12 s, `exp(1)` around 1.61 s and
`sin(1+i)` around 1.93 s. Reason probes confirm allocation exhaustion for ln and
complex sine. At 10,000 places some allocation failures take 4.0–4.6 s, leaving
little time headroom on slower machines. Increasing only the deadline would not
fix the measured allocation failures.

`20000!` Full and oversized exact complex Auto output fail with budget reason
`none`, exceeding the separate output policy. Exact complex Auto at ten places
still emits roughly 60,000 bytes before that boundary; scientific notation is
successful for the same value. Any bounded Auto display policy should preserve
the stored exact value and explicit Full/Fraction semantics.

## Separate decimal core profiles

Medians of three 2,000-significant-digit probes. Percentages use profiled phase
totals, not the ordinary API timer. Operation intervals are disjoint; `pow`/GCD
contain internal BigInt work, whose nested multiplication is not counted again
as direct mul. Profiling overhead remains in other/core scope time. Extra clocks
run only in explicit phase probes and are omitted from ordinary builds.

| Fixture | Direct mul | Div + divmod | Pow | Requested bytes | Peak live payload |
| --- | ---: | ---: | ---: | ---: | ---: |
| `exp_ordinary_p2000` | 15.9% | 20.4% | 0.0% | 15,410,781 | 13,560 |
| `ln_ordinary_p2000` | 25.1% | 33.1% | 0.0% | 23,039,000 | 20,331 |
| `log10_ordinary_p2000` | 25.3% | 34.0% | 0.0% | 44,843,035 | 21,507 |
| `sin_large_angle_p2000` | 22.2% | 33.8% | 0.1% | 25,968,236 | 31,090 |
| `sqrt_ordinary_p2000` | 0.9% | 86.7% | 1.4% | 129,414 | 16,482 |
| `root_degree7_p2000` | 0.1% | 37.7% | 56.5% | 498,734 | 53,863 |

Direct multiplication contributes about 16–25% in the exp/log/trigonometric
profiles. Division and preparation matter too. A faster multiply cannot be
assumed to accelerate the entire function by the same factor, and recursive
allocations can worsen the existing budget without bounded scratch reuse.

## Large multiplication, squares and factorial

Cells are medians of three process medians, each using the existing seven-sample
protocol and separate memory probes. Inputs/references are prepared outside the
timers. The two square columns use exactly the same operands.

| Limbs per large operand | Balanced multiply | Generic square (`mul(a,a)`) | Private square |
| --- | ---: | ---: | ---: |
| 2,048 | 29.672 ms | 28.319 ms | 69.221 ms |
| 4,096 | 115.442 ms | 112.112 ms | 277.155 ms |
| 8,192 | 472.887 ms | 445.891 ms | 1108.036 ms |

Doubling balanced size approximately quadruples time, supporting a thresholded
Karatsuba experiment. Keep current small/unbalanced paths until measured
crossovers justify switching. The existing square experiment is slower and
should not be promoted.

Production `100000!` integer median is 1.201 s versus 2.006 s for the private
product tree; allocation counts are 16 versus 199,997. Keep grouped factorial.
No `1000000!` support or argument-limit increase is established here.

## HTTP queue

Dedicated loopback servers return idle `1+2` in 2.03–2.44 ms. Submitted 100 ms
after `100000!`, it waits 1.56–1.59 s while the slow request succeeds in
1.66–1.70 s. Previous approximately five-second blockage is reduced, but the
synchronous server still blocks unrelated requests. Internet/tunnel/browser
latency is excluded; the user's running server and tunnel are untouched.

## Recommended order

1. **GCD/modulo scratch reuse.** GCD already rotates x/y/remainder ownership.
   Public modulo still stages division outputs and normalized arrays each step.
   Introduce a private remainder-only path with reusable normalization/remainder
   scratch for the private GCD loop. Dropping just the unused quotient is not
   enough. Preserve signed public division/modulo, aliases and atomic GCD commit.
   Acceptance: `gcd_large`, `rational_large` and exact complex rational probes fit
   unchanged five-second/64 MiB limits; native references, signs/zeros/aliases,
   every allocation/deadline failure and 32-bit verification pass.
2. **Decimal series workspaces.** Profile/reuse per-call rounding/division,
   powers-of-ten and series temporaries while keeping guard digits, small-input
   stability and all rounding modes. Measure complete 5,000-place requests, not
   just unrestricted scalar latency. Consider binary splitting/argument reduction
   if reducing allocation traffic alone is insufficient.
3. **Thresholded Karatsuba prototype.** Benchmark privately against balanced,
   unbalanced and square shapes with bounded scratch, independent exact oracles
   and fault/cancellation checks. Promote measured crossover ranges only.
4. **Server/display behavior separately.** Before multi-user hosting, design
   bounded workers/processes with safe session/cache ownership and overload
   responses. Agree separately on bounded exact-component eligibility in Auto.

This turn changes benchmark diagnostics and priorities. Production arithmetic,
resource limits, release version and running server/tunnel behavior are unchanged.

## Verification and reproduction

MSVC: eight benchmark/ledger/complex-memory smoke suites pass. GCC: four matching
arithmetic/decimal/audit/ledger suites pass. Final single-call/reason extensions
pass both affected suites on each compiler. Protocol checks reject corrupted
results/remainders, enforce one sample in `--validate`, validate finite/nonnegative
and enclosed operation profiles, and distinguish immediate deadline from domain
failure. Summary checks complete artifacts/counts and the older manifest schema.
Timing thresholds are not CI assertions.

```powershell
node benchmarks/run_performance_audit.js build/factorial-bench/Release build/new-audit
node benchmarks/summarize_performance_audit.js build/new-audit
```

Use an empty output directory and a current Release benchmark/apps/testing build.
Raw data, metadata, independent fixtures and analysis remain in ignored
`build/performance-refresh-2026-10-11`, including its `supplement` directory.
The original audit and follow-ups remain preserved.
