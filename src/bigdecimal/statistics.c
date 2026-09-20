#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <numforge/bigdecimal.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define BIGDECIMAL_STATISTICS_GUARD_DIGITS INT64_C(16)

/*
------------------------------------------------------------------------------------------------------------------------------
    Decimal order statistics, specialized means, and exact sufficient
    statistics. Median sorts references only. Geometric mean roots the exact
    product; harmonic mean keeps its reciprocal sum as an exact fraction.

    For d_i = x_i - x_0:
    variance_population = (n * sum(d_i^2) - sum(d_i)^2) / n^2
    variance_sample     = (n * sum(d_i^2) - sum(d_i)^2) / (n * (n - 1))

    Translation keeps cancellation exact while avoiding unnecessarily large
    squares when the inputs share a large common offset.
    Standard deviations round the variance internally with guard digits and
    apply the caller's rounding mode only to the final square root.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigDecimalStatus statistics_validate(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    size_t minimum_count,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    if (result == NULL || values == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (count < minimum_count || digits <= 0 || !bigdecimal_valid_rounding(rounding))
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

static BigDecimalStatus statistics_set_count(
    BigDecimal *value,
    size_t count
)
{
    char text[32];

    (void)snprintf(text, sizeof(text), "%zu", count);
    return bigdecimal_set_string(value, text);
}

static BigDecimalStatus statistics_validate_non_negative(
    const BigDecimal *const *values,
    size_t count,
    bool allow_zero,
    bool *contains_zero
)
{
    if (contains_zero != NULL)
    {
        *contains_zero = false;
    }

    for (size_t index = 0U; index < count; index++)
    {
        bool negative = false;
        bool zero = false;
        BigDecimalStatus status = bigdecimal_is_negative(&negative, values[index]);

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_is_zero(&zero, values[index]);
        }

        if (status != BIGDECIMAL_OK)
        {
            return status;
        }

        if (negative || (!allow_zero && zero))
        {
            return BIGDECIMAL_INVALID_ARGUMENT;
        }

        if (zero && contains_zero != NULL)
        {
            *contains_zero = true;
        }
    }

    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_median(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count
)
{
    const BigDecimal **ordered;
    BigDecimal *sum = NULL;
    BigDecimal *half = NULL;
    BigDecimalStatus status = statistics_validate(
        result, values, count, 1U, 1, BIGDECIMAL_ROUND_HALF_EVEN);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (count > SIZE_MAX / sizeof(*ordered))
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    ordered = numforge_malloc(count * sizeof(*ordered));

    if (ordered == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    for (size_t index = 0U; index < count; index++)
    {
        ordered[index] = values[index];
    }

    for (size_t index = 1U; status == BIGDECIMAL_OK && index < count; index++)
    {
        const BigDecimal *selected = ordered[index];
        size_t position = index;

        while (position > 0U)
        {
            int comparison;

            status = bigdecimal_compare(&comparison, ordered[position - 1U], selected);

            if (status != BIGDECIMAL_OK || comparison <= 0)
            {
                break;
            }

            ordered[position] = ordered[position - 1U];
            position--;
        }

        ordered[position] = selected;
    }

    if (status == BIGDECIMAL_OK && count % 2U != 0U)
    {
        status = bigdecimal_copy(result, ordered[count / 2U]);
    }
    else if (status == BIGDECIMAL_OK)
    {
        sum = bigdecimal_create();
        half = bigdecimal_create();

        if (sum == NULL || half == NULL)
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
        }
        else
        {
            status = bigdecimal_add(sum, ordered[count / 2U - 1U], ordered[count / 2U]);

            if (status == BIGDECIMAL_OK)
            {
                status = bigdecimal_set_string(half, "0.5");
            }

            if (status == BIGDECIMAL_OK)
            {
                status = bigdecimal_mul(result, sum, half);
            }
        }
    }

    bigdecimal_destroy(sum);
    bigdecimal_destroy(half);
    free(ordered);
    return status;
}

BigDecimalStatus bigdecimal_geometric_mean(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *product;
    bool contains_zero;
    BigDecimalStatus status = statistics_validate(
        result, values, count, 1U, digits, rounding);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

#if SIZE_MAX > UINT32_MAX
    if (count > UINT32_MAX)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }
#endif

    status = statistics_validate_non_negative(values, count, true, &contains_zero);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (contains_zero)
    {
        return bigdecimal_set_string(result, "0");
    }

    product = bigdecimal_create();

    if (product == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_product(product, values, count);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_root(result, product, (uint32_t)count, digits, rounding);
    }

    bigdecimal_destroy(product);
    return status;
}

BigDecimalStatus bigdecimal_harmonic_mean(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *numerator = NULL;
    BigDecimal *denominator = NULL;
    BigDecimal *next_numerator = NULL;
    BigDecimal *next_denominator = NULL;
    BigDecimal *count_value = NULL;
    BigDecimal *scaled_denominator = NULL;
    BigDecimalStatus status = statistics_validate(
        result, values, count, 1U, digits, rounding);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    status = statistics_validate_non_negative(values, count, false, NULL);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    numerator = bigdecimal_create();
    denominator = bigdecimal_create();
    next_numerator = bigdecimal_create();
    next_denominator = bigdecimal_create();
    count_value = bigdecimal_create();
    scaled_denominator = bigdecimal_create();

    if (numerator == NULL || denominator == NULL || next_numerator == NULL ||
        next_denominator == NULL || count_value == NULL || scaled_denominator == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    status = bigdecimal_set_string(denominator, "1");

    for (size_t index = 0U; status == BIGDECIMAL_OK && index < count; index++)
    {
        BigDecimal *swap;

        status = bigdecimal_mul(next_numerator, numerator, values[index]);

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_add(next_numerator, next_numerator, denominator);
        }

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_mul(next_denominator, denominator, values[index]);
        }

        if (status == BIGDECIMAL_OK)
        {
            swap = numerator;
            numerator = next_numerator;
            next_numerator = swap;
            swap = denominator;
            denominator = next_denominator;
            next_denominator = swap;
        }
    }

    if (status == BIGDECIMAL_OK)
    {
        status = statistics_set_count(count_value, count);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_mul(scaled_denominator, denominator, count_value);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_div_exact_or_significant(
            result, scaled_denominator, numerator, digits, rounding);
    }

cleanup:
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(next_numerator);
    bigdecimal_destroy(next_denominator);
    bigdecimal_destroy(count_value);
    bigdecimal_destroy(scaled_denominator);
    return status;
}

static BigDecimalStatus statistics_variance(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    bool sample,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *sum_deviations = NULL;
    BigDecimal *sum_squares = NULL;
    BigDecimal *difference = NULL;
    BigDecimal *square = NULL;
    BigDecimal *count_value = NULL;
    BigDecimal *other_count = NULL;
    BigDecimal *left = NULL;
    BigDecimal *right = NULL;
    BigDecimal *numerator = NULL;
    BigDecimal *denominator = NULL;
    BigDecimalStatus status = statistics_validate(
        result, values, count, sample ? 2U : 1U, digits, rounding);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    sum_deviations = bigdecimal_create();
    sum_squares = bigdecimal_create();
    difference = bigdecimal_create();
    square = bigdecimal_create();
    count_value = bigdecimal_create();
    other_count = bigdecimal_create();
    left = bigdecimal_create();
    right = bigdecimal_create();
    numerator = bigdecimal_create();
    denominator = bigdecimal_create();

    if (sum_deviations == NULL || sum_squares == NULL || difference == NULL ||
        square == NULL || count_value == NULL || other_count == NULL || left == NULL ||
        right == NULL || numerator == NULL || denominator == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    for (size_t index = 1U; status == BIGDECIMAL_OK && index < count; index++)
    {
        status = bigdecimal_sub(difference, values[index], values[0]);

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_add(sum_deviations, sum_deviations, difference);
        }

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_mul(square, difference, difference);
        }

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_add(sum_squares, sum_squares, square);
        }
    }

    if (status == BIGDECIMAL_OK)
    {
        status = statistics_set_count(count_value, count);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = statistics_set_count(other_count, sample ? count - 1U : count);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_mul(left, count_value, sum_squares);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_mul(right, sum_deviations, sum_deviations);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_sub(numerator, left, right);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_mul(denominator, count_value, other_count);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_div_exact_or_significant(
            result, numerator, denominator, digits, rounding);
    }

cleanup:
    bigdecimal_destroy(sum_deviations);
    bigdecimal_destroy(sum_squares);
    bigdecimal_destroy(difference);
    bigdecimal_destroy(square);
    bigdecimal_destroy(count_value);
    bigdecimal_destroy(other_count);
    bigdecimal_destroy(left);
    bigdecimal_destroy(right);
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    return status;
}

static BigDecimalStatus statistics_standard_deviation(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    bool sample,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *variance;
    BigDecimalStatus status = statistics_validate(
        result, values, count, sample ? 2U : 1U, digits, rounding);
    int64_t work_digits;

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (!bigdecimal_i64_add(digits, BIGDECIMAL_STATISTICS_GUARD_DIGITS, &work_digits))
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    variance = bigdecimal_create();

    if (variance == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = statistics_variance(
        variance,
        values,
        count,
        sample,
        work_digits,
        BIGDECIMAL_ROUND_HALF_EVEN);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_sqrt(result, variance, digits, rounding);
    }

    bigdecimal_destroy(variance);
    return status;
}

BigDecimalStatus bigdecimal_variance_population(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return statistics_variance(result, values, count, false, digits, rounding);
}

BigDecimalStatus bigdecimal_standard_deviation_population(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return statistics_standard_deviation(result, values, count, false, digits, rounding);
}

BigDecimalStatus bigdecimal_standard_deviation_sample(
    BigDecimal *result,
    const BigDecimal *const *values,
    size_t count,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return statistics_standard_deviation(result, values, count, true, digits, rounding);
}
