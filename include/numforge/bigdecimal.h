#ifndef NUMFORGE_BIGDECIMAL_H
#define NUMFORGE_BIGDECIMAL_H

#include <stdbool.h>
#include <stdint.h>

/* BigDecimal is an opaque exact base-10 number. */
typedef struct BigDecimal BigDecimal;

/* Status codes returned by BigDecimal operations.
 * BIGDECIMAL_NOT_IMPLEMENTED is reserved for future optional API areas. */
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

/* Rounding modes used by division and rescaling. */
typedef enum BigDecimalRoundingMode
{
    BIGDECIMAL_ROUND_TOWARD_ZERO = 0,
    BIGDECIMAL_ROUND_AWAY_FROM_ZERO,
    BIGDECIMAL_ROUND_FLOOR,
    BIGDECIMAL_ROUND_CEILING,
    BIGDECIMAL_ROUND_HALF_UP,
    BIGDECIMAL_ROUND_HALF_EVEN
} BigDecimalRoundingMode;

const char *bigdecimal_status_to_string(
    BigDecimalStatus status
);

/* Lifetime. bigdecimal_create() currently creates a normalized zero value.
 * bigdecimal_destroy() accepts NULL. */
BigDecimal *bigdecimal_create(
    void
);
void bigdecimal_destroy(
    BigDecimal *value
);

/* Conversion and copying. The caller owns a string returned through result
 * and must release it with free(). On failure, destination values are left
 * unchanged. */
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

/* Comparison and inspection. comparison receives a value less than, equal
 * to, or greater than zero; boolean results are written on success. */
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

/* Exact arithmetic. Input/output aliasing will be supported. */
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

/* Round value to target_scale decimal places. A positive target scale keeps
 * digits after the decimal point; a negative scale rounds to powers of ten. */
BigDecimalStatus bigdecimal_rescale(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
);

/* Divide a by b and round the result to target_scale decimal places. */
BigDecimalStatus bigdecimal_div(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
);

#endif
