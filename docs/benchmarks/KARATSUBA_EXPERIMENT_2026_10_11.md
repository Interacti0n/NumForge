# Private Karatsuba experiment — 11 October 2026

This is a benchmark-only experiment. Production `bigint_mul`, factorial,
calculator limits and the installed C API are unchanged. Earlier product-tree
measurements used classical products; this experiment tests faster products
before reconsidering the tree.

## Implementation and bounds

`benchmarks/karatsuba_experiment.c` splits magnitudes at `k=floor(max_size/2)`.
It computes `z0=a0*b0`, `z2=a1*b1` and
`z1=(a0+a1)*(b0+b1)-z0-z2`, then combines them at limb offsets 0, k and 2k.
Signs are applied only to the final normalized result. Small inputs and shapes
with the smaller operand at most half the larger use classical multiplication.
The leaves use the same private 64-by-64 product helper as production.

There are at most two allocations per multiplication: the unpublished result
and one per-call scratch arena. Child calls reuse arena slices, with no recursive
heap allocations. The arena bound uses `h=ceil(n/2)+1` and
`S(n)=4h+S(h)` until n is below the leaf threshold. Low/high products reuse the
arena before local sums and the cross product are allocated; their output slices
are disjoint. Checked size arithmetic bounds allocation requests. No global
scratch or compiler-specific 128-bit integer is introduced.

Cancellation checks cover recursive entry, classical rows, blocks of 256 inner
products, sum/subtraction/recombination loops and final publication. A failed
operation frees its scratch and preserves its destination, including either
input alias and a same-input square. Large work still requires an explicit
runtime budget; a complexity improvement is not a completion guarantee.

## Correctness and failure checks

The independent Node BigInt generator covers five leaf thresholds (16, 32, 64,
128 and 256), odd/even splits, threshold edges, carries, random/max/sparse/
alternating limbs, signs, zero, squares, reversed shapes and output aliases.
Native MSVC and GCC each pass 3600 complete product/factorial comparisons through
2049 limbs; the smoke matrix has 1920 cases. Win32 and portable GCC are checked
with the same smoke contract. A deliberately
corrupted recursive reference is rejected. Each measurement compares the whole
integer, not just residues or leading digits.

The separate deterministic fault check passes 2622 invocations, exhausting
allocation failures and every deadline checkpoint in representative recursive,
aliased and long-inner-loop unbalanced calls. It also checks cumulative and
single-allocation budget rejection. Failures preserve the original value;
tracked scratch returns to baseline and final tracked live storage is zero.
MSVC and GCC additionally pass all 90 Python factorial references through
200000, including small-input aliases and all private tree thresholds. Those
correctness runs were concurrent and are not used for speed comparisons.
All 900 timing fixtures through 8192 limbs also pass whole-value validation in
the fast hex ingestion format. Signed/zero hex values, invalid digits and empty
hex payloads have explicit protocol regressions.

## Measurement method

MSVC Release on the same Windows x64 Ryzen 7 7435HS machine as the previous
reports. Three serial benchmark processes, forward/reverse/forward case order,
with three warmups and three calibrated samples per process. Tables report the
median of the three per-process medians. Fixtures, parsing, alias restoration,
full reference comparison, CSV output and live-memory probes are outside speed
timing. Allocation/free inside the multiplication is included.

The matrix contains 900 cases from 16 through 8192 limbs, balanced products,
squares, single-limb factors, 8:1 and 2:1 shapes, near-balanced odd shapes and
both operand orders. CPU affinity/frequency is not locked; measurements are
local evidence, not a CI speed requirement or a cross-machine promise.
Raw references, executable SHA-256, environment metadata, CSV and summaries
are retained in ignored `build/karatsuba-2026-10-11`.

No builds or test suites overlapped the multiplication recording. Ordinary
desktop activity and a brief fixture-count script were not excluded; interpret
small timing differences cautiously. The recorded executable predates the
factorial-harness extension; the multiplication implementation is identical.

## Multiplication results and proposed dispatcher

Median milliseconds, balanced random operands, leaf threshold 32:

| Limbs per operand | Production classical | Private Karatsuba | Speed ratio |
| ---: | ---: | ---: | ---: |
| 128 | 0.1142 | 0.0602 | 1.90x |
| 256 | 0.4392 | 0.1849 | 2.37x |
| 512 | 1.7388 | 0.5717 | 3.04x |
| 1024 | 7.3579 | 1.7553 | 4.19x |
| 2048 | 29.7171 | 5.5548 | 5.35x |
| 4096 | 118.1299 | 16.4679 | 7.17x |
| 8192 | 482.0753 | 50.2117 | 9.60x |

At 8192 limbs the classical call requests 131072 bytes once. Karatsuba32 requests
393248 bytes across two allocations; the separate live probe reports the same
additional peak payload above the existing input/output baseline. This excludes
allocator overhead and process RSS. Same-input squares at 8192 limbs take
622.1845 versus 59.1244 ms (10.52x); this is ordinary multiplication with the same
input, not the older diagonal-square experiment.

