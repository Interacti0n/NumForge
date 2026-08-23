# BigDecimal design

`BigDecimal` will provide exact base-10 values on top of `BigInt`, without
using binary floating point. It is deliberately not part of the public
function API yet: the representation and arithmetic rules below should be
implemented and tested together before declaring a stable interface.

## Representation

Each value is represented as:

```text
value = coefficient × 10^(-scale)
```

- `coefficient` is an owned `BigInt *`.
- `scale` is a signed `int64_t`.
- A positive scale represents fractional decimal places: `123 × 10^-2` is
  `1.23`.
- A negative scale represents trailing whole-number zeroes: `12 × 10^2` is
  `1200`.

The internal struct lives in `src/bigdecimal/bigdecimal_internal.h`. Keep it
private: consumers must only see the opaque `BigDecimal` type.

## Canonical form

Every successful public operation must leave a value normalized:

1. `coefficient` is never `NULL`.
2. Zero is exactly `coefficient == 0, scale == 0`.
3. For non-zero values, the coefficient is not divisible by ten.
4. Parsing, arithmetic, and rounding all finish by normalizing once.

For example, all of `1.2300`, `12300e-4`, and `123 × 10^-2` normalize to a
coefficient of `123` and a scale of `2`.

This avoids multiple in-memory forms for the same number, simplifies equality
and hashing later, and prevents scales from growing unnecessarily.

## Arithmetic rules

### Parsing and formatting

`set_string` should accept an optional sign, decimal point, and an optional
decimal exponent (`e` or `E`). It should reject whitespace and malformed
numbers. Parse into a temporary object and commit only after success, matching
the `BigInt` error-safety rule.

Formatting should produce ordinary decimal notation by default. Scientific
notation can be a separate formatter later; it must not change the stored
value.

### Addition and subtraction

Align both operands to the larger scale by multiplying the lower-scale
coefficient by a power of ten, then add or subtract the coefficients. Reject a
scale difference or allocation that cannot be represented with
`BIGDECIMAL_VALUE_TOO_LARGE`.

### Multiplication

Multiply coefficients and add scales, checking `int64_t` overflow before the
operation. Normalize the result once.

### Division

Division cannot generally be exact, so its first public API must require both
a target scale (or precision) and a rounding mode. Do not use a global mutable
precision setting.

The initial rounding modes should be:

- toward zero;
- away from zero;
- toward negative infinity;
- toward positive infinity;
- half up;
- half even.

The result should return `OK` when rounding succeeds. If callers need to know
whether information was discarded, expose that as an explicit output flag or
a context/trap option later rather than treating it as a generic error.

## Error model

Introduce a dedicated `BigDecimalStatus` enum when lifecycle and parsing are
implemented. It should mirror the useful `BigIntStatus` cases without exposing
`BigInt` internals:

- success;
- null argument;
- out of memory;
- invalid input;
- division by zero;
- value or scale too large.

All mutating operations should provide the same strong guarantee as
`bigint_set_string`: on failure, their destination is unchanged.

## Implementation phases

1. **Foundation:** `create`, `destroy`, `copy`, normalization, parsing, and
   plain decimal formatting. Add focused tests for signs, zero, exponent
   notation, and canonical form.
2. **Exact arithmetic:** comparison, negate, add, subtract, and multiply.
   Add property tests such as `(a + b) - b == a`.
3. **Division:** target scale/precision, all rounding modes, tie cases, and
   sign combinations. Test against generated decimal reference vectors.
4. **Optimization:** cache small powers of ten within an operation, avoid
   string conversions internally, and only add narrow `BigInt` helpers after
   profiling identifies a bottleneck.

## BigInt boundary

Use the public `BigInt` API in the first version. This keeps the layers
independent and prevents `BigDecimal` from relying on limb layout. Internally,
build a reusable `BigInt` value for ten and use `bigint_div_mod` during
normalization. If profiling later shows repeated decimal scaling is expensive,
add a small, well-tested `BigInt` helper for multiplication or division by a
`uint64_t`; do not expose the `BigInt` limb representation.
