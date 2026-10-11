# Integer conversion and combinatorics — 11 October 2026

Follow-up to [the cross-layer audit](PERFORMANCE_AUDIT_2026_10_10.md).
Measurements use the same Windows PC and MSVC Release benchmark build.
Production limits remain unchanged: 5000 ms, cumulative allocation budget,
single-allocation cap and output cap. No running server was replaced.

## Changes

- `bigdecimal_from_bigint` copies the exact coefficient into a private decimal,
  normalizes it and commits atomically, avoiding full decimal serialization.
- Calculator formatting reuses an explicitly verified exact integer projection.
  Rounded rational projections and integers produced through other evaluation
  paths keep their existing conversion path. Exact projections survive copies.
- Permutations/combinations multiply their private accumulator in place when
  the factor fits one limb. Combinations divide in place for one-limb divisors
  and check zero remainder. Larger factors/divisors retain generic operations.

## Measurements

Three serial samples per row unless noted. Times are elapsed complete API calls,
except the isolated conversion row. Calculator uses Auto with 10 output places.
Allocation bytes below are cumulative requests, not peak live memory.

| Case | Audit baseline | After changes |
| --- | --- | --- |
| Convert stored `100000!` to BigDecimal | 3.768–3.802 s; 22 allocations, 1,892,255 bytes | 0.492–0.510 s; 3 allocations, 262,192 bytes |
| Complete `100000!` | 11.598 s with extended deadline (one sample); timeout with 5 s | 4.803–4.837 s; succeeds with 5 s |
| `npr(10000;10000)` | Allocation budget exceeded in 61.7–63.0 ms | 34.8–39.3 ms; succeeds, 103,236 requested bytes |
| `npr(50000;50000)` | Allocation budget exceeded in 60.4–61.3 ms | 1.079–1.133 s; succeeds, 679,729 requested bytes |
| `ncr(10000;5000)` | 0.997–1.048 s; 16,329,429 requested bytes | 6.29–6.79 ms; 27,474 requested bytes |
| `ncr(100000;50000)` | Allocation budget exceeded in 4.474–4.512 s | 0.601–0.620 s; succeeds, 102,248 requested bytes |

After the conversion change alone, complete `100000!` took 5.138–5.494 s
in three serial extended-deadline samples. Reusing its exact projection removes
another conversion; final allocation requests fall to 1,341,227 bytes.
The final 5-second margin is only about 160–197 ms on this machine. This is
not a cross-machine guarantee or sufficient server headroom; thresholded
multiplication/product-tree work remains a candidate for subsequent measurement.

Raw local CSV files are in ignored `build/performance-direct-conversion-2026-10-11`.
Initial conversion and extended samples ran alongside compiler verification;
the isolated conversion rerun and serial calculator samples above replace those
initial observations. No speed threshold is added to CI.

## Verification

- MSVC: ten relevant CTest suites pass, including integer/decimal/calculator,
  complex numeric, session/application, unit web/HTTP and allocation failures.
- GCC: eight corresponding numeric/session/allocation suites pass.
- Conversion tests cover signs, zero, limb boundaries, trailing decimal zeros,
  exact round trips, null arguments and unchanged source/destination on failure.
- Every allocation position in the new conversion and combinatorial paths is
  injected; existing alias and domain tests remain enabled.
- Closed-form references cover factors below, at and above `2^64`.
- `check_combinatorial_optimization.cjs` validates nine complete exact outputs
  using independent JavaScript BigInt factorial quotients, including
  `npr(10000;10000)` and `ncr(100000;50000)`. Oracle generation is outside the
  measured C request. The large permutation exceeding the output cap is skipped.

Run the independent check after building benchmark targets:

```text
node benchmarks/check_combinatorial_optimization.cjs <calculator_limit_benchmark executable>
```
