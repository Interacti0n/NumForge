# Factorial admission boundary and normalization, 11 October 2026

Follow-up to the [product-tree integration](FACTORIAL_TREE_INTEGRATION_2026_10_11.md).
This change adds benchmark diagnostics and references; production algorithms,
the public C API and calculator limits are unchanged.

## Complete calculator requests

Windows x64, Ryzen 7 7435HS, MSVC 19.51 Release, production numeric code from
`b95f8f6`. Ten scientific decimal places, half-even rounding, five seconds,
64 MiB cumulative allocation requests, 512 KiB per allocation. Measurements
ran serially without concurrent builds/tests and without affinity/frequency
lock. Independent Python integer references were converted and rounded by
Node before timed work; successful outputs were compared byte for byte.

The ordinary `calculator_compute` pipeline ran once per input. A phase mode
then ran three cold processes in forward/reverse/forward order, performing
the same value computation, formatting and destruction inside one shared
production budget. These are phase-mode medians; evaluation includes parsing,
exact arithmetic and decimal projection, rather than only BigInt factorial:

| n | Complete request, ms | Evaluate, ms | Final formatting, ms | Outcome |
| ---: | ---: | ---: | ---: | --- |
| 200,000 | 1,872.073 | 1,871.911 | 0.093 | Success |
| 225,000 | 2,354.006 | 2,353.717 | 0.091 | Success |
| 230,000 | 2,454.196 | 2,453.999 | 0.102 | Success |
| 233,000 | 4,992.593 | 4,992.593 | Not reached | Time limit |
| 234,000 | 4,991.170 | 4,991.170 | Not reached | Time limit |
| 250,000 | 4,987.864 | 4,987.864 | Not reached | Time limit |
| 300,000 | 4,992.209 | 4,992.209 | Not reached | Time limit |

The ordinary pipeline had the same success/failure outcomes; successful whole
times were 1,853/2,411/2,450 ms. Failed times are cancellation observations,
not completed calculations. Small differences around five seconds reflect
the production deadline clock and checkpoints, not a different configured limit.
230,000 is the largest successful input in this matrix, not a proven maximum.

## Admission cliff versus real memory constraints

The conservative single-allocation plan changes from 512 KiB to 1 MiB near
233,000, so public factorial switches back to grouped scalar multiplication.
The cumulative plan still fits. The resulting time failures occur during
evaluation, before any final display conversion.

Separate budgeted BigInt diagnostics force the workspace tree without the
optional preflight. Every successful complete integer matches Python:

| n | Public BigInt, ms | Forced workspace tree, ms | Tree requested bytes | Peak additional live bytes |
| ---: | ---: | ---: | ---: | ---: |
| 200,000 | 611.844 | 604.566 | 7,952,752 | 1,855,928 |
| 225,000 | 806.155 | 790.927 | 8,712,808 | 1,967,552 |
| 230,000 | 814.304 | 819.216 | 8,861,096 | 1,990,816 |
| 233,000 | Time limit | 798.441 | 8,969,704 | 2,004,200 |
| 234,000 | Time limit | 794.660 | 9,010,272 | 2,010,688 |
| 250,000 | Time limit | Allocation limit | — | — |
| 300,000 | Time limit | Allocation limit | — | — |

These are single cold timing observations followed by separate tracked memory
invocations, not three-run speed comparisons. Peak values exclude the eight-byte
input baseline and are library live buffers, not process RSS. Forced tree at
250,000/300,000 reports memory rejection after about 557/779 ms; these are
failure timings. No complete calculator success is claimed for these inputs.

The workspace's largest frame depends on the larger partial product, not just
half the final result size: upper factors produce the larger half. Power-of-two
capacity growth can therefore cross 512 KiB even when the result itself would
fit. Simply removing admission checks would replace some time failures with
resource failures. A tighter bound could safely admit 233,000/234,000; it must
still account for unequal halves, rounding, geometric capacity and all requests.

## The successful path's main CPU cost

Two separate clock-instrumented observations used existing exclusive decimal
profile hooks. They are diagnostic probes, separate from the timing medians:

| n | Complete request, ms | Decimal normalization, ms | Final formatting, ms |
| ---: | ---: | ---: | ---: |
| 200,000 | 1,838.161 | 1,218.810 | 0.132 |
| 230,000 | 2,422.807 | 1,624.527 | 0.113 |

Normalization occupies roughly two thirds of these requests. Source inspection
identifies the mechanism: exact integer projection calls `bigdecimal_from_bigint`,
whose canonicalization calls `bigint_strip_decimal_zeros`. This repeatedly
scans/divides the coefficient by powers up to `10^19`. Factorials contain many
trailing zeros (49,998 for 200,000 and 57,497 for 230,000), requiring thousands
of full-magnitude passes. The measured normalization interval includes this
work; it is not the cost of converting all digits into a display string.

The final short scientific formatter already operates on the projected value
and takes about 0.1 ms here. Optimize normalization/projection first, rather
than replacing that formatter based on the complete request time.

## Recommended next work

1. Benchmark faster trailing-zero removal independently: larger exact power
   blocks and valuation-based strategies, including integers with no/few/many
   zeros. Preserve canonical decimal representation, scale checks, cancellation,
   aliases and destination preservation. Select an approach by measurement;
   a single very large long division is not automatically faster.
2. Tighten factorial admission using checked bounds for the two factor ranges
   and the actual frame growth rule. Regression-test the boundary and constrained
   scopes. Verify complete calculator/HTTP requests before claiming larger
   supported cases; do not raise the memory/time budgets to hide failures.
3. Re-run this matrix. At 250,000 the real scratch allocation needs attention
   too: consider bounded capacity growth after measuring its allocation traffic.
   One million remains outside the demonstrated application capacity.

Lazy exact-integer projection is another possible design, but typed values,
copies, sessions and approximate functions currently rely on the decimal
projection. It needs an explicit ownership/consumer audit before changing that
contract. A faster canonicalization helper can benefit the existing paths.

## Verification and reproduction

Stress fixtures contain 28 complete references through 300,000, covering public
and forced tree paths with fresh/reused and aliased destinations. Without an
application budget both paths complete; MSVC/GCC validation checks the entire
integer. Larger fixture records required increasing the benchmark's bounded
line buffer from 1 MiB to 8 MiB; production input/output limits did not change.
Ordinary/phase/profile benchmark modes also matched independently known integer,
fraction and complex outputs, error statuses and allocation metrics on both
compilers. Profile clock completeness was checked.
The existing Karatsuba-reference, BigInt-arithmetic and performance-audit smoke
suites passed on both MSVC and GCC. Regenerating stress fixtures reproduced the
same SHA-256, and the documentation link/anchor check passed.

```powershell
python benchmarks/generate_karatsuba_factorials.py build/factorial-stress.tsv --workspace --stress
node benchmarks/run_factorial_stress.cjs build/factorial-bench/Release/calculator_limit_benchmark.exe build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/factorial-stress.tsv build/factorial-stress-recording
build/factorial-bench/Release/calculator_limit_benchmark.exe '230000!' 10 2 5000 --phases-profile
build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/factorial-stress.tsv --validate
```

Configure/build the opt-in benchmark targets first; recording directory must be
empty. `--phases` adds evaluate/format columns; `--phases-profile` additionally
enables diagnostic normalization/core clocks. Default CSV remains unchanged.
Local ignored `build/factorial-stress-2026-10-11` records hold individual samples,
complete fixture/output references, executable hashes and the separate profile
CSV. The runner serializes all requests and does not adjust production limits.
