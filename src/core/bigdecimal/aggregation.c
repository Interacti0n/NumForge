#include <numforge/bigdecimal.h>

#include <stdio.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Exact sequence aggregation and a single rounded division for arithmetic
    means. Accumulation uses public alias-safe operations so this module stays
    independent of BigDecimal's private representation.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigDecimalStatus bigdecimal_validate_sequence(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count
)
{
    if (result == NULL || values == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (count == 0U)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    for (size_t index = 0U; index < count; index++)
    {
        if (values[index] == NULL)
        {
            return BIGDECIMAL_NULL_ARGUMENT;
        }
    }

    return BIGDECIMAL_OK;
}

static BigDecimalStatus bigdecimal_aggregate(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    bool multiply
)
{
    BigDecimal *accumulator;
    BigDecimalStatus status = bigdecimal_validate_sequence(result, values, count);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    accumulator = bigdecimal_create();

    if (accumulator == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_copy(accumulator, values[0]);

    for (size_t index = 1U; status == BIGDECIMAL_OK && index < count; index++)
    {
        status = multiply ? bigdecimal_mul(accumulator, accumulator, values[index])
                          : bigdecimal_add(accumulator, accumulator, values[index]);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_copy(result, accumulator);
    }

    bigdecimal_destroy(accumulator);
    return status;
}

BigDecimalStatus bigdecimal_sum(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count
)
{
    return bigdecimal_aggregate(result, values, count, false);
}

BigDecimalStatus bigdecimal_product(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count
)
{
    return bigdecimal_aggregate(result, values, count, true);
}

BigDecimalStatus bigdecimal_mean(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *sum;
    BigDecimal *divisor;
    BigDecimalStatus status = bigdecimal_validate_sequence(result, values, count);
    char count_text[32];

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (digits <= 0 ||
        rounding < BIGDECIMAL_ROUND_TOWARD_ZERO ||
        rounding > BIGDECIMAL_ROUND_HALF_EVEN)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    sum = bigdecimal_create();
    divisor = bigdecimal_create();

    if (sum == NULL || divisor == NULL)
    {
        bigdecimal_destroy(sum);
        bigdecimal_destroy(divisor);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    (void)snprintf(count_text, sizeof(count_text), "%zu", count);
    status = bigdecimal_sum(sum, values, count);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_set_string(divisor, count_text);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_div_exact_or_significant(
            result, sum, divisor, digits, rounding);
    }

    bigdecimal_destroy(sum);
    bigdecimal_destroy(divisor);
    return status;
}
