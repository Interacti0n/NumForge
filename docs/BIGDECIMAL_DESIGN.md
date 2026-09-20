# BigDecimal design

`BigDecimal` provides exact base-10 values on top of `BigInt`, without using
binary floating point. Its public API is declared in
`include/numforge/bigdecimal.h` and implemented in
`src/bigdecimal/`. No numeric implementation depends on calculator headers.

## Source layout

| File | Responsibility |
| --- | --- |
| `bigdecimal.c` | Shared private helpers, canonicalization, status text, and object lifecycle. |
| `conversion.c` | Parsing and conversion to ordinary decimal text. |
| `comparison.c` | Numeric comparison and basic predicates. |
| `arithmetic.c` | Sign operations and exact addition, subtraction, and multiplication. |
| `aggregation.c` | Exact sequence sum/product and exact-first arithmetic mean. |
| `statistics.c` | Median, geometric/harmonic means, and population/sample dispersion. |
| `division.c` | Rescaling, fixed-scale division, significant-digit division, and exact-first division. |
| `operations.c` | BigInt conversions, integer/sign helpers, min/max, and integer powers. |
| `roots.c` | General real roots plus square-root and cube-root wrappers. |
| `transcendental.c` | Guarded exponential and logarithmic functions with decimal argument reduction. |
| `trigonometric.c` | Guarded radian trigonometric functions and magnitude-aware π reduction. |
| `format.c` | Precision-aware readable and scientific result formatting. |
| `constants.c` | Stored and dynamically calculated high-precision mathematical constants. |
| `bigdecimal_internal.h` | Private representation and declarations shared only by these modules. |

The installed public API remains entirely in `include/numforge/bigdecimal.h`;
the source split does not expose the internal representation or helper calls.

## Current API

The public header defines the component's stable 1.x surface:

- lifecycle: `bigdecimal_create`, `bigdecimal_destroy`;
- conversion: `bigdecimal_copy`, `bigdecimal_set_string`,
  `bigdecimal_to_string`;
- comparison and inspection: `bigdecimal_compare`, `bigdecimal_is_zero`,
  `bigdecimal_is_negative`;
- exact arithmetic: absolute value, negation, addition, subtraction,
  multiplication, sequence sum, and sequence product;
- controlled inexact operations: division, rescaling, named integer/decimal-place
  rounding, sequence means, median, variance, standard deviation, real roots, exponential,
  logarithmic, and trigonometric functions with explicit precision and rounding.

All listed operations are implemented. Every mutating operation computes into
a temporary value and commits only on success, so its destination is unchanged
after an error.

