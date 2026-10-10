# NumForge performance audit — 10 October 2026

This is a measurement and prioritization change. Production numeric algorithms,
application limits and the running server are unchanged. Raw measurements stay
in ignored build directories. Conclusions describe one local Windows machine,
not portable completion guarantees.

## Environment and method

Windows x64, AMD Ryzen 7 7435HS, MSVC 19.51.36247.0, Release. Numeric sources are
based on `dbeeb7b`. The recorder preserves the pending patch, new harness sources,
compiler/CMake metadata, CPU/OS and power-plan information. Measurements run
serially without concurrent builds/tests. An existing local server remains
running; CPU affinity, frequency and outside activity are not controlled.

The main recording is `build/performance-audit-2026-10-10`. Reproduce with:

```sh
cmake -S . -B build/performance-audit -DNUMFORGE_BUILD_BENCHMARKS=ON \
  -DNUMFORGE_BUILD_APPS=ON -DBUILD_TESTING=ON -DNUMFORGE_WARNINGS_AS_ERRORS=ON
cmake --build build/performance-audit --config Release --parallel 2
node benchmarks/run_performance_audit.js build/performance-audit/Release build/audit-results
node benchmarks/summarize_performance_audit.js build/audit-results
```

Single-config generators additionally need `-DCMAKE_BUILD_TYPE=Release` and
executables directly under the build directory. The output directory must be
empty. The summary rejects incomplete recordings and missing attempts/artifacts.

- The full independent decimal/math matrix runs once per case in each of three
  fresh processes. `--validate --timing-only` excludes untimed warm-up and separate
  memory/phase probes. The timed API result still undergoes the existing strict
  oracle check; permitted legacy directed differences remain labelled.
- Representative BigInt size sweeps, formatting and cache cases retain the existing
  multi-sample protocols and separate memory probes. Selected BigInt results
  are compared with complete independent Node BigInt references.
- Complex calls at 34 and 250 digits use five independently checked corpus passes.
  The current driver has request counters enabled, unlike the historical
  uninstrumented complex runs; those numbers are contextual comparisons,
  not a controlled before/after speedup claim.
- Application stress probes call the complete `calculator_compute` pipeline,
  including parsing, typed evaluation and formatting, with one shared 5,000 ms
  deadline, 64 MiB cumulative allocation requests, 512 KiB single allocations,
  and 65,536-byte output limit. Their statuses are exploratory application
  outcomes, not independent mathematical oracle checks.
- A process timeout is never labelled a successful five-second operation. Startup,
  fixture parsing, reference comparison and CSV output are outside the API timer,
  but inside the child-process timeout. Failed calculations report failure
  elapsed time rather than the completion time of a correct answer.

Historical raw formatting/decimal/general BigInt recordings referenced in older
reports are no longer present locally. Compare against their published tables;
the extended factorial arithmetic and pipeline recordings are still present.
Old decimal measurements used warmed multi-sample medians; new single-call
ranges are scale checks, not rigorous regression percentages.

## Measured results

The main recording contains **4,530 process attempts**: 4,527 completed and three
whole-process timeouts, all for the repeatedly sampled `100000!` BigInt harness.
That harness performs destination warm-up as well as discarded warm-ups, seven
timed samples and a memory probe: the original 60-second child limit was too short
for the aggregate. The final recorder allows 120 seconds; the historical timeout
attempts are preserved and a follow-up rerun measures this case explicitly.

All **3,201 decimal reference checks** passed. Sixteen legacy directed-rounding
cases per run use their existing one-ULP policy (48 attempts); all other decimal
checks match the strict expectations. There were no mathematical reference
mismatches. Complex probes verified 321 cases at each of 34/250 digits in five
passes: **3,210 independently checked calls**, yielding 642 timing summaries.

The main application matrix mistakenly included an invalid two-argument
`convert(qty(...); ...)` fixture. Its three syntax-error outcomes are retained;
they are not conversion timings. The recorder now uses the valid three-argument
form, and valid conversion/quantity cases are measured in the follow-up. A smoke
test checks `qty(1/3;"m")+qty(1/6;"m") = 0.5 m` independently.

### Comparison with retained historical tables

Ranges are min/max of the three per-run measurements. Different sampling methods,
unlocked CPU clocks and unavailable old full fixtures prevent attributing small
differences to a regression or optimization.

