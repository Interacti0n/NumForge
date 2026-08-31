#ifndef NUMFORGE_BIGDECIMAL_H
#define NUMFORGE_BIGDECIMAL_H

#include <stdbool.h>
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

    BIGDECIMAL_NOT_IMPLEMENTED is reserved for future optional API areas.
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
    BIGDECIMAL_SCALE_OVERFLOW,
    BIGDECIMAL_NOT_IMPLEMENTED
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
