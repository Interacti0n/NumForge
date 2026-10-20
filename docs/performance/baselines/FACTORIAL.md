# Split factorial scratch, 12 October 2026

This records the final bounded factorial experiment and checked admission.
Splitting scratch helps
capacity, without demonstrating a significant arithmetic speedup. Production
factorial now uses it; additional factorial tuning is deferred in favor of
overall regression/UX/CI review and release preparation.

## Change and bounds

Each Karatsuba depth previously owned one geometrically grown frame containing
operand sums and their cross product. It now owns two equal allocations: the
first stores both sums, the second the cross product. Combined capacity and
geometric growth remain the same; each allocation is half as large. Depths
remain independent, so recursive child products preserve parent sums.

The workspace records combined limb capacity. Capacity is published only after
both reallocations succeed; a failure after the first resize retains both owned
pointers for cleanup. Public destinations are still committed atomically.
Every exit frees both arrays, and existing cancellation checkpoints remain.
The layout flag is fixed while a workspace owns buffers. Generic contiguous
Karatsuba is unchanged; private unsplit adapters remain for reproducible comparisons.

The cumulative admission bound is unchanged. The largest scratch request bound
is halved, but the final product buffer is still included in the maximum.
No installed C API, counter bound or application limit changes: five seconds,
64 MiB cumulative requests, 512 KiB per allocation, 65,536 output bytes.
The conservative fresh-budget tree admission boundary moves from 234,536 to
247,572. This is an algorithm admission boundary, not a new factorial input cap.

At 250,000, forced split-tree arithmetic now completes under the ordinary
allocation budget. Its complete exact integer occupies 515,296 live bytes.
However, the checked final-product upper bound remains 529,752 bytes, so public
calculator admission still chooses scalar factorial. This experiment does not
claim calculator success at 250,000 or tighten the product estimate further.
At 255,000, forced split arithmetic hits the actual single-allocation limit.

## Measurements

Windows x64, Ryzen 7 7435HS, MSVC 19.51 Release; baseline numeric code `5599b26`.
Independent complete Python factorial fixtures are generated outside timed work.
Three serial cold processes use forward/reverse/forward order, without concurrent
builds/tests, affinity or frequency lock. Raw timings exclude text formatting
and application budgets; each complete integer is independently compared.

| n | Unsplit raw median, ms | Split raw median, ms |
| ---: | ---: | ---: |
| 100,000 | 174.864 | 174.354 |
| 230,000 | 774.307 | 761.656 |
| 235,000 | 782.664 | 797.972 |
| 245,000 | 802.805 | 799.627 |
| 250,000 | 813.186 | 842.455 |
| 255,000 | 834.151 | 884.768 |

Small differences do not establish a universal speed benefit. Cumulative scratch
bytes are unchanged; allocation calls increase (for example 8,233 to 8,275 in
the 100,000 raw adapter). Memory tracking is separate from timing. The decision
to adopt is based on bounded allocation size and practical admission gain.

Complete calculator medians below include ten-place half-even scientific output
and all application budgets. Each new result is compared byte for byte with an
independently rounded complete integer. Three serial new processes are recorded;
the saved baseline has one diagnostic process per input:

| Expression | Baseline outcome | New complete median, ms |
| --- | --- | ---: |
| `230000!` | Success, 1,562.837 ms | 1,540.810 |
| `235000!` | Time limit | 1,563.849 |
| `245000!` | Time limit | 1,680.330 |
| `247572!` | Time limit | 1,690.919 |

Baseline failures are cancellation outcomes, not completed timings. Separate
ordinary-budget raw checks: split 250,000 matches the entire reference in
804.698 ms and requests 11,598,192 cumulative bytes; split 255,000 fails with
memory reason after about 555 ms. Neither observation includes calculator projection.

## Verification

- Initial experiment: 52 complete unsplit/split references, including leaf and
  dispatch boundaries, fresh/aliased destinations, and inputs through 255,000.
- Final MSVC and GCC: 30 complete split references per compiler, through 255,000
  and both sides of the 247,572/247,573 admission boundary, under their checked
  tree memory plans. Diagnostic plan budgets can exceed application limits for
  rejected inputs; ordinary-budget success/failure is checked separately.
- Final complete multiplication oracles: 5,200 cases on each native compiler,
  with split/unsplit workspaces, odd/unequal magnitudes, signs, all aliases and
  deliberate corrupt-reference rejection.
- Allocation/deadline injection exhausts both split reallocations, subsequent
  growth and fresh/alias result paths; tracked live allocations return to baseline
  on failures and zero after cleanup. Public factorial failures preserve callers.
- Updated admission tests preserve exact budget boundaries, non-consuming
  fallback and overflow rejection. Nineteen selected CTest suites passed on each
  native compiler; ten passed on each Win32/portable division build. All four
  full builds and the ordinary noninstrumented app build passed.
- A temporary loopback HTTP server matched independent outputs for 235,000,
  245,000 and 247,572 factorial, committed the last into a fresh session and
  verified `ans` and compact history. Ordinary CLI process regressions passed.
  The user's existing running server/tunnel was not restarted.

Local ignored `build/split-workspace` retains complete fixtures, per-process
CSV/JSON samples, executable/fixture hashes and baseline outcomes. For reproduction:

```powershell
python benchmarks/generate_karatsuba_factorials.py build/split-full.tsv --workspace --split
build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/split-full.tsv --validate --check
python benchmarks/generate_karatsuba_factorials.py build/split-timing.tsv --workspace --split --timing
node benchmarks/run_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/split-recording --factorials build/split-timing.tsv
node benchmarks/check_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe --full
```

Build opt-in benchmarks/tests first; recording directory must be empty.
`tree_workspace` preserves the unsplit baseline, `tree_split` forces the current
layout, and `factorial` exercises production dispatch. Stress/admission fixtures
and their runner now use the current split adapter. Future larger algorithms or
tighter bounds remain optional research, not work required before the next release.

## Stress diagnostics

For a wider admission/normalization investigation, generate
`generate_karatsuba_factorials.py build/factorial-stress.tsv --workspace --stress`
and run `run_factorial_stress.cjs` with the calculator-limit executable, BigInt
benchmark executable, fixture file and a fresh output directory, in that order.
`calculator_limit_benchmark --phases` separates evaluation/formatting;
`--phases-profile` adds diagnostic normalization/core clocks. Run those clocks
separately from ordinary timing. Complete output projection and application
limits remain part of a calculator success; raw integer completion alone is
insufficient. Earlier boundary-tuning recordings remain in Git history.