| Case | Historical result | Current result |
| --- | ---: | ---: |
| `ln(2)`, 100 significant digits, direct API | 4.261–5.221 ms | 4.070–5.256 ms |
| `sqrt(2)`, 1,000 significant digits, direct API | 11.928–12.698 ms | 11.567–11.891 ms |
| Dense division, 2,000 digits, significant | 3.806–3.982 ms | 3.545–3.656 ms |
| Dense division, 2,000 digits, exact-first | 23.278–26.220 ms | 23.343–27.124 ms |
| pi factory, 501 digits | 47.954–49.678 ms | 47.821–58.788 ms |
| Balanced multiplication, 1,024 limbs | 6.67–7.14 ms | 6.54–9.50 ms |
| Experimental product tree, `100000!` | 1.878–1.891 s | 1.892–1.961 s |
| Scalar evaluation of `100000!`, including decimal projection | 7.852–7.877 s | 7.873–8.001 s |
| Short formatting after that scalar evaluation | 0.303–0.339 ms | 0.302–0.412 ms |
| Auto, 16,384-digit coefficient, ten places | 2.951–2.975 ms before prefix optimization | 0.127–0.131 ms |
| Same Auto case, later prefix follow-up | 0.124–0.193 ms | 0.127–0.131 ms |
| Legacy `1/3` cache cold / hit | 37.4–59.4 / 15.2–18.2 µs | 37.4–37.9 / 15.2–15.4 µs |

The 9.50 ms multiplication value occurred in the first recording; the other two
medians were 6.54/6.86 ms. Sizes match the old table, but old complete input fixtures
are unavailable. These observations do not establish an arithmetic regression.
Short-formatting improvements predate this audit; no new performance change was
made here. Full 16,384-digit scientific formatting still takes 2.90–2.95 ms.

### Individual public math calls near or over five seconds

These direct APIs have no implicit application deadline in this benchmark. Every
listed result passed its independent reference. Precision is significant digits.

| Operation | Precision | Single-call range |
| --- | ---: | ---: |
| `log10(2)` | 2,000 | **8.795–8.979 s** |
| `log(2;3)` | 2,000 | **8.668–8.898 s** |
| `atanh(0.99999999999999999999)` | 2,000 | **4.980–5.049 s** |
| `sin(1E100)` | 2,000 | **4.908–5.040 s** |
| `cos(1E100)` | 2,000 | 4.904–4.924 s |
| `log(2;1.00000000000000000001)` | 2,000 | 4.757–4.818 s |
| `ln(2)` | 2,000 | 4.512–4.575 s |

Even calls individually below five seconds can exceed it when combined in one
expression, when component computations are repeated, or when formatting adds
more work. The high-precision tangent-pole regression now passes all unchanged
references; it is no longer the historical accuracy mismatch.

### Complete application outcomes with production budgets

Precision below is the calculator output setting; working precision includes
its guards, so it differs from the direct API matrix. Three cold runs per case.

| Expression / setting | Outcome | Elapsed range |
| --- | --- | ---: |
| `50000!`, Auto 10 places | success | 2.612–2.635 s |
| `100000!`, Auto 10 places | time limit, 3/3 | approximately 5 s |
| `2^100000`, Auto 10 places | success | 54–58 ms |
| `2^1000000`, Auto 10 places | time limit, 3/3 | approximately 5 s |
| `2^100000/3^63000`, Auto 10 places | time limit, 3/3 | approximately 5 s |
| `ln(2)`, 2,000 places | success, little time headroom | 4.484–4.609 s |
| `sin(1E100)`, 2,000 places | time limit, 3/3 | approximately 5 s |
| `ln(1+i)` / `tan(1+i)`, 2,000 places | time limit, 3/3 each | approximately 5 s |
| `ln(1+i)`, 1,000 places | success | 1.205–1.255 s |
| `tan(1+i)`, 1,000 places | success | 2.173–2.313 s |
| `tan(1+i)`, 10,000 places | value too large, 3/3 | 3.407–3.461 s |
| `sqrt(2)`, 10,000 places | success | 1.450–1.486 s |
| `20000!`, Full | output too large, 3/3 | 414–421 ms |
| `10000!`, Full | success, 33,169 output bytes | 90–92 ms |
| `2^200000`, plain | success, 60,206 output bytes | 203–212 ms |