Additive APIs also provide exact integer conversions, sign/integer predicates,
min/max, sequence aggregates, floor/ceil/trunc/half-even round, integer powers,
population/sample statistics, real roots, significant and exact-first division,
constants and readable formatting; see [API.md](API.md#additional-numeric-operations).
The calculator calls public APIs without accessing the representation.

Roots preserve exact finite results. For canonical `C * 10^-s`, a finite kth
root exists iff s is divisible by k and abs(C) is a perfect kth power. Otherwise
exponent division normalizes the radicand independently of the absolute scale.
Integer Newton iteration computes a floor root with a guard digit; a sticky
digit encodes the nonzero tail for the six rescaling modes. Degree and precision
are public arguments with checked size/scale arithmetic, not application caps.

Transcendental functions also take significant digits and a rounding mode.
`exp` repeatedly halves its argument into [-0.5, 0.5], evaluates its Taylor
series with 24 guard digits, and reconstructs the result by squaring. `ln`
repeatedly square-roots a positive input into [0.9, 1.1], evaluates
`2 * atanh((x-1)/(x+1))`, and restores the removed powers of two. `log10` and
arbitrary-base `log` divide two guarded natural logarithms. Internal iterations
use half-even rounding so directed output modes cannot prevent convergence;
the requested rounding mode is applied to the final significant result. A
bounded number of reduction steps rejects magnitudes that cannot fit the
representation, and every loop checks an active cooperative resource budget.

Forward trigonometric functions are radian-only at the public library boundary.
Argument reduction rounds `x/(π/2)` to the nearest integer, evaluates sine and
cosine together on `[-π/4, π/4]`, and maps the pair by quadrant. The π request
includes the requested precision, 24 working guard digits, and the input's
integer-digit magnitude, preventing a large angle from discarding the reduced
fraction. `tan` divides the shared pair. `atan` uses reciprocal and repeated
half-angle reduction before its alternating series; `asin` and `acos` build on
`atan` and `sqrt((1-x)*(1+x))`, preserving the exact distance from the domain
endpoints. `acos` uses a complementary arctangent formula to avoid subtracting
nearly equal angles near 1. Intermediate rounding is half-even and only the final
step applies the caller's requested mode.

Constants use a hybrid policy. `bigdecimal_set_constant` preserves the original
full 500-decimal-place values. The precision-aware factory rounds those stored
values through 500 significant digits and, above that threshold, calculates π
with the quadratically convergent Gauss-Legendre iteration, e as `exp(1)`, and φ
as `(1 + sqrt(5)) / 2`. Dynamic calculations use 24 decimal guard digits and
half-even intermediate rounding before applying the caller's final rounding
mode. The library keeps no mutable global constant cache, so independent calls
and threads do not share hidden state; clients may retain or cache returned
objects when repeated use matters.

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
Before its exact scientific check, the formatter uses conservative integer
bounds around `log10(2)` to prove when a value must remain in ordinary notation.
That proof skips a redundant full coefficient conversion; inconclusive values
retain the exact previous path, and rounding is followed by another notation
check so a carry across the scientific threshold is handled correctly.

### Addition and subtraction

Addition and subtraction align both operands to the larger scale by multiplying
the lower-scale coefficient by a power of ten, then adding or subtracting the
coefficients. An unrepresentable scale difference returns
`BIGDECIMAL_VALUE_TOO_LARGE`; an allocation failure returns
`BIGDECIMAL_OUT_OF_MEMORY`.

### Multiplication

Multiplication first multiplies and normalizes the coefficient at scale zero,
then adds the operand scales and the normalization adjustment using checked
`int64_t` arithmetic. Cross-factor trailing zeroes can therefore rescue an
otherwise overflowing intermediate sum. Zero remains canonical at scale zero;
true final scale overflow leaves the destination unchanged.

Normalization strips decimal zeroes in blocks of up to 19 using private
single-limb division helpers. The common already-normalized case is detected
without allocation. This avoids thousands of general BigInt divisions when
high-precision division produces a short terminating decimal.

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
- scale overflow.

All mutating operations provide the same strong guarantee as
`bigint_set_string`: on failure, their destination is unchanged.

## Testing and future work

The implementation is complete for the current public surface. Focused unit
tests cover explicit regressions and API errors. A separate deterministic
property suite uses a bounded independent `int64_t` reference model to check
conversion, canonical form, exact arithmetic, comparison, aliasing, rescaling,
division, and every rounding mode. Focused transcendental and trigonometric
suites cover guarded series, domains, large-argument reduction, and known
high-precision values. The allocation-failure suite additionally
fails each allocation in conversion, comparison, exact and rounded arithmetic,
then verifies out-of-memory propagation and unchanged destinations. Its
end-to-end case also covers parser, evaluator, and formatter cleanup.

The most useful next work is larger generated decimal vectors and optional
external-oracle checks, followed by profiling-guided optimization such as
reusable small powers of ten inside one operation. Any narrow `BigInt` helper
added for performance must preserve the public layering and be independently
tested.

## BigInt boundary

Most decimal operations use the public `BigInt` API. There are two internal
dependencies within the numeric library: normalization calls the private
`bigint_strip_decimal_zeros` helper, and the root implementation reads the
coefficient's limbs to determine its bit length. These dependencies stay in
`src/`; installed headers keep both numeric representations opaque.
Power-of-ten construction and normalization are centralized behind private
helpers. The calculator and web clients use only public numeric APIs and
must not include either numeric internal header.
