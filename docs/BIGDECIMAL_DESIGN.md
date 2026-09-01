# BigDecimal design

`BigDecimal` provides exact base-10 values on top of `BigInt`, without using
binary floating point. Its public API is declared in
`include/numforge/bigdecimal.h` and implemented in
`src/bigdecimal/bigdecimal.c`.

## Current API

The public header defines the component's current intended 1.0 surface:

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

The public `bigdecimal_to_string` function produces ordinary decimal notation.
The calculator's separate formatter adds scientific notation when useful
without changing the stored value or the public BigDecimal conversion contract.

### Addition and subtraction

Addition and subtraction align both operands to the larger scale by multiplying
the lower-scale coefficient by a power of ten, then adding or subtracting the
coefficients. An unrepresentable scale difference returns
`BIGDECIMAL_VALUE_TOO_LARGE`; an allocation failure returns
`BIGDECIMAL_OUT_OF_MEMORY`.

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

All mutating operations provide the same strong guarantee as
`bigint_set_string`: on failure, their destination is unchanged.

## Testing and future work

The implementation is complete for the current public surface. Focused unit
tests cover explicit regressions and API errors. A separate deterministic
property suite uses a bounded independent `int64_t` reference model to check
conversion, canonical form, exact arithmetic, comparison, aliasing, rescaling,
division, and every rounding mode. The allocation-failure suite additionally
fails each allocation in conversion, comparison, exact and rounded arithmetic,
then verifies out-of-memory propagation and unchanged destinations. Its
end-to-end case also covers parser, evaluator, and formatter cleanup.

The most useful next work is larger generated decimal vectors and optional
external-oracle checks, followed by profiling-guided optimization such as
reusable small powers of ten inside one operation. Any narrow `BigInt` helper
added for performance must preserve the public layering and be independently
tested.

## BigInt boundary

The implementation uses the public `BigInt` API. This keeps the layers
independent and prevents `BigDecimal` from relying on limb layout. Power-of-ten
construction and normalization are centralized behind internal helpers. If
profiling later shows repeated decimal scaling is expensive, a small,
well-tested `BigInt` helper for multiplication or division by a `uint64_t` can
be added without exposing the limb representation.