The 10,000-place complex tangent requests approximately 64 MiB cumulatively
before failing, despite its prospective output being below 65,536 bytes. It is
an allocation-budget failure, not proof that the process needed 64 MiB live RAM.
The Windows budget clock has coarser granularity than the benchmark clock, so
time-limit outcomes can measure slightly below 5,000 ms; this is not completion.

Auto Cartesian formatting of the exact value `2^100000+2^100000*i` emits 60,211
bytes despite the ten-place setting. The exact complex rational Auto path is
different from bounded-prefix decimal scientific output and deserves a separate
size-aware formatting policy. Scientific notation is measured separately in the
follow-up before proposing any behavioral change.

### Isolated integer materialization follow-up

Raw follow-up output, its recorder and source snapshots are preserved in
`build/performance-audit-followup-2026-10-10`. The full seven-sample BigInt
`100000!` rerun completed with a **4.054 s median** (individual timed samples
3.986–4.085 s). Its whole harness process took 90.4 s, explaining the earlier
60-second process timeouts without implying a 60-second factorial operation.

The complete typed calculator pipeline for `100000!`, Auto ten places, took
**11.598 s** with only its diagnostic deadline extended to 60 seconds. The
allocation and output budgets remained enabled; the output matched the independent
literal `2.824229408E+456573`. This is a single cold pipeline observation.
The earlier 7.9-second scalar benchmark does not include the full typed display
path: `calculator_format_value` materializes the authoritative integer again,
even when evaluation already created a decimal projection. The pipeline's actual
5-second mode still fails in all three main-recording runs.

For the exact `100000!` integer, three isolated conversion samples measured:

| Bridge | Time | Allocation requests / cumulative bytes |
| --- | ---: | ---: |
| Existing public `bigdecimal_from_bigint` | **3.768–3.802 s** | 22 / 1,892,255 B |
| Private fresh coefficient copy + normalization | **0.475–0.480 s** | 1 / 262,144 B |

Factorial generation, fresh experimental object construction and full integer
round-trip checks are outside these timers. Both outputs restore the original
integer exactly. The private experiment is about eight times faster for this
bridge, but excludes the public atomic commit and does not replace the library.
Its residual normalization cost is significant. Reusing a valid existing decimal
projection could additionally remove duplicate work, subject to precision/context
and ownership checks.

Combining optimizations might bring `100000!` within five seconds with headroom,
but that must be measured after safe integration. This experiment does not
establish a sub-minute result for `1000000!`.

### Higher-function phase and allocation evidence

Separate exclusive phase probes attribute approximately **92–94%** of the
profiled `ln(2)`/large-angle sine at 1,000 digits and `log10(2)` at 2,000 digits
to the integer core. The core aggregates add/sub/mul/div/mod/GCD/pow: this
does not isolate division alone. Alignment/normalization were small fractions.
Phase totals are separate diagnostic invocations, not subdivisions of the
unprofiled latency samples.

The independently verified 2,000-digit `log10(2)` memory probe made **199,663
allocation requests totalling 92,015,325 bytes**, with only **27,840 bytes peak
tracked live payload**. This directly demonstrates why the cumulative request
budget can matter even when the arithmetic is not retaining much memory. Merely
raising time or live-memory allowances will not address repeated temporary work.

### Web queue follow-up

On three dedicated loopback servers, `1+2` alone took **1.68–2.37 ms**. When sent
100 ms after an unrelated `100000!` request, the same fast expression took
**4.886–4.888 s**, and its result was still correctly `3`. The slow request
returned `TLE` after approximately 4.99 s. These are local network round trips;
they exclude Internet/tunnel latency and browser rendering. Existing running
server/tunnel processes were not used or stopped.

### Additional number-theory and constant probes

Three complete-budget runs per row are preserved in
`build/performance-audit-number-theory-2026-10-10` and
`build/performance-audit-constants-2026-10-10`. Constant expressions use UTF-8
input files (`--input-file`), avoiding Windows narrow command-line transcoding.
The earlier ASCII `pi`/`phi` display labels yielded invalid-token diagnostics;
those rows are retained and are not presented as constant timings.

