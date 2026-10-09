# BigComplex foundation

`<numforge/bigcomplex.h>` is an additive part of `NumForge::numforge`.
It owns two opaque `BigDecimal` components representing `re + im*i`.
The calculator now supports complex(re;im), typed arithmetic, sessions and
HTTP snapshots; see CALCULATOR_EXPRESSIONS.md and SESSION_HTTP_API.md.

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

`bigcomplex_ln(result, value, digits, rounding)` returns the principal natural
logarithm ln(|z|)+i*arg(z), in radians with imaginary part in (-pi, pi].
Zero is invalid; ln(1+0i) is zero. The negative real axis uses +pi, with the
-pi limit on approach from below. Signed zero is not retained. This is one
principal value, not the family of logarithms differing by 2*pi*i.

The real component computes ln(re*re+im*im)/2 from the exact stored decimal
squares at digits+12 working digits, avoiding a rounded magnitude which could
lose a small logarithm near the unit circle. Components round independently to
the requested significant digits. Approximate results are not guaranteed to be
correctly rounded; exact intermediate sizes/scales can still exceed resource
limits. Aliasing is supported; all failures preserve the destination. The
calculator explicitly projects rational components at guarded working precision
before using this decimal API, so projection error may remain in either component.

`bigcomplex_exp(result, value, digits, rounding)` computes
exp(re)*(cos(im)+i*sin(im)) in radians, using digits+12 working digits and
rounding each resulting component to the requested significant digits.
It supports aliasing and preserves the destination on every failure. This is
an approximate numerical exponential, not symbolic simplification or a promise
of correct rounding. Small imaginary residuals near multiples of pi are retained.

`bigrationalcomplex_conjugate` preserves exact rational components and supports
aliasing. `bigrationalcomplex_abs_squared` returns the exact rational sum of
the component squares. Both preserve their destination on failure.

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

## Complex trigonometry

`bigcomplex_sin`, `bigcomplex_cos` and `bigcomplex_tan` accept finite complex
inputs, always in radians. Sine and cosine use
sin(x+iy)=sin(x)cosh(y)+i*cos(x)sinh(y) and
cos(x+iy)=cos(x)cosh(y)-i*sin(x)sinh(y), guarded scalar calls at digits+12,
then component-wise significant rounding. Tangent divides guarded complex
sine by cosine, with another 12 guard digits before the final division.
digits must be positive and leave room for 12 guard digits for sin/cos,
24 for tan; rounding must be a valid mode. Aliasing is supported and every
failure preserves the destination.

These results are approximate, without correctly-rounded guarantees or symbolic
recognition of pi multiples. Tangent's poles are on the real axis at
pi/2+k*pi; a computed zero cosine returns DIVISION_BY_ZERO. Finite decimal
approximations to poles need not give exact zeros. Near poles, rounding and
rational projection errors can be amplified. Large imaginary parts grow
sin/cos exponentially; runtime and scale limits apply to intermediates even
when a final tangent is bounded. No saturation approximation is substituted.
Tiny nonzero residuals are preserved. For example sin(i) is approximately
1.1752011936*i, cos(i) 1.5430806348 and tan(i) 0.761594156*i.

The calculator uses this path for complex-typed inputs, including inputs
whose imaginary part is zero. Exact fractions project at guarded working
precision. Results remain complex decimal approximations. Real-only inputs
keep RAD/DEG behavior; complex inputs use radians even in DEG. Complex inverse
trigonometry and hyperbolic functions remain future work.

## Principal base logarithms

`bigcomplex_log(result, value, base, digits, rounding)` returns
ln(value)/ln(base), using the principal logarithm branch (-pi, pi] in radians.
Zero value, zero base and base one return INVALID_ARGUMENT. Negative real
and nonreal bases are allowed. This is a numerical approximation, not all
logarithm branches: log(-1;i)=2 and log(-i;i)=-1. It is not a general inverse
identity for principal powers across branch cuts.

Both logarithms use digits+12 working digits before component-wise
exact-or-significant division. digits must be positive and at most
INT64_MAX-24. There is no correctly-rounded guarantee; bases near one amplify
input projection and rounding errors. Runtime/scale limits apply. Aliasing
with either or both inputs is supported, and failures preserve the result.
The calculator projects exact components at guarded working precision and
retains an approximate complex result even when it displays a real number.

## Principal general powers

`bigcomplex_pow(result, value, exponent, digits, rounding)` computes the
approximate principal value exp(exponent*ln(value)), in radians with the
logarithm branch (-pi, pi]. It uses digits+12 for ln, exact decimal
multiplication, and exp with its own 12 guard digits. `digits` must be positive
and at most INT64_MAX-24. There is no correct-rounding guarantee; cancellation
and exact-rational projection can lose relative accuracy. Intermediate scale
and runtime limits apply. Small numerical residuals are retained.

Unlike `bigcomplex_pow_int`, this general API is approximate even for integer
exponents. Use the integer API when exactness matters. Zero to zero is one;
zero to a positive real exponent is zero. Zero to a negative real exponent
returns DIVISION_BY_ZERO, and a nonreal exponent returns INVALID_ARGUMENT.
Errors preserve the destination; aliasing with either or both inputs is
supported. For example i^i is approximately 0.2078795764. This returns one
principal value, not all branches; identities across branch cuts need not hold.

## Integer powers and modulus

`bigcomplex_sqrt(result, value, digits, rounding)` returns the principal square
root: real part >= 0, imaginary sign follows the input, and the negative real
axis has a positive imaginary root. Signed zero is not retained. Zero maps to
zero. The branch cut lies on the negative real axis; approaching it from below
gives a negative imaginary limit, while the value on the axis uses the positive
root. This is one root, not a list of both plus/minus solutions.

The algorithm computes the larger component from sqrt((abs(z)+abs(re))/2)
and the smaller by division, avoiding subtraction of nearly equal quantities.
Working precision is digits+12. A candidate whose exact square matches the
stored input remains exact; otherwise both components round to significant
digits. Approximate results have no correctly-rounded guarantee. Exact squares
and large exponent gaps remain subject to resource/scale limits. Aliasing is
supported and every failure preserves the destination.

The calculator first proves rational component roots through BigInt/BigRational;
irrational rational-input roots explicitly project to decimals at guarded working
precision. `sqrt(-1)` stays a real-domain error; `sqrt(-1+0i)` returns exact `i`.

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
2. Consider an explicit complex mode for extending real-domain functions.
   The calculator already supports i, complex(re;im), re/im/conj/abs/arg and exp.
   Mixed approximate operations explicitly project exact components at working
   precision. Typed sessions and snapshots are implemented.
3. Improve argument/exponential/square-root/logarithm rounding guarantees, add
   inverse trigonometric/hyperbolic functions and improve transcendental error bounds.
   Define cuts, zero behavior and numerical
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

Calculator exact real inputs promote to rational complex values. Mixed
approximate operations project exact components at working precision to decimal
complex values. Typed snapshots preserve kind and both stored components.

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
