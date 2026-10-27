# BigInt design

Opaque public API: `include/numforge/bigint.h`. Implementation:
`src/core/bigint/`; its CMake manifest owns the source list.

## Source layout

Lifecycle/magnitude helpers live in `bigint.c`; conversion, comparison, arithmetic,
bitwise, number theory, primality and roots have separate files. Private
`multiplication.c`, `factorial.c` and `normalization.c` own bounded scratch paths.
`bigint_internal.h` never becomes consumer API.

## Representation and invariants

`value = sign * sum(limbs[i] * 2^(64*i))`: little-endian magnitude limbs,
`capacity >= size`, nonzero highest used limb. Zero has size zero and no negative
sign. `bigint_normalize` alone removes high zero limbs and clears negative zero.

## Ownership and failure safety

Create returns owned zero; destroy(NULL) is safe. Returned strings use `free()`.
Mutations compute privately and commit only on success. Output/input aliasing
is supported except quotient and remainder of `bigint_div_mod` must differ.
Allocation/deadline failure preserves destinations and aliases. Optional runtime
scopes check expensive loops; ordinary public calls have no implicit budget.

## Decimal conversion

Parse an optional sign plus digits into a temporary. Parsing/formatting use
19-digit chunks (`10^19` fits uint64). Formatting divides scratch magnitude
by that radix. GCC/Clang with `__int128` and MSVC x64 use native 128/64 division;
other targets retain a checked bit-serial helper with `high < divisor`.

## Arithmetic semantics

- Signed division truncates toward zero; remainder has the dividend's sign.
- Multi-limb division uses normalized radix-2^64 long division, quotient
  correction and add-back. Inputs are borrowed; fresh outputs commit together.
- `0^0=1`; negative integer exponents are rejected.
- Factorial's representation bound is `UINT64_MAX`; budgets govern practical
  work. From 8192, conservative bit-length/capacity preflight admits a balanced
  product tree with per-depth scratch; constrained/small cases use grouped
  scalar multiplication. uint64 leaf products check overflow before multiplying.
  Checkpoints cover factors, limb blocks and final publication.
- Permutations/combinations require `0 <= r <= n`; combinations use symmetry
  and exact divisions rather than full factorial intermediates.
- GCD is nonnegative and rotates reusable remainder/division buffers. LCM uses
  `(|a|/gcd(a,b))*|b|` to limit growth.

## Bitwise operations

AND/OR/XOR require nonnegative operands; sign-magnitude has no implicit infinite
two's-complement width. NOT is `-(a+1)`. Shifts follow documented truncation rules.

## Primality and squares

Miller–Rabin witnesses are deterministic for uint64 values; larger results are
probable primes, not certificates. Square detection verifies an integer root.
Status and boolean outputs are separate and preserved on failure.

## Performance boundaries

Karatsuba dispatch requires at least 128 limbs in the smaller operand and a
size greater than half the larger; recursion leaves use classical multiplication
below 32 limbs. One per-call arena supplies scratch; no global/recursive
allocations. If preflight cannot fit optional scratch, classical work remains.
Factorial splits equal sum/cross buffers to bound individual allocations.
[Evidence](../performance/baselines/ARITHMETIC.md) ·
[Factorial scratch](../performance/baselines/FACTORIAL.md).

Future changes need independent correctness/failure tests and measured benefit;
candidate areas are repeated-product scratch and divide-and-conquer conversion.

## Explicit numeral bases

Bases 2–36 use ASCII digits and explicit output case. Parsing validates the full
input before committing; maximal uint64 radix chunks serve both directions.
Checked sizes/budgets bound buffers. Decimal entry points keep 19-digit chunks.
Independent BigInt references and allocation faults cover both conversions.