| Expression / setting | Outcome | Complete-pipeline range |
| --- | --- | ---: |
| `ncr(1000;500)`, Auto 10 places | success | 9.3–10.1 ms |
| `ncr(10000;5000)`, Auto 10 places | success | 0.997–1.048 s |
| `ncr(100000;50000)`, Auto 10 places | cumulative allocation limit, 3/3 | 4.474–4.512 s |
| `npr(10000;10000)`, Auto 10 places | cumulative allocation limit, 3/3 | **61.7–63.0 ms** |
| `npr(50000;50000)`, Auto 10 places | cumulative allocation limit, 3/3 | 60.4–61.3 ms |
| `gcd(2^100000-1;3^63000)`, Auto 10 places | time limit, 3/3 | approximately 5 s |
| `lcm(2^10000-1;2^9999-1)`, Auto 10 places | success | 9.8–10.3 ms |
| `isqrt(2^10000)`, Auto 10 places | success | 20.7–21.6 ms |
| `isqrt(2^100000)`, Auto 10 places | success | 2.547–2.622 s |
| `exp(1)`, 2,000 places | success | 1.617–1.638 s |
| `exp(1)`, 10,000 places | cumulative allocation limit, 3/3 | 3.437–3.536 s |
| `root(2;7)`, 10,000 places | time limit, 3/3 | approximately 5 s |
| `cbrt(2)`, 10,000 places | success | 2.998–3.031 s |
| `π`, 2,000 places | success | 0.864–0.888 s |
| `π`, 10,000 places | time limit, 3/3 | approximately 5 s |
| `φ`, 10,000 places | success | 1.320–1.379 s |
| `convert(1/3;"m";"cm")`, Auto 10 places | success | 0.165–0.291 ms |
| `qty(1/3;"m")+qty(1/6;"m")`, Auto 10 places | success | 0.166–0.178 ms |

`npr(10000;10000)` is mathematically the same value as `10000!`, which completes
under the normal budget. The falling-product implementation repeatedly invokes
generic BigInt multiplication, allocating a new product buffer as it grows.
The recorded failure requested approximately 64 MiB cumulatively; it is not a
factorial argument limit or a five-second timeout. Reuse the existing machine-word
multiply helper when the factor fits, on the private working value, retaining the
generic fallback. For combinations, pair this with an exact small-denominator
division path and retain its zero-remainder validation. Consider factorial reuse
for `npr(n;n)` within the factorial domain. Test large n with small r, symmetry,
aliases, exact full references and every allocation failure before integration.

Compound `ln(2)+ln(3)`, `sin(1E100)+cos(1E100)` and `exp(ln(2))` at 2,000 places
each hit the shared deadline in all three runs. Requested output precision alone
cannot predict completion; expression complexity and intermediate reuse matter.

For `2^110000+2^110000*i`, Auto fails on output size in 133–141 ms, while explicit
scientific output succeeds in 149–157 ms with just 43 output bytes. This supports
reviewing the exact-complex Auto eligibility policy independently of arithmetic.

Across the primary and three follow-up collections, **4,626 recorder attempts**
are preserved. Only the three original repeated-factorial process timeouts lacked
successful measurements; their successful longer rerun is separate. Exploratory
application failures and invalid input diagnostics remain visible, never counted
as successful numerical answers. The final recorder includes the newly added
number-theory, high-precision and UTF-8 constant cases, so a fresh invocation has
more cases than the original main
recording; each saved manifest/fixture set is authoritative for its own run.


## Recommended implementation sequence

1. Replace the public integer-to-decimal text round trip safely and remove only
   demonstrably redundant materialization. Re-measure complete requests.
2. Use small-factor in-place arithmetic on private npr/ncr temporaries and optimize
   general integer division, starting with single-limb divisors, then
   normalized limb-wise division. Measure high-precision math and cumulative
   allocation traffic; follow with safe private scratch reuse if justified.
3. Add size-adaptive Karatsuba and improve factorial grouping/product trees.
   Measure small/unbalanced operand regressions before changing defaults.
4. Resolve large exact complex Auto output and fail impossible Full output early,
   preserving stored values and explicit-format semantics.
5. Before serving multiple simultaneous users, introduce bounded computation
   workers with safe session/cache ownership and observable queue behavior.

The first step is the strongest demonstrated low-scope opportunity. The second
targets the broader high-precision bottleneck. The third is necessary to pursue
million-factorial performance with the project's own arithmetic. These remain
proposals for agreement; no production implementation is part of this audit.

## Proposed changes and required checks

### 1. Remove the integer → text → decimal round trip

