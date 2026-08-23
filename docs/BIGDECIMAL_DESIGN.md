# BigDecimal design

`BigDecimal` provides exact base-10 values on top of `BigInt`, without using
binary floating point. Its public API is declared in
`include/numforge/bigdecimal.h` and implemented in
`src/bigdecimal/bigdecimal.c`.

## Current API

The public header already reserves the stable surface of the component:

- lifecycle: `bigdecimal_create`, `bigdecimal_destroy`;
- conversion: `bigdecimal_copy`, `bigdecimal_set_string`,
  `bigdecimal_to_string`;
- comparison and inspection: `bigdecimal_compare`, `bigdecimal_is_zero`,
  `bigdecimal_is_negative`;
- exact arithmetic: absolute value, negation, addition, subtraction, and
  multiplication;
- controlled inexact operations: `bigdecimal_rescale` and `bigdecimal_div`,
  each accepting a target scale and `BigDecimalRoundingMode`.

All listed operations are implemented. Every mutating operation computes into
a temporary value and commits only on success, so its destination is unchanged
after an error. `BIGDECIMAL_NOT_IMPLEMENTED` remains reserved for future
optional API areas and is not returned by the current operations.

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

`set_string` accepts an optional sign, decimal point, and optional decimal
exponent (`e` or `E`). It rejects whitespace and malformed numbers. Parsing
uses a temporary object and commits only after success, matching the `BigInt`
error-safety rule.

Formatting produces ordinary decimal notation. Scientific notation can be a
separate formatter later; it must not change the stored value.

### Addition and subtraction

Addition and subtraction align both operands to the larger scale by multiplying
the lower-scale coefficient by a power of ten, then adding or subtracting the
coefficients. They reject a scale difference or allocation that cannot be
represented with
`BIGDECIMAL_VALUE_TOO_LARGE`.

### Multiplication

Multiplication multiplies coefficients and adds scales, checking `int64_t`
overflow before the operation. It normalizes the result once.

### Division

Division cannot generally be exact, so the public API requires both a target
scale and a rounding mode. It does not use a global mutable precision setting.

The implemented rounding modes are:

- toward zero;
- away from zero;
- toward negative infinity;
- toward positive infinity;
- half up;
- half even.

The operation returns `OK` when rounding succeeds. If callers later need to
know whether information was discarded, expose that as an explicit output flag
or context/trap option rather than treating it as a generic error.

## Error model

`BigDecimalStatus` mirrors useful `BigIntStatus` cases without exposing
`BigInt` internals:

- success;
- null argument;
- out of memory;
- invalid input;
- division by zero;
- value too large;
- scale overflow;
- not implemented (reserved for future optional API areas).

All mutating operations should provide the same strong guarantee as
`bigint_set_string`: on failure, their destination is unchanged.

## Future work

The implementation is complete for the current public surface. The most useful
next work is property/reference testing for large generated decimal values,
followed by profiling-guided optimization such as reusable small powers of ten
inside one operation. Any narrow `BigInt` helper added for performance must
preserve the public layering and be independently tested.

## BigInt boundary

Use the public `BigInt` API in the first version. This keeps the layers
independent and prevents `BigDecimal` from relying on limb layout. Internally,
build a reusable `BigInt` value for ten and use `bigint_div_mod` during
normalization. If profiling later shows repeated decimal scaling is expensive,
add a small, well-tested `BigInt` helper for multiplication or division by a
`uint64_t`; do not expose the `BigInt` limb representation.
