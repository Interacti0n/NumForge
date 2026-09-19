#ifndef NUMFORGE_BIGDECIMAL_H
#define NUMFORGE_BIGDECIMAL_H

#include <stdbool.h>
#include <stddef.h>
#include <numforge/bigint.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
------------------------------------------------------------------------------------------------------------------------------
    Opaque exact base-10 number. Values must be created and destroyed through
    this API; their coefficient and scale remain private implementation details.

    Implementation: src/bigdecimal/bigdecimal.c
------------------------------------------------------------------------------------------------------------------------------
*/
typedef struct BigDecimal BigDecimal;

/*
------------------------------------------------------------------------------------------------------------------------------
    Status codes returned by BigDecimal operations.

    Implementation: src/bigdecimal/bigdecimal.c
------------------------------------------------------------------------------------------------------------------------------
*/
typedef enum BigDecimalStatus
{
    BIGDECIMAL_OK = 0,
    BIGDECIMAL_NULL_ARGUMENT,
    BIGDECIMAL_OUT_OF_MEMORY,
    BIGDECIMAL_INVALID_ARGUMENT,
    BIGDECIMAL_DIVISION_BY_ZERO,
    BIGDECIMAL_VALUE_TOO_LARGE,
    BIGDECIMAL_SCALE_OVERFLOW
} BigDecimalStatus;

/*
------------------------------------------------------------------------------------------------------------------------------
    Rounding modes used by division and rescaling.

    Used by: src/bigdecimal/division.c, roots.c, transcendental.c, and format.c
------------------------------------------------------------------------------------------------------------------------------
*/
typedef enum BigDecimalRoundingMode
{
    BIGDECIMAL_ROUND_TOWARD_ZERO = 0,
    BIGDECIMAL_ROUND_AWAY_FROM_ZERO,
    BIGDECIMAL_ROUND_FLOOR,
    BIGDECIMAL_ROUND_CEILING,
    BIGDECIMAL_ROUND_HALF_UP,
    BIGDECIMAL_ROUND_HALF_EVEN
} BigDecimalRoundingMode;

