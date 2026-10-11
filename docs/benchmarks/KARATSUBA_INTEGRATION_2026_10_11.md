# Budget-aware Karatsuba integration, 11 October 2026

This report records the initial multiplication integration. The subsequent
[factorial tree integration](FACTORIAL_TREE_INTEGRATION_2026_10_11.md) implements
the workspace follow-up described below; timings here remain historical.

This follows the [private experiment](KARATSUBA_EXPERIMENT_2026_10_11.md).
`bigint_mul` now selects Karatsuba when the smaller magnitude has at least
128 radix-2^64 limbs and is greater than half the larger magnitude. Recursive
leaves use the classical kernel below 32 limbs. Small inputs and ratios at
least 2:1 keep the existing classical implementation.

The 128-limb entry point is conservative. GCC sometimes favors 16-limb leaves;
the common 32-limb cutoff performed well on both native compilers. These are
private implementation choices, not public numeric limits or promises of an
optimal cutoff on every machine.

## Workspace and failure contract

The kernel is shared by production and the benchmark threshold adapter. Each
call owns a fresh result and one scratch arena; recursion allocates nothing.
The scratch bound is the sum of `4*h`, with `h=ceil(n/2)+1`, over recursive
levels. Checked size arithmetic precedes allocation. Each child uses disjoint
result slices, with reusable arena slices for sums and the middle product.

An internal, non-consuming budget preflight checks both allocation sizes and
their sum against the current thread-local single/cumulative limits. If the
optional arena does not fit, dispatch uses the old classical path without
setting a sticky budget failure. Allocations are still charged normally after
selection. A real allocation failure or deadline expiry returns immediately;
it does not retry after poisoning the budget. All destinations/aliases remain
unchanged on failure, and all owned scratch is released.

This preserves result-only scopes but does not eliminate the extra cumulative
allocation traffic of recursive multiplication. High-level compositions need
their own resource checks. The calculator keeps its 5-second, 64-MiB cumulative,
512-KiB single-allocation and 65,536-byte output limits. No public C or HTTP API
changes are required. Production factorial still uses grouped scalar factors;
the experimental product tree remains separate.

## Recording protocol

Recordings are retained under ignored `build/karatsuba-integration`. The GCC
pre-integration matrix has 1,440 exact product cases, including balanced/square,
3:2, 4:3, just-under-2:1, exactly-2:1 and more unequal operands in both orders.
Focused recordings have 150 cases at 64/128/256/1024/4096 limbs, using classical
public multiplication and recursive cutoffs 32/64. All expected products come
from Node BigInt and are compared in full outside the calculation clock.

Three serial processes run forward/reverse/forward. Each uses three warmups,
three calibrated samples and a separate tracked-memory call. Tables use the
median of the three per-process medians. Metadata records executable/fixture
hashes, source revision and machine details. The before executables are retained
separately so rebuilding does not replace the comparison implementation.

Environment: Windows x64, Ryzen 7 7435HS, Release MSVC 19.51 / GCC 16.2. No
affinity or frequency lock; ordinary desktop activity remains a source of noise.
Builds and tests do not overlap speed recordings. Source/document edits and
brief recording inspection occurred during the initial GCC matrix.

## GCC multiplication crossover

Times below are milliseconds, using the pre-integration public classical path
and the shared kernel's 32-limb experimental entry.

| Limbs, balanced | Classical | Karatsuba32 |
| ---: | ---: | ---: |
| 128 | 0.0947 | 0.0597 |
| 256 | 0.3690 | 0.1533 |
| 1024 | 6.0725 | 1.3569 |
| 4096 | 99.1249 | 12.6749 |
| 8192 | 399.9016 | 38.1879 |

At 4096 x 2730 limbs (about 3:2), 65.998 ms becomes 16.560 ms. At
4096 x 2049 (just under 2:1), 49.281 ms becomes 8.370 ms. Exactly 2:1
does not recurse and is excluded by the production dispatcher. The entry
condition applies to the **smaller** operand, including these shapes.

The focused MSVC pre-integration measurements likewise favor Karatsuba32:
balanced 128 limbs, 0.1082 to 0.0622 ms; 256, 0.5176 to 0.2003 ms;
1024, 7.2747 to 1.9677 ms; 4096, 117.9530 to 16.9496 ms.
At 4096 x 2730, 79.619 ms becomes 22.062 ms; at 4096 x 2049,
59.506 ms becomes 11.146 ms. Both compilers support the selected shape guard.

## Production dispatch measurements

The post-integration focused recordings verify the selected path through the
actual public `bigint_mul`, with matching inputs/protocol to the before records:

| Balanced limbs | MSVC before ms | MSVC after ms | GCC before ms | GCC after ms |
| ---: | ---: | ---: | ---: | ---: |
| 64 | 0.0273 | 0.0266 | 0.0260 | 0.0233 |
| 128 | 0.1082 | 0.0587 | 0.0947 | 0.0578 |
| 256 | 0.5176 | 0.1966 | 0.3690 | 0.1403 |
| 1024 | 7.2747 | 1.8026 | 6.0725 | 1.3169 |
| 4096 | 117.9530 | 15.8936 | 99.1249 | 12.8885 |

