# BigComplex foundation

`<numforge/bigcomplex.h>` is an additive part of `NumForge::numforge`.
It owns two opaque `BigDecimal` components representing `re + im*i`.
No calculator, session or HTTP behavior changes in this first stage.

```c
#include <numforge/bigcomplex.h>
#include <stdlib.h>

BigComplex *z = bigcomplex_create();
char *text = NULL;
if (z != NULL &&
    bigcomplex_set_strings(z, "0", "1") == BIGCOMPLEX_OK &&
    bigcomplex_mul(z, z, z) == BIGCOMPLEX_OK &&
    bigcomplex_to_string(z, &text) == BIGCOMPLEX_OK) {
    /* text is "-1": i*i = -1. */
    free(text);
}
bigcomplex_destroy(z);
```

## Contract

- Creation returns owned zero or NULL; destruction accepts NULL. Setters and
  accessors copy values. There are no borrowed pointers to mutable components.
- All mutations preserve their destination on failure, including failure after
  calculating only one component. Arithmetic and copy support result/input
  aliasing. Boolean and owned-text outputs also remain unchanged on failure.
- Addition, subtraction, multiplication, negation, conjugation and squared
  modulus operate on the stored finite decimals exactly, within resource and
  scale limits. Equality is numeric component equality; there is no ordering.
- Division accepts explicit positive significant digits and a rounding mode.
  It computes products and sums exactly, then divides each component once via
  BigDecimal's exact-or-significant policy. Terminating components remain exact;
  recurring components are rounded independently. These are componentwise
  semantics, not a guaranteed relative error for the whole complex number.
- A zero divisor is an error. Very different exponents can create large exact
  intermediates and fail with scale/resource limits. The implementation does
  not silently replace those intermediates with machine floating point.
- Runtime budgets are inherited. Operations do not start budgets or introduce
  globals; callers sharing mutable objects between threads must synchronize.
- `to_string` emits exact component text with `*i` for non-unit coefficients:
  `0`, `2`, `i`, `-i`, `2*i`, `3 - 2*i`. Free successful output with `free()`.
  This is display text, not a persistence format or a new expression parser.
- BigComplex does not preserve recurring rational components such as `1/3`
  exactly. No claim about the exact mathematical origin of a decimal is made.

## Integer powers and modulus

`bigcomplex_pow_int(result, value, exponent, digits, rounding)` accepts signed
64-bit exponents, including `INT64_MIN`. Exponentiation by squaring performs
exact decimal multiplications. Negative exponents then compute one reciprocal
with the same componentwise exact-or-significant policy as division. No rounded
reciprocal is repeatedly multiplied. Zero to exponent zero is defined as one;
zero to a negative exponent returns `BIGCOMPLEX_DIVISION_BY_ZERO`.

`bigcomplex_abs(result_decimal, value, digits, rounding)` computes the square
root of the exact `re*re + im*im` through BigDecimal's integer-based root API.
Finite decimal roots remain exact even beyond the requested significant digits;
irrational roots round using the requested mode. For example, `abs(3 + 4*i)` is
exactly `5`, while `abs(1 + i)` at five digits with half-even rounding is `1.4142`.
The exact sum retains small components even across severe exponent gaps, so
directed rounding accounts for them. There is no fixed guard-digit estimate or
machine floating-point fallback. Squaring and exact exponent alignment can
exceed scale or memory limits, even when the final mathematical result is small.

Both functions require positive digits and a valid rounding mode for every
input, including exact results and exponent zero. Failures preserve output;
powers support input/output aliasing and inherit runtime budgets.

Functions and complete signatures are in the public headers. Complex
transcendental functions remain deferred.

## Display formatting

`bigcomplex_format(value, places, rounding, mode, max_output_bytes, &text)`
formats each signed component with `bigdecimal_format_mode`, then composes the
complex display. `places = -1` retains stored digits; other nonnegative values
use BigDecimal's existing policy: decimal places for fixed-point values and
mantissa places when its rounding policy selects scientific notation. Selecting
a notation changes display only, not this rounding policy. It is not a uniform
significant-digit parameter. Modes are auto, plain, scientific and mathematical.

Signed components are rounded before their signs are separated, preserving
floor/ceiling semantics. Components rounded to zero are omitted; a plain
imaginary coefficient of one displays as `i`. Scientific coefficients retain
their notation, e.g. `2E+0 - 1E+0*i`. Mathematical imaginary coefficients are
parenthesized, e.g. `2 × 10^0 - (1 × 10^0)*i`, to make multiplication unambiguous.
Formatting never changes the stored number or guarantees expression round trips.