const char *bigdecimal_status_to_string( /*Human-readable description of a BigDecimalStatus, for logging/debugging*/
    BigDecimalStatus status
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Lifetime, conversion, and copy functions for BigDecimal.

    create returns a normalized zero value, or NULL on allocation failure.
    destroy accepts NULL. set_string accepts an optional sign, decimal point,
    and e/E exponent; it rejects whitespace and malformed values. to_string
    returns ordinary decimal notation through result, owned by the caller and
    released with free(). On failure, output BigDecimal values and *result are
    unchanged.

    Implementation: lifecycle and copy in src/bigdecimal/bigdecimal.c;
    text conversion in src/bigdecimal/conversion.c.
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimal *bigdecimal_create(
    void
);
void bigdecimal_destroy(
    BigDecimal *value
);

BigDecimalStatus bigdecimal_copy(
    BigDecimal *destination,
    const BigDecimal *source
);
BigDecimalStatus bigdecimal_set_string(
    BigDecimal *value,
    const char *string
);
BigDecimalStatus bigdecimal_to_string(
    const BigDecimal *value,
    char **result
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Comparison and inspection functions for BigDecimal.

    comparison receives a value less than, equal to, or greater than zero;
    boolean results are written on success.

    Implementation: src/bigdecimal/comparison.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_compare(
    int *comparison,
    const BigDecimal *a,
    const BigDecimal *b
);
BigDecimalStatus bigdecimal_is_zero(
    bool *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_is_negative(
    bool *result,
    const BigDecimal *value
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Exact arithmetic operation functions for BigDecimal.

    All operations support output/input aliasing, for example
    bigdecimal_add(value, value, other). On failure, result is unchanged.

    Implementation: src/bigdecimal/arithmetic.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_abs(
    BigDecimal *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_negate(
    BigDecimal *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_add(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
);
BigDecimalStatus bigdecimal_sub(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
);
BigDecimalStatus bigdecimal_mul(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Aggregate arithmetic functions for BigDecimal sequences.

    sum and product are exact. mean forms an exact sum and rounds only a
    non-terminating quotient to the requested positive number of significant
    digits. At least one non-NULL value is required. Output/input aliasing is
    supported and failure preserves result.

    Implementation: src/bigdecimal/aggregation.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_sum(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count
);
BigDecimalStatus bigdecimal_product(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count
);
BigDecimalStatus bigdecimal_mean(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Rounded arithmetic operation functions for BigDecimal.

    A positive target scale keeps digits after the decimal point; a negative
    scale rounds to powers of ten. Stored results are normalized, so trailing
    zeroes are not retained. floor, ceil, and trunc round to an integer;
    round uses half-even at the requested number of decimal places. Negative
    places round to tens, hundreds, and larger powers of ten. Division by zero
    leaves result unchanged.

    Implementation: src/bigdecimal/division.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_rescale(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_floor(
    BigDecimal *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_ceil(
    BigDecimal *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_trunc(
    BigDecimal *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_round(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t places
);
BigDecimalStatus bigdecimal_div(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Integer conversion, inspection, selection, and power functions.

    Fractional input to to_bigint is rejected. pow accepts a non-negative
    integer exponent and defines 0^0 as 1. All outputs are preserved on
    failure, and output/input aliasing is supported where applicable.

    Implementation: src/bigdecimal/operations.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_from_bigint(
    BigDecimal *result,
    const BigInt *value
);
BigDecimalStatus bigdecimal_to_bigint(
    BigInt *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_is_integer(
    bool *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_sign(
    int *result,
    const BigDecimal *value
);
BigDecimalStatus bigdecimal_min(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
);
BigDecimalStatus bigdecimal_max(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
);
BigDecimalStatus bigdecimal_pow(
    BigDecimal *result,
    const BigDecimal *base,
    const BigInt *exponent
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Real-root functions with explicit significant digits and rounding.

    Exact finite decimal roots remain exact even beyond the requested digits.
    Degree must be positive; negative values require an odd degree. Irrational
    roots round to significant digits, not decimal places. Failure preserves
    result, output/input aliasing is supported, and calculator limits do not
    apply to these library calls.

    Implementation: src/bigdecimal/roots.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_root(
    BigDecimal *result,
    const BigDecimal *value,
    uint32_t degree,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_sqrt(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_cbrt(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Exponential and logarithmic functions with explicit significant digits.

    exp computes e^value. ln and log10 require a positive value. log requires
    a positive value and a positive base other than one. Irrational results
    are rounded to significant digits. Failure preserves result and
    output/input aliasing is supported.

    Implementation: src/bigdecimal/transcendental.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_exp(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_ln(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_log10(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_log(
    BigDecimal *result,
    const BigDecimal *value,
    const BigDecimal *base,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Trigonometric functions with radian arguments and significant digits.

    sin, cos, and tan accept any finite BigDecimal. asin and acos require a
    value in [-1, 1]; atan accepts any value. Inverse results are radians.
    Failure preserves result and output/input aliasing is supported.

    Implementation: src/bigdecimal/trigonometric.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_sin(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_cos(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_tan(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_asin(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_acos(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_atan(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Significant-digit division functions.

    The exact-first variant preserves finite quotients and rounds only
    recurring results. bigdecimal_div retains fixed-scale semantics.

    Implementation: src/bigdecimal/division.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_div_significant(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_div_exact_or_significant(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

typedef enum BigDecimalConstant
{
    BIGDECIMAL_CONSTANT_PI,
    BIGDECIMAL_CONSTANT_E,
    BIGDECIMAL_CONSTANT_PHI
} BigDecimalConstant;

/*
------------------------------------------------------------------------------------------------------------------------------
    Built-in constants and readable formatting.

    bigdecimal_set_constant returns the stored 500-decimal-place approximation.
    bigdecimal_set_constant_significant rounds that stored value through 500
    significant digits and calculates larger requests dynamically; digits must
    be positive. Constants remain approximations, not exact irrational values.
    Both factories preserve result on failure.

    Formatting uses ordinary notation for exponent magnitude below 10 and
    scientific notation otherwise. places applies
    after the decimal point, or after the scientific mantissa; -1 keeps all
    stored digits. The caller releases the returned string with free().

    Implementation: constants in src/bigdecimal/constants.c;
    formatting in src/bigdecimal/format.c.
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_set_constant(
    BigDecimal *result,
    BigDecimalConstant constant
);
BigDecimalStatus bigdecimal_set_constant_significant(
    BigDecimal *result,
    BigDecimalConstant constant,
    int64_t digits,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_format(
    const BigDecimal *value,
    int64_t places,
    BigDecimalRoundingMode rounding,
    char **result
);

#ifdef __cplusplus
}
#endif

#endif