Each post-integration matrix validates 150 exact values in each of three
processes. The 64-limb products still allocate one buffer; selected recursive
products allocate two. At 4096 balanced limbs, production requests 196,576
bytes instead of 65,536. A 4096 x 2048 product remains one allocation (49,152
bytes); 4096 x 2049 selects recursion (180,200 bytes). The small-case variations
do not justify lowering the conservative entry threshold.

A separate MSVC `--budget` probe validates a complete 16,383 x 16,383-limb
product against Node BigInt. The result needs 262,128 bytes, but the proposed
arena needs 524,384, exceeding the actual 512-KiB single-allocation cap. Dispatch
falls back before charging scratch: the operation succeeds in about 1.824 s,
with one 262,128-byte allocation and no budget failure. The separate memory
call also passes the complete reference and releases its tracked allocations.

## Correctness and portability

Native MSVC passes 57 CTest suites excluding the separately labeled complex
allocation shards, including numeric/property/oracle, application/session,
web/API, fuzz smoke, memory-ledger and all AGM interval fault suites. GCC passes
14 selected production/benchmark suites. Win32 MSVC and forced-portable GCC
each pass eight multiplication/core/allocation suites. Full product validation
on each native compiler passes 3,864 complete Node/Python reference cases;
these include entry/recursive boundaries, odd splits, dense/sparse limbs, signs,
unequal operands, aliases and small grouped factorial-tree comparisons. Invalid
fixture inputs and a corrupt exact reference are rejected.

Each native compiler also passes 960 complete public-logarithm references
through 5,000 digits across all rounding modes, domains, aliases and deadlines.
Those diagnostic reference runs use 60-second deadlines; actual five-second
calculator probes are reported separately below. Regression builds/tests run
concurrently, and their elapsed times are not speed measurements.

The production and forced-threshold fault harnesses exhaust allocation failure
positions and deadline checkpoints with live-allocation cleanup checks. The
normal allocation-failure suite additionally verifies the independent radix
identity and budget fallback described below, even without benchmark targets.

All four configurations build the production code with diagnostics/warnings as
errors. The complete Win32 build exposed an existing diagnostic-tool mismatch:
`calculator_limit_benchmark` used uint64 format specifiers for size_t counters.
It now uses `%zu`; a whole-output `1+2` reference confirms the Win32 harness.
The normal CLI/web build also compiles with testing and benchmarks disabled.

## Complete calculator resource probes

Three serial cold processes per case run forward/reverse/forward, Plain
notation, through `calculator_limit_benchmark`, before and after integration.
All have the actual five-second/64-MiB/512-KiB/output policies and succeed with
budget reason `none`. `ln(2)` compares all 5,002 output bytes against the retained
independent Decimal reference; other cases are resource/timing diagnostics,
with mathematical references covered separately by the regression families.
Requested bytes are cumulative allocation traffic, not live memory or RSS.

| Expression | Places | Before median ms | After median ms | Before bytes | After bytes |
| --- | ---: | ---: | ---: | ---: | ---: |
| `ln(2)` | 5000 | 1098.641 | 1074.317 | 40385440 | 41346080 |
| `ln(1+i)` | 2000 | 386.491 | 397.015 | 26882787 | 26882787 |
| `exp(1)` | 2000 | 177.134 | 176.038 | 4766368 | 4766368 |
| `exp(1)` | 5000 | 2622.031 | 2628.746 | 29335487 | 29343583 |
| `sin(1+i)` | 2000 | 586.211 | 600.083 | 31772320 | 31772320 |
| `root(2;7)` | 5000 | 202.710 | 137.369 | 2145491 | 3207955 |

The root improves about 32%; other selected compositions change little relative
to normal variation. Large-product speedups do not imply an equivalent speedup
of whole logarithms/exponentials. Extra arena traffic remains well within the
budget in these cases; this does not prove that every possible composition fits.
Old dated reports used other measurement conditions and should not be treated
as paired baselines for this table. Raw records include executable hashes.

## Reproduction

```text
node benchmarks/run_karatsuba.cjs path/to/bigint_arithmetic_benchmark.exe build/new-recording --focused
node benchmarks/check_karatsuba.cjs path/to/bigint_arithmetic_benchmark.exe --full
ctest --test-dir build/factorial-bench -C Release -R "multiplication_fault|karatsuba|allocation_failure" --output-on-failure
```

Enable `NUMFORGE_BUILD_BENCHMARKS=ON` for measurement/fault harnesses. The normal
allocation-failure suite also tests the production dispatch boundary using the
independent radix identity `(B^128-1)^2=B^256-2*B^128+1`. It covers all aliases,
both allocation failures, every deadline checkpoint, and result-only scopes
with cumulative/single limits and prior budget consumption.

## Next separate step

Design a factorial product-tree workspace that respects the existing allocation
limits, then revalidate complete factorial values and the calculator/HTTP
pipeline. The earlier unrestricted tree timings do not demonstrate successful
bounded requests or `1000000!` completion.