The limit counts bytes of the complete UTF-8 display, including separators and
parentheses, excluding the final NUL. A result exactly at the limit succeeds;
overflow returns `BIGCOMPLEX_VALUE_TOO_LARGE` without truncating or replacing the
caller's pointer. `SIZE_MAX` disables the extra output bound. The bound does not
limit all temporary storage: use runtime budgets for that. Caller frees
successful output with `free()`; all failures preserve the output pointer.

## Proposed continuation

1. Consider a scaled/adaptive magnitude algorithm to reduce exact intermediate
   storage while preserving rounding guarantees. Integer powers, exact-sum
   modulus and bounded display formatting are available.
2. Decide calculator mixed exact/decimal promotion rules. BigRationalComplex
   provides exact rational components; its decimal projection is explicit.
3. Extend CalculatorValue, ownership/copying, evaluator, cache and snapshots.
   Decide whether `i` is reserved and whether `sqrt(-1)` needs complex mode.
   Unsupported real-only functions and unit conversions must reject complex
   arguments explicitly. Preserve existing real behavior.
4. Add versioned typed complex snapshots, session variables/ans/history and
   documented HTTP operations together with localized web formatting/tests.
   Neither plain display strings nor a discarded imaginary part are snapshots.
5. Improve argument rounding guarantees, add principal square root and logarithm branches, then exp,
   powers and trigonometric functions. Define cuts, zero behavior and numerical
   validation separately. Correct rounding needs more than fixed guard digits.

The stages are a roadmap, not implemented features or release dates.

## Exact rational complex values

`<numforge/bigrationalcomplex.h>` adds opaque `BigRationalComplex`, owning two
reduced BigRational components. Setters/accessors copy; arithmetic and signed
integer powers are exact, support destination aliasing and preserve outputs on
failure. Negative powers use exact reciprocal; zero to a negative power is an
error and `0^0` is one. The status type is BigComplexStatus. Runtime budgets are
inherited. No implicit promotion or conversion changes existing decimal APIs.

`set_parts` accepts two BigRational objects. `to_string` retains fractions,
e.g. `1/3 + (1/3)*i`; its complete byte bound excludes NUL and the caller frees
successful output. `to_bigcomplex` explicitly projects with positive significant
digits: finite components remain exact, recurring components round independently.
`from_bigcomplex` converts stored finite decimals exactly; it cannot recover
fractions previously rounded to decimals. These conversions are atomic.

Calculator promotion, typed snapshots and HTTP integration remain future work.
Exact real integers/rationals should promote to rational complex values; mixed
decimal operations need an explicit policy before connecting to sessions.

## Cartesian, trigonometric and exponential forms

`bigcomplex_format_form` selects Cartesian `a + b*i`, trigonometric
`(r)*(cos(phi) + i*sin(phi))`, or exponential `(r)*e^(i*(phi))`. A radius of one
omits the coefficient. The forms display the same stored Cartesian value,
without replacing it with rounded polar coordinates. Angles are radians, with
the principal branch mathematically in `(-pi, pi]`. Axis angles use symbolic
UTF-8 `π` or `±π/2`: `-1` displays as `e^(i*(π))`. Other angles are decimal
approximations. Zero displays `0` in every form; its argument alone is undefined.
These are display strings, not a supported calculator/parser extension.

`bigcomplex_arg` computes a quadrant-aware argument using atan, with twelve
extra working significant digits, then rounds the final angle. This is an
approximation policy, not a correctly rounded atan2 guarantee: no adaptive
error bounds are implemented. `format_form` accepts separate numerical digits
and display places/notation, and bounds the complete UTF-8 output.

`bigrationalcomplex_format_form` preserves exact fractions in Cartesian form.
Polar forms explicitly project components at `digits+12`, then use decimal
polar formatting. These polar coordinates are approximate even if the source
is exact. Places/notation apply only to polar output; all arguments are validated
for every form. The source rational components remain exact.

`bigcomplex_set_polar(result, radius, angle, digits, rounding)` constructs
Cartesian components from a nonnegative radius and a radian angle via cosine
and sine at twelve extra working digits, followed by exact decimal products.
It is approximate and cannot retain symbolic pi or exact rational origins;
round trips can differ. A zero radius produces zero. Negative radius and invalid
precision/modes are errors. Failure preserves the destination.
