# BigInt design

`BigInt` is NumForge's signed arbitrary-precision integer foundation. Its
public API is declared in `include/numforge/bigint.h`; this document records
the internal representation, semantic decisions, and maintenance boundaries.

## Representation and invariants

The magnitude is stored as a little-endian array of base-2^64 limbs:

```text
value = sign × sum(limbs[i] × 2^(64 × i))
```

The private `BigInt` structure maintains these invariants:

- `limbs` contains the absolute magnitude only;
- `size` is the number of significant limbs;
- if `size > 0`, the most significant limb is non-zero;
- `size == 0` represents zero;
- zero is never negative;
- `capacity >= size` and owns the limb allocation.

`bigint_normalize()` is the single source of truth for removing high zero
limbs and clearing a zero sign. Public code must never create a negative zero.

## Ownership and failure safety

`bigint_create()` returns an owned zero value and `bigint_destroy()` releases
it; destroying `NULL` is safe. `bigint_to_string()` allocates a C string owned
by the caller, which must release it with `free()`.

Mutating operations provide a strong failure guarantee: when they return an
error, their destination is unchanged. Operations that need temporary storage
compute into fresh buffers or temporary objects and commit only after success.

Arithmetic supports output/input aliasing unless documented otherwise:

```c
bigint_add(value, value, other);
bigint_mul(value, value, value);
bigint_div_mod(quotient, remainder, quotient, remainder);
```

The only exception is `bigint_div_mod`: quotient and remainder must be
different objects.

## Decimal conversion

Parsing accepts an optional sign followed by decimal digits. It parses into a
temporary value, so malformed input and allocation failures preserve the old
destination.

Formatting and parsing use chunks of up to 19 decimal digits because 10^19
fits in `uint64_t`. This reduces the number of full-magnitude operations
compared with processing one decimal digit at a time.

## Arithmetic semantics

- Addition, subtraction, multiplication, division, and modulo are signed.
- Division truncates toward zero.
- The modulo result has the dividend's sign, matching C `%` semantics.
- `0^0` is defined as `1`.
- Powers reject negative exponents.
- Factorial accepts `0 <= n <= BIGINT_FACTORIAL_MAX_N`; the limit prevents an
  unbounded resource request.
- GCD is non-negative; LCM is computed as `(|a| / gcd(a, b)) × |b|` to reduce
  intermediate growth.

## Bitwise operations

AND, OR, and XOR currently accept only non-negative operands. This avoids
pretending that a sign-magnitude representation has an implicit infinite
two's-complement width. `bigint_not` is defined algebraically for every value
as `-(a + 1)`. Shifts preserve the documented truncation-toward-zero behavior.

## Primality and squares

Primality uses Miller-Rabin witnesses that are deterministic for values fitting
in `uint64_t`; for larger values the result is a strong probable-prime result,
not a certificate. Perfect-square detection builds an integer square root bit
by bit and verifies the resulting square.

## Performance boundaries

The current multiplication and division implementations prioritize correctness,
aliasing safety, and portability. Likely future optimizations, after profiling,
are:

- faster multiplication algorithms for very large limb counts;
- specialized division for common small divisors;
- cached powers used repeatedly by higher-level decimal operations.

These must preserve the representation and public semantic contracts above.
