#include "src/core/bigdecimal/bigdecimal_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>

/*
 * Public decimal conversions and exact operations. Temporary destinations
 * preserve caller values on failure, including output/input aliasing.
 */

static BigDecimalStatus from_integer_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK:
            return BIGDECIMAL_OK;
        case BIGINT_NULL_ARGUMENT:
            return BIGDECIMAL_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return BIGDECIMAL_OUT_OF_MEMORY;
        case BIGINT_VALUE_TOO_LARGE:
            return BIGDECIMAL_VALUE_TOO_LARGE;
        default:
            return BIGDECIMAL_INVALID_ARGUMENT;
    }
}

BigDecimalStatus bigdecimal_from_bigint(
    BigDecimal *result,
    const BigInt *value
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

    status = from_integer_status(bigint_copy(temporary->coefficient, value));
    if (status != BIGDECIMAL_OK)
    {
        bigdecimal_destroy(temporary);
        return status;
    }

    /* Copy the exact coefficient without materializing its decimal text.
     * Finish owns the temporary and preserves result if normalization fails. */
    temporary->scale = 0;
    return bigdecimal_finish(result, temporary);
}

BigDecimalStatus bigdecimal_to_bigint(
    BigInt *result,
    const BigDecimal *value
)
{
    char *text = NULL;
    BigDecimalStatus status;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (value->scale > 0)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    status = bigdecimal_to_string(value, &text);

    if (status == BIGDECIMAL_OK)
    {
        status = from_integer_status(bigint_set_string(result, text));
    }

    free(text);

    return status;
}

BigDecimalStatus bigdecimal_is_integer(
    bool *result,
    const BigDecimal *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    *result = value->scale <= 0;

    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_sign(
    int *result,
    const BigDecimal *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (bigint_is_zero(value->coefficient))
    {
        *result = 0;
    }
    else
    {
        *result = bigint_is_negative(value->coefficient) ? -1 : 1;
    }

    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_min(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    int comparison;
    BigDecimalStatus status;

    if (result == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = bigdecimal_compare(&comparison, a, b);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    return bigdecimal_copy(result, comparison <= 0 ? a : b);
}

BigDecimalStatus bigdecimal_max(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    int comparison;
    BigDecimalStatus status;

    if (result == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = bigdecimal_compare(&comparison, a, b);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    return bigdecimal_copy(result, comparison >= 0 ? a : b);
}

BigDecimalStatus bigdecimal_pow(
    BigDecimal *result,
    const BigDecimal *base,
    const BigInt *exponent
)
{
    BigInt *remaining;
    BigDecimal *accumulator;
    BigDecimal *factor;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;

    if (result == NULL || base == NULL || exponent == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (bigint_is_negative(exponent))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    remaining = bigint_create();
    accumulator = bigdecimal_create();
    factor = bigdecimal_create();

    if (remaining == NULL || accumulator == NULL || factor == NULL)
    {
        goto cleanup;
    }

    status = from_integer_status(bigint_copy(remaining, exponent));

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_set_string(accumulator, "1");
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_copy(factor, base);
    }

    while (status == BIGDECIMAL_OK && !bigint_is_zero(remaining))
    {
        if (!numforge_budget_check())
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
            break;
        }

        if (bigint_is_odd(remaining))
        {
            status = bigdecimal_mul(accumulator, accumulator, factor);
        }

        if (status == BIGDECIMAL_OK)
        {
            status = from_integer_status(
                bigint_shift_right(remaining, remaining, 1U));
        }

        if (status == BIGDECIMAL_OK && !bigint_is_zero(remaining))
        {
            status = bigdecimal_mul(factor, factor, factor);
        }
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_copy(result, accumulator);
    }

cleanup:
    bigint_destroy(remaining);
    bigdecimal_destroy(accumulator);
    bigdecimal_destroy(factor);

    return status;
}

BigDecimalStatus bigdecimal_pow_signed(
    BigDecimal *result,
    const BigDecimal *base,
    const BigInt *exponent,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigInt *magnitude = NULL;
    BigDecimal *power = NULL;
    BigDecimal *one = NULL;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;

    if (result == NULL || base == NULL || exponent == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (!bigint_is_negative(exponent))
    {
        return bigdecimal_pow(result, base, exponent);
    }

    if (bigint_is_zero(base->coefficient))
    {
        return BIGDECIMAL_DIVISION_BY_ZERO;
    }

    magnitude = bigint_create();
    power = bigdecimal_create();
    one = bigdecimal_create();

    if (magnitude == NULL || power == NULL || one == NULL)
    {
        goto cleanup;
    }

    status = from_integer_status(bigint_abs(magnitude, exponent));

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_pow(power, base, magnitude);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_set_string(one, "1");
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_div_exact_or_significant(
            result, one, power, digits, rounding);
    }

cleanup:
    bigint_destroy(magnitude);
    bigdecimal_destroy(power);
    bigdecimal_destroy(one);
    return status;
}

/* Shared guarded intermediates; preserve the destination through rounding failures. */
BigDecimalStatus bigdecimal_binary_rounded(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    bool subtract
)
{
    BigDecimal *temporary = bigdecimal_create();
    BigDecimalStatus status;

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = subtract ? bigdecimal_sub(temporary, a, b)
                      : bigdecimal_add(temporary, a, b);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_significant(
            result, temporary, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    }

    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_multiply_rounded(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits
)
{
    BigDecimal *temporary = bigdecimal_create();
    BigDecimalStatus status;

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_mul(temporary, a, b);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_significant(
            result, temporary, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    }

    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_set_u64(
    BigDecimal *value,
    uint64_t integer
)
{
    char text[32];

    (void)snprintf(text, sizeof(text), "%" PRIu64, integer);
    return bigdecimal_set_string(value, text);
}
