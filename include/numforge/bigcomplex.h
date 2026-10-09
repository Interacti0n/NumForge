#ifndef NUMFORGE_BIGCOMPLEX_H
#define NUMFORGE_BIGCOMPLEX_H

#include <numforge/bigdecimal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Owned pair of finite base-10 decimals, re + im*i. This is not a pair of
 * exact rationals: recurring division results require rounding. No ordering
 * or implicit complex extension of real functions is defined. */
typedef struct BigComplex BigComplex;
typedef enum BigComplexStatus {
    BIGCOMPLEX_OK = 0,
    BIGCOMPLEX_NULL_ARGUMENT,
    BIGCOMPLEX_OUT_OF_MEMORY,
    BIGCOMPLEX_INVALID_ARGUMENT,
    BIGCOMPLEX_DIVISION_BY_ZERO,
    BIGCOMPLEX_VALUE_TOO_LARGE,
    BIGCOMPLEX_SCALE_OVERFLOW
} BigComplexStatus;

const char *bigcomplex_status_to_string(BigComplexStatus status);
/* create returns owned zero or NULL; destroy accepts NULL. Every mutation
 * supports result/input aliasing and leaves result unchanged on failure.
 * Runtime budgets are inherited, never implicitly started. */
BigComplex *bigcomplex_create(void);
void bigcomplex_destroy(BigComplex *value);
BigComplexStatus bigcomplex_copy(BigComplex *result, const BigComplex *value);
BigComplexStatus bigcomplex_set_parts(BigComplex *result, const BigDecimal *real, const BigDecimal *imaginary);
/* Separate decimal strings, using BigDecimal's syntax (not a complex parser). */
BigComplexStatus bigcomplex_set_strings(BigComplex *result, const char *real, const char *imaginary);
BigComplexStatus bigcomplex_from_bigdecimal(BigComplex *result, const BigDecimal *value);
/* Copies into an initialized decimal destination. */
BigComplexStatus bigcomplex_get_real(BigDecimal *result, const BigComplex *value);
BigComplexStatus bigcomplex_get_imaginary(BigDecimal *result, const BigComplex *value);
/* Boolean outputs are unchanged on failure. */
BigComplexStatus bigcomplex_is_zero(bool *result, const BigComplex *value);
BigComplexStatus bigcomplex_equal(bool *result, const BigComplex *a, const BigComplex *b);
BigComplexStatus bigcomplex_negate(BigComplex *result, const BigComplex *value);
BigComplexStatus bigcomplex_conjugate(BigComplex *result, const BigComplex *value);
/* Finite decimal addition, subtraction and multiplication are exact. */
BigComplexStatus bigcomplex_add(BigComplex *result, const BigComplex *a, const BigComplex *b);
BigComplexStatus bigcomplex_sub(BigComplex *result, const BigComplex *a, const BigComplex *b);
BigComplexStatus bigcomplex_mul(BigComplex *result, const BigComplex *a, const BigComplex *b);
/* Exact intermediate products/sums; each component uses exact-or-significant
 * division independently. digits >= 1, rounding must be a valid mode.
 * Terminating components remain exact, recurring ones round to digits.
 * Cancellation does not lose digits before the final divisions. Resource and
 * scale limits may reject otherwise valid large intermediate computations. */
BigComplexStatus bigcomplex_div(BigComplex *result, const BigComplex *a, const BigComplex *b,
    int64_t digits, BigDecimalRoundingMode rounding);
/* Integer powers use exact exponentiation by squaring, followed by one
 * reciprocal for negative exponents. 0^0 = 1; zero to a negative power is an
 * error. digits and rounding are validated for every exponent. INT64_MIN is
 * supported. Exact intermediates remain subject to resource/scale limits. */
BigComplexStatus bigcomplex_pow_int(BigComplex *result, const BigComplex *value,
    int64_t exponent, int64_t digits, BigDecimalRoundingMode rounding);
/* Exact re*re + im*im. */
BigComplexStatus bigcomplex_abs_squared(BigDecimal *result, const BigComplex *value);
/* Square root of the exact squared modulus. Exact finite roots remain exact;
 * irrational roots round to positive significant digits with the given mode.
 * Exact intermediates may exceed resource/scale limits. */
