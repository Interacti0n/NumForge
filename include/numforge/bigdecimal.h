#ifndef NUMFORGE_BIGDECIMAL_H
#define NUMFORGE_BIGDECIMAL_H

#include <stdbool.h>
#include <numforge/bigint.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
------------------------------------------------------------------------------------------------------------------------------
    Opaque exact base-10 number. Values must be created and destroyed through
    this API; their coefficient and scale remain private implementation details.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef struct BigDecimal BigDecimal;

/*
------------------------------------------------------------------------------------------------------------------------------
    Status codes returned by BigDecimal operations.
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
    Rounded arithmetic operation functions for BigDecimal.

    A positive target scale keeps digits after the decimal point; a negative
    scale rounds to powers of ten. Stored results are normalized, so trailing
    zeroes are not retained. Division by zero leaves result unchanged.
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_rescale(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
);

/* Exact integer conversions; fractional input to to_bigint is rejected.
 * All outputs are preserved on failure. */
BigDecimalStatus bigdecimal_from_bigint(BigDecimal *result, const BigInt *value);
BigDecimalStatus bigdecimal_to_bigint(BigInt *result, const BigDecimal *value);
BigDecimalStatus bigdecimal_is_integer(bool *result, const BigDecimal *value);
BigDecimalStatus bigdecimal_sign(int *result, const BigDecimal *value);
BigDecimalStatus bigdecimal_min(BigDecimal *result, const BigDecimal *a, const BigDecimal *b);
BigDecimalStatus bigdecimal_max(BigDecimal *result, const BigDecimal *a, const BigDecimal *b);

/* Exact non-negative integer exponent; 0^0 is 1. Output/input aliasing works. */
BigDecimalStatus bigdecimal_pow(BigDecimal *result, const BigDecimal *base, const BigInt *exponent);

/* Real roots with explicit working significant digits (>=1) and rounding.
 * Exact finite decimal roots remain exact, even beyond requested digits.
 * Degree is a positive uint32_t; negative values require odd degree.
 * Irrational roots round to digits, not decimal places. Failure preserves
 * result; output/input aliasing is supported. No calculator limits apply. */
BigDecimalStatus bigdecimal_root(BigDecimal *result, const BigDecimal *value,
    uint32_t degree, int64_t digits, BigDecimalRoundingMode rounding);
BigDecimalStatus bigdecimal_sqrt(BigDecimal *result, const BigDecimal *value,
    int64_t digits, BigDecimalRoundingMode rounding);
BigDecimalStatus bigdecimal_cbrt(BigDecimal *result, const BigDecimal *value,
    int64_t digits, BigDecimalRoundingMode rounding);

/* Significant-digit division; exact variant preserves finite quotients and
 * rounds only recurring ones. Existing bigdecimal_div retains fixed scale. */
BigDecimalStatus bigdecimal_div_significant(BigDecimal *result, const BigDecimal *a,
    const BigDecimal *b, int64_t digits, BigDecimalRoundingMode rounding);
BigDecimalStatus bigdecimal_div_exact_or_significant(BigDecimal *result, const BigDecimal *a,
    const BigDecimal *b, int64_t digits, BigDecimalRoundingMode rounding);

typedef enum BigDecimalConstant
{
    BIGDECIMAL_CONSTANT_PI,
    BIGDECIMAL_CONSTANT_E,
    BIGDECIMAL_CONSTANT_PHI
} BigDecimalConstant;
/* Stored approximations with 500 decimal places, not exact irrational values. */
BigDecimalStatus bigdecimal_set_constant(BigDecimal *result, BigDecimalConstant constant);

/* Readable output: ordinary notation for exponent magnitude <10, scientific
 * otherwise. Places applies after the decimal point (of scientific mantissa
 * when applicable); -1 keeps all stored digits. Caller frees the returned
 * string with free(). On failure *result is unchanged. */
BigDecimalStatus bigdecimal_format(const BigDecimal *value, int64_t places,
    BigDecimalRoundingMode rounding, char **result);

BigDecimalStatus bigdecimal_div(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
);

#ifdef __cplusplus
}
#endif

#endif
