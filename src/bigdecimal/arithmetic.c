#include "bigdecimal_internal.h"

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include <stdbool.h>
#include <stdint.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Exact arithmetic and sign operations for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_abs(
    BigDecimal *result,
    const BigDecimal *value
)
{
    BigDecimal *temporary;
    BigDecimalStatus status;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    temporary = bigdecimal_create();

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_from_bigint_status(bigint_abs(temporary->coefficient, value->coefficient));

    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = value->scale;
        bigdecimal_commit(result, temporary);
    }
    else
    {
        bigdecimal_destroy(temporary);
    }

    return status;
}

BigDecimalStatus bigdecimal_negate(
    BigDecimal *result,
    const BigDecimal *value
)
{
    BigDecimal *temporary;
    BigDecimalStatus status;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    temporary = bigdecimal_create();

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_from_bigint_status(bigint_negate(temporary->coefficient, value->coefficient));

    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = value->scale;
        bigdecimal_commit(result, temporary);
    }
    else
    {
        bigdecimal_destroy(temporary);
    }

    return status;
}

static BigDecimalStatus bigdecimal_add_or_subtract(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    bool subtract
)
{
    BigDecimal *temporary;
    BigInt *scaled_a = NULL;
    BigInt *scaled_b = NULL;
    int64_t scale = a->scale >= b->scale ? a->scale : b->scale;
    uint64_t difference;
    BigDecimalStatus status;

    if (bigint_is_zero(a->coefficient))
    {
        return subtract ? bigdecimal_negate(result, b) : bigdecimal_copy(result, b);
    }
    if (bigint_is_zero(b->coefficient))
    {
        return bigdecimal_copy(result, a);
    }

    temporary = bigdecimal_create();
    scaled_a = bigint_create();
    scaled_b = bigint_create();
    if (temporary == NULL || scaled_a == NULL || scaled_b == NULL)
    {
        bigdecimal_destroy(temporary);
        bigint_destroy(scaled_a);
        bigint_destroy(scaled_b);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_scale_difference(scale, a->scale, &difference);
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_multiply_power_of_ten(scaled_a, a->coefficient, difference);
    }
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_scale_difference(scale, b->scale, &difference);
    }
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_multiply_power_of_ten(scaled_b, b->coefficient, difference);
    }
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(
            subtract ? bigint_sub(temporary->coefficient, scaled_a, scaled_b)
                     : bigint_add(temporary->coefficient, scaled_a, scaled_b));
    }

    bigint_destroy(scaled_a);
    bigint_destroy(scaled_b);
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = scale;
        return bigdecimal_finish(result, temporary);
    }

    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_add(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    return bigdecimal_add_or_subtract(result, a, b, false);
}

BigDecimalStatus bigdecimal_sub(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    return bigdecimal_add_or_subtract(result, a, b, true);
}

BigDecimalStatus bigdecimal_mul(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    BigDecimal *temporary;
    BigDecimalStatus status;
    int64_t scale;

    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    temporary = bigdecimal_create();

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_from_bigint_status(
        bigint_mul(temporary->coefficient, a->coefficient, b->coefficient));

    if (status == BIGDECIMAL_OK)
    {
        // Normalize the product coefficient before adding operand scales.
        // Cross-factor trailing zeros can make an overflowing sum representable.
        status = bigdecimal_normalize(temporary);

        if (status == BIGDECIMAL_OK && !bigint_is_zero(temporary->coefficient))
        {
            int64_t adjustment = temporary->scale;

            if (!((bigdecimal_i64_add(a->scale, adjustment, &scale) &&
                   bigdecimal_i64_add(scale, b->scale, &scale)) ||
                  (bigdecimal_i64_add(b->scale, adjustment, &scale) &&
                   bigdecimal_i64_add(scale, a->scale, &scale)) ||
                  (bigdecimal_i64_add(a->scale, b->scale, &scale) &&
                   bigdecimal_i64_add(scale, adjustment, &scale))))
            {
                status = BIGDECIMAL_SCALE_OVERFLOW;
            }
            else
            {
                temporary->scale = scale;
            }
        }

        if (status == BIGDECIMAL_OK)
        {
            bigdecimal_commit(result, temporary);
            return BIGDECIMAL_OK;
        }
    }

    bigdecimal_destroy(temporary);
    return status;
}
