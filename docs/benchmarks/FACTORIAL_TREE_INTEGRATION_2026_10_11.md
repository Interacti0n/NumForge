# Bounded factorial product tree, 11 October 2026

Production `bigint_factorial` now uses a balanced product tree from `n = 8192`
when a conservative allocation plan fits the active budget. Smaller inputs or
plans that do not fit retain grouped scalar multiplication. This follows the
[Karatsuba integration](KARATSUBA_INTEGRATION_2026_10_11.md).

The public C API, aliases, counter range and calculator limits are unchanged:
five seconds, 64 MiB cumulative allocation requests, 512 KiB per allocation
and 65,536 output bytes. Numeric calls without an application budget retain
their existing unrestricted-time contract. This optimization introduces no
new factorial input cap and does not guarantee a particular input completes.

## Implementation and bounds

Leaves pack at most 32 consecutive factors into overflow-checked uint64
groups. Internal nodes multiply balanced partial products. A per-call workspace
retains separate Karatsuba scratch frames by recursion depth, growing each in
powers of two and reusing it throughout the tree. Recursive calls use deeper
frames, preserving the parent's sums and middle product. Small/unbalanced
products retain the ordinary multiplication dispatcher.

For `b = bit_length(n)`, the preflight bounds factorial magnitude by `n*b`
bits. With `B = 8*(ceil(n*b/64)+2)`, it reserves conservatively
`B*(b+8) + 8*n + 32768` cumulative bytes; the single-allocation bound is the
next power of two at least `B+64`. Checked size arithmetic rejects plans that
cannot be represented, including on Win32. Product buffers are bounded per
tree level; the additional terms cover leaf rounding, geometric frame growth
and additive scratch terms. The plan check consumes no budget and records no
failure. A rejected plan selects the scalar path rather than retrying after a
sticky resource failure. Actual allocations still enforce the same limits.

Each factor and expensive multiplication loop checks cancellation. Every
failure frees temporary products and workspace frames; only a successful final
result replaces the caller's destination. Input/output aliases remain valid.

The deliberately loose preflight can select scalar multiplication even when a
tighter estimate would admit the tree. Improving that estimate is separate work.
One million factorial was not measured or enabled specially; these results do
not demonstrate that it fits the current application budget.

## Measurements

Windows x64, AMD Ryzen 7 7435HS, MSVC 19.51 Release. Each table reports medians
of three serial cold processes in forward/reverse/forward order, without
concurrent builds/tests. No affinity or frequency lock was applied. Baseline
executables were retained from `d24bac1`; the new executables contain this
follow-up. Ignored local records under `build/factorial-tree-integration`
retain executable/fixture SHA-256 hashes and individual samples.

Raw BigInt factorial timings exclude parsing, copying, reference checking and
text output; no calculator budget is active:

| n | Grouped scalar, ms | Product tree, ms |
| ---: | ---: | ---: |
| 2,000 | 0.122 | 0.184 |
| 8,192 | 2.784 | 2.140 |
| 10,000 | 4.895 | 3.457 |
| 50,000 | 163.312 | 56.268 |
| 100,000 | 868.820 | 184.739 |
| 150,000 | 2,211.740 | 371.591 |
| 200,000 | 4,137.772 | 626.131 |

This supports the conservative 8192 entry threshold on the measured host.
For the 200,000 tree, a separate tracked invocation requested 7,952,752 bytes
in 16,435 allocations, with 1,855,928 bytes peak additional live allocation.
These metrics count library allocation requests/live buffers, not process RSS.

The following measurements cover the actual calculator request **including
rounded scientific output**, with all application budgets active, ten places
and half-even rounding:

| Expression | Previous calculator, ms | New calculator, ms | Exact integer decimal digits |
| --- | ---: | ---: | ---: |
| `100000!` | 1,132.019 | 460.425 | 456,574 |
| `150000!` | 2,744.603 | 1,029.953 | 711,273 |
| `200000!` | Time limit in all three runs | 1,834.463 | 973,351 |

The new 200,000 request allocated 9,546,255 cumulative bytes in 17,330 calls
and emitted 20 bytes. Its output was `1.4202253455E+973350`. The exact integer
remains stored; the short display does not truncate the numeric value. Raw
factorial time and complete calculator time are different measurements; this
record does not assign their difference to individual pipeline phases.

## Correctness and portability

- Complete Python `math.factorial` references: 84 rows per MSVC/GCC build,
  public/grouped/tree paths, fresh and aliased destinations, boundary inputs
  0/1/31/32/33/8191/8192/8193 and large inputs through 200,000.
- Complete independent Node BigInt products: 4,566 references on each compiler,
  including workspace multiplication, odd lengths, signs, sparse magnitudes,
  all supported aliases and intentionally corrupted-reference rejection.
- Every injected allocation failure for a forced 8192 tree preserves fresh or
  aliased output. Every injected cancellation checkpoint for a 2000 tree
  preserves the alias; tracked workspace checks return to the live baseline.
- A 64 KiB allocation budget rejects the tree plan without poisoning the scope
  and successfully evaluates public 8192 factorial through the grouped path.
- MSVC: 57 CTest suites passed. GCC: 14 selected suites; Win32 and forced
  portable GCC division: nine selected suites each passed. All four full
  builds passed, as did the ordinary app build without benchmark instrumentation.
- Existing high-precision logarithm/fault regressions passed; an independent
  5,000-place `ln(2)` calculator result also matched under the standard budget.
- Temporary loopback HTTP server: independent rounded outputs for
  100,000/150,000/200,000 factorial matched. A fresh session committed 200,000
  factorial, previewed `ans`, and returned one correct compact history entry.
  The existing user's server/tunnel was not changed.

## Reproduce

```powershell
cmake --build build/factorial-bench --config Release --parallel 6
python benchmarks/generate_karatsuba_factorials.py build/factorial-full.tsv --workspace
build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/factorial-full.tsv --validate
node benchmarks/check_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe --full
python benchmarks/generate_karatsuba_factorials.py build/factorial-timing.tsv --workspace --timing
node benchmarks/run_karatsuba.cjs build/factorial-bench/Release/bigint_arithmetic_benchmark.exe build/factorial-new-recording --factorials build/factorial-timing.tsv
ctest --test-dir build/factorial-bench -C Release -LE allocation --output-on-failure
```

Configure with testing, apps and benchmarks enabled first. The recording
directory must be empty. Python references are generated outside timed work.
Scientific-output references were independently obtained by decimal conversion
of Python's complete integer and Node integer-string half-even rounding.

Next candidates are tighter workspace admission estimates and further reduction
of projection/formatting allocation traffic, followed by a new complete-request
measurement. Further speedups should preserve cancellation and memory bounds.