`bigdecimal_from_bigint` calls `bigint_to_string`, then `bigdecimal_set_string`.
Even a short scientific result can require full decimal serialization and
parsing earlier in evaluation. Bounded-prefix formatting cannot remove that
earlier work. A full-length temporary allocation precedes the output limit.

Proposed change: construct a private decimal temporary by copying the integer
coefficient, assigning scale zero, normalizing and committing atomically. Retain
the public representation, sign and destination-preservation contracts.
Then measure trailing-zero normalization separately; copying alone does not
eliminate its cost. Lazy decimal materialization for authoritative integer/rational
calculator values is a separate possible follow-up.

Required checks: zero/signs, powers of ten, long zero suffixes, source preservation,
every allocation failure, scale boundaries and calculator/API output equivalence.
Compare conversion and entire requests independently.

### 2. Optimize integer division and precision-dependent math

General `bigint_divmod_abs` uses bit-at-a-time long division. Decimal logarithm,
trigonometric and hyperbolic series repeatedly perform significant division,
rounded multiplication and normalization. High precision combines many expensive
integer operations; large-angle reduction additionally needs more digits of pi.
This is a source-based explanation, not an assumption that every millisecond is
caused by multiplication.

Proposed sequence: profile expensive calls; add a general single-limb divisor
path using existing wide division helpers; implement normalized limb-wise long
division with verified quotient correction; compare higher-function performance.
Preserve signed quotient/remainder semantics, aliases and atomic failure. Study
per-request reuse of constants, power-of-ten preparation and common sin/cos
intermediates afterwards.

For still-expensive large precisions, evaluate binary splitting or stronger
argument reduction per function. Preserve cancellation guards near poles and
branches; reducing work precision indiscriminately is not an acceptable fix.

### 3. Improve multiplication and large factorials after the conversion baseline

Keep schoolbook multiplication for small or strongly unbalanced operands. Add
Karatsuba with measured size/ratio thresholds and bounded scratch ownership.
Group small factorial factors into machine-word products, then adopt a balanced
product tree with a measured crossover. The existing tree is experimental and
creates substantially more allocation traffic. Prime Swing/Toom-Cook are later
options if measurements justify them.

Do not raise factorial argument, memory or time limits based solely on GMP.
A million-factorial feature must pass the complete NumForge pipeline, including
short/full output and cancellation/resource behavior.

### 4. Separate computation failures from web queue waiting

The loopback server accepts and handles one connection at a time. A computational
deadline begins when a request is processed; waiting behind another request is
outside that deadline. A fast expression can therefore have poor wall-clock
latency even when its numeric work is tiny.

Before deployment for multiple users, design a bounded worker/process queue with
explicit overload responses and cancellation. Session/cache state needs safe
ownership or serialized commits; simply running the shared store concurrently
is insufficient. Preserve variable/random snapshots and stale revisions.

### 5. Keep resource limits explicit

Increasing precision can encounter cumulative allocation or output limits before
the deadline. Cumulative requested bytes measure allocation traffic, not live RAM.
Present time, allocation and output failures distinctly. Keep the five-second
default while optimizing; consider an explicit asynchronous job mode later.

Also evaluate a bounded exact-component eligibility check for complex Auto output,
analogous to small real fractions. When the exact text would be excessive, format
the unchanged stored value with the requested decimal/scientific precision.
Full/explicit exact output must keep its stated semantics and size-limit error.
Independent display references and existing complex-format contracts must guide
this separate behavior proposal.

## Validation

The new smoke test checks timing-only oracle enforcement, missing memory columns,
successful complete-pipeline output, mathematical failures, invalid arguments and
rejection of corrupted expected results. Performance thresholds are excluded from
CI. Existing benchmark smoke suites still exercise memory/phase bookkeeping.

All six applicable MSVC Release benchmark smoke CTests passed. A separate GCC
16.2 Release build with warnings-as-errors passed the changed decimal harness,
materialization experiment and complete-pipeline smoke tests (3/3). UTF-8 constant
input, a 4,097-byte oversized input and exact quantity output are included in the
new harness test. No browser/runtime UI change requires a browser rerun here.

The materialization harness also measures a private limb-copy/normalization
experiment, with fresh object construction and exact round-trip checks outside
timing. It excludes the final public atomic commit and is not a production
implementation or a prediction of complete-request latency.

No production optimizer or behavior change is included in this audit.