BigComplexStatus bigcomplex_abs(BigDecimal *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
/* Principal square root: nonnegative real part; imaginary sign follows input,
 * with positive imaginary root on the negative real axis. Zero maps to zero.
 * Exact finite roots remain exact; otherwise approximate components round to
 * significant digits. Uses digits+12 working digits and a cancellation-free
 * formula, not a correctly-rounded guarantee. Exact intermediate squares may
 * exceed resource/scale limits. Supports aliasing and atomic failure. */
BigComplexStatus bigcomplex_sqrt(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
/* Exact canonical decimal text: 0, re, i, -i, im*i, re + im*i, re - im*i.
 * This is display text, not a versioned persistence format. Caller frees the
 * owned string with free(); *result is unchanged on failure. */
BigComplexStatus bigcomplex_to_string(const BigComplex *value, char **result);
/* Formats signed components independently using BigDecimal's places policy
 * (-1 retains all digits), then composes re +/- im*i. Notation does not change
 * rounding policy. Rounded zero components are omitted; mathematical imaginary
 * coefficients are parenthesized. max_output_bytes bounds the complete UTF-8
 * output excluding NUL, SIZE_MAX removes this bound. This is an output-size
 * bound, not a working-memory budget. Caller frees output; failure preserves
 * *result and never mutates value. */
BigComplexStatus bigcomplex_format(const BigComplex *value, int64_t places,
    BigDecimalRoundingMode rounding, BigDecimalFormatMode mode,
    size_t max_output_bytes, char **result);
typedef enum BigComplexForm {
    BIGCOMPLEX_FORM_CARTESIAN = 0,
    BIGCOMPLEX_FORM_TRIGONOMETRIC,
    BIGCOMPLEX_FORM_EXPONENTIAL
} BigComplexForm;
/* Principal argument in radians, (-pi, pi]; zero has no argument and returns
 * INVALID_ARGUMENT. Uses digits+12 working digits; approximate, not a claim
 * of correctly rounded atan2. Output unchanged on failure. */
BigComplexStatus bigcomplex_arg(BigDecimal *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
/* Construct Cartesian components from radius >= 0 and radians. Approximate
 * trig at digits+12 followed by exact decimal products. Zero radius yields
 * zero. Failure preserves result. Not an exact symbolic trigonometric type. */
BigComplexStatus bigcomplex_set_polar(BigComplex *result, const BigDecimal *radius,
    const BigDecimal *angle, int64_t digits, BigDecimalRoundingMode rounding);
/* exp(re + im*i) = exp(re)*(cos(im) + i*sin(im)), radians. Approximate;
 * uses digits+12 working digits and rounds components to significant digits.
 * Supports aliasing and preserves result on failure. */
BigComplexStatus bigcomplex_exp(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
/* Principal natural logarithm: ln(abs(z)) + i*arg(z), radians, arg in
 * (-pi, pi]. Zero returns INVALID_ARGUMENT; ln(1+0i) is zero. The negative
 * real axis uses +pi; approaching from below gives the -pi limit. Signed
 * zero is not retained. Real part uses ln(re*re+im*im)/2 with exact squares
 * and digits+12 working digits. Approximate, not correctly-rounded; exact
 * intermediates may exceed resource/scale limits. Atomic and alias-safe. */
BigComplexStatus bigcomplex_ln(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
/* Principal power exp(exponent*ln(value)), in radians, with digits+12 guard
 * digits. Approximate even for integer exponents; use pow_int for exact
 * integer powers. 0^0=1, zero to a positive real exponent=0, negative real
 * exponent returns DIVISION_BY_ZERO, nonreal exponent INVALID_ARGUMENT.
 * Supports aliasing with either input and preserves result on failure.
 * No correct-rounding guarantee; cancellation and input projection can lose
 * relative accuracy. digits must leave room for 24 internal guard digits. */
BigComplexStatus bigcomplex_pow(BigComplex *result, const BigComplex *value,
    const BigComplex *exponent, int64_t digits, BigDecimalRoundingMode rounding);
/* Principal base logarithm ln(value)/ln(base), radians and (-pi, pi] branch.
 * Zero value, zero base and base 1 are INVALID_ARGUMENT. Approximate with
 * digits+12 working digits; no correct-rounding guarantee. Near-unit bases
 * amplify input/rounding error. Supports either-input aliasing, atomic failure.
 * digits must leave room for 24 internal guard digits. */
BigComplexStatus bigcomplex_log(BigComplex *result, const BigComplex *value,
    const BigComplex *base, int64_t digits, BigDecimalRoundingMode rounding);
/* Complex sine/cosine in radians, independent of calculator RAD/DEG:
 * sin(x+iy)=sin(x)cosh(y)+i*cos(x)sinh(y),
 * cos(x+iy)=cos(x)cosh(y)-i*sin(x)sinh(y).
 * Approximate, digits+12 guarded scalar calls, final component significant
 * rounding. No correct-rounding guarantee; large imaginary parts can exceed
 * runtime/scale limits. Alias-safe, failure preserves result. digits>=1 and
 * <=INT64_MAX-12. No implicit symbolic recognition of multiples of pi. */
BigComplexStatus bigcomplex_sin(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
BigComplexStatus bigcomplex_cos(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
/* tan(z)=sin(z)/cos(z), guarded sine/cosine at digits+12 and final division.
 * digits<=INT64_MAX-24. Computed zero denominator gives DIVISION_BY_ZERO;
 * near real-axis poles can amplify errors. Finite decimal pi approximations
 * need not give exact poles. Resource limits apply also to intermediates,
 * even when the final tangent is bounded. Atomic and alias-safe. */
BigComplexStatus bigcomplex_tan(BigComplex *result, const BigComplex *value,
    int64_t digits, BigDecimalRoundingMode rounding);
/* Cartesian delegates to format; polar displays r*(cos(phi)+i*sin(phi)) or
 * r*e^(i*(phi)), radians. Zero displays 0 in every form. Polar coordinates
 * are approximate, with explicit working digits; places is display policy.
 * Axis angles display symbolic pi (UTF-8) or +/-pi/2.
 * This changes display only, never stored Cartesian components. */
BigComplexStatus bigcomplex_format_form(const BigComplex *value, BigComplexForm form,
    int64_t digits, int64_t places, BigDecimalRoundingMode rounding,
    BigDecimalFormatMode mode, size_t max_output_bytes, char **result);

#ifdef __cplusplus
}
#endif
#endif