Thresholds 16 and 32 are competitive across the large range; 32 is faster in the
recorded 128/512/2048/4096/8192 balanced medians. Small 32/64-limb gains vary
between runs. Proposed initial integration: enter Karatsuba conservatively from
128 limbs in the smaller operand, then recurse with a 32-limb leaf threshold.
Retain the existing production path for small/unbalanced inputs rather than
routing them through the experiment's classical leaf. The experimental leaf
adds more inner-loop cancellation checks and is about 10% slower in the measured
large 8:1 and 2:1 shapes; Karatsuba is not selected for those shapes.

The matrix establishes balanced and near-balanced gains, not an optimal ratio
cutoff for all intermediate shapes. Before production integration, time 3:2 and
just-under-2:1 cases, confirm the crossover on another compiler, and decide the
shape guard using those results. Correctness references already include those
ratios. Thresholds are implementation choices, not changes to the public API.

## Product-tree factorial follow-up

After the multiplication recordings, compare the unchanged production grouped
factorial with a private product tree using 32-factor leaves. Compare the same
tree with classical versus Karatsuba products so leaf grouping and fast
multiplication are not conflated. The old single-factor tree remains available
as the existing `tree` operation. All variants keep the destination unchanged
on failure, including aliases; the private experiment allocation check now
also exhausts grouped-tree allocation failures.

Three serial processes, one complete operation per case per process, no warmup;
median milliseconds below. Separate live-memory calls and reference parsing are
excluded. Python `math.factorial` supplies every complete result, ingested as
`hex:` limbs to avoid decimal fixture-parsing overhead. Final decimal display,
calculator materialization, HTTP and production budgets are not part of these
unrestricted speed measurements.

| Input | Production grouped factorial | Grouped tree, classical | Tree, Karatsuba16 | Tree, Karatsuba32 |
| ---: | ---: | ---: | ---: | ---: |
| 10000! | 8.336 | 12.278 | 5.436 | 6.043 |
| 50000! | 278.174 | 441.952 | 84.563 | 96.154 |
| 100000! | 1428.306 | 2093.438 | 289.439 | 322.827 |
| 200000! | 6898.962 | 9634.092 | 930.130 | 1061.331 |

Classical products still make the tree slower. Karatsuba changes that result;
the 16-limb leaf wins this particular factorial workload. Do not assume one
threshold is optimal for both generic multiplication and factorial trees.

## Actual application-budget probes and next work

Separate serial probes use `--budget`: 5000 ms, 64 MiB cumulative requests,
512 KiB per allocation. The harness forces single-call validation and begins a
fresh scope around each operation, excluding fixture ingestion. It compares a
successful complete value and performs a separate live-memory call.

| Input | Production factorial | Private tree, Karatsuba32 |
| ---: | --- | --- |
| 100000! | Exact reference passes | Exact reference passes |
| 150000! | Exact reference passes | Memory budget rejection, about 0.412 s |
| 200000! | Time budget rejection, about 5 s | Memory budget rejection, about 0.687 s |

The large tree scratch request crosses the single-allocation limit; unrestricted
CPU timings therefore cannot be promoted as successful calculator requests.
The calculator/HTTP formatting pipeline is not changed or measured by these
private probes. Keep the current production factorial until the tree has a
compatible workspace plan and passes complete application tests.

Recommended next steps:

1. Complete focused compiler and intermediate-ratio crossover measurements.
2. Integrate a private, thresholded multiplication path only after preserving
   public aliases, atomic failure and the existing application memory budgets.
   Re-measure high-precision decimal/complex compositions, not just raw products.
3. Design bounded/chunked or reusable tree scratch, with preflight planning and
   a safe small-input/current-factorial path. Do not raise limits merely to fit
   this experiment. Revalidate the complete calculator/HTTP pipeline before
   promoting the tree.

No result here establishes `1000000!` completion, full decimal output under a
minute, or multiuser throughput. A larger input also needs its own result-buffer,
memory-budget and output-size analysis.

## Reproduction

Enable `NUMFORGE_BUILD_BENCHMARKS=ON` and build `bigint_arithmetic_benchmark`.
`BUILD_TESTING=ON` additionally enables deterministic fault injection.

```text
ctest --test-dir build/factorial-bench -C Release -R "karatsuba|bigint_arithmetic_benchmark_smoke" --output-on-failure
node benchmarks/check_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe --full
node benchmarks/run_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/new-karatsuba-recording
python benchmarks/generate_karatsuba_factorials.py build/factorial-cases.tsv --timing
node benchmarks/run_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/new-factorial-recording --factorials build/factorial-cases.tsv
bigint_arithmetic_benchmark build/factorial-cases.tsv --budget --case tree_karatsuba32-100000-reuse
```

The recording directory must be empty. Finish builds and tests before speed
recording. `--validate` on the arithmetic harness makes one timing call and a
separate live-memory call per exact reference. Standard-library Python
`generate_karatsuba_factorials.py` prepares full factorial references through
200000 independently of the C implementation; these fixtures are diagnostic,
not changes to calculator input or output limits.
Timing fixtures now use the optional `hex:` ingestion format; decimal fixtures
remain supported and the recorded multiplication values are unchanged.
