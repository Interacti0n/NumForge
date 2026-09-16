#include <numforge/bigdecimal.h>
#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Exponential and logarithmic functions. Argument reduction keeps both
    series close to their fastest-converging region. Every intermediate value
    is rounded to a guarded significant precision; binary floating point is
    never used.
------------------------------------------------------------------------------------------------------------------------------
*/

#define BIGDECIMAL_TRANSCENDENTAL_GUARD_DIGITS 24
#define BIGDECIMAL_TRANSCENDENTAL_MAX_REDUCTIONS 256U

#define TRANSCENDENTAL_TRY(operation)         \
    do                                       \
    {                                        \
        status = (operation);                \
        if (status != BIGDECIMAL_OK)         \
        {                                    \
            goto cleanup;                    \
        }                                    \
    } while (0)

static BigDecimalStatus round_significant(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    char *coefficient;
    const char *number;
    size_t length;
    size_t discarded;
    int64_t target_scale;

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_copy(result, value);
    }

    coefficient = bigint_to_string(value->coefficient);

    if (coefficient == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    number = coefficient[0] == '-' ? coefficient + 1 : coefficient;
    length = strlen(number);

    if ((uint64_t)length <= (uint64_t)digits)
    {
        free(coefficient);
        return bigdecimal_copy(result, value);
    }

    discarded = length - (size_t)digits;
    free(coefficient);

    if (discarded > (size_t)INT64_MAX ||
        !bigdecimal_i64_sub(value->scale, (int64_t)discarded, &target_scale))
    {
        return BIGDECIMAL_SCALE_OVERFLOW;
    }

    return bigdecimal_rescale(result, value, target_scale, rounding);
}

static BigDecimalStatus multiply_significant(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    BigDecimalRoundingMode rounding
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
        status = round_significant(result, temporary, digits, rounding);
    }

    bigdecimal_destroy(temporary);
    return status;
}

static BigDecimalStatus add_significant(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *temporary = bigdecimal_create();
    BigDecimalStatus status;

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_add(temporary, a, b);

    if (status == BIGDECIMAL_OK)
    {
        status = round_significant(result, temporary, digits, rounding);
    }

    bigdecimal_destroy(temporary);
    return status;
}

static BigDecimalStatus set_integer(BigDecimal *value, uint64_t integer)
{
    char text[32];

    (void)snprintf(text, sizeof(text), "%" PRIu64, integer);
    return bigdecimal_set_string(value, text);
}

static BigDecimalStatus set_exact_result(BigDecimal *result, const char *text)
{
    BigDecimal *temporary = bigdecimal_create();
    BigDecimalStatus status;

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_set_string(temporary, text);

    if (status == BIGDECIMAL_OK)
    {
        bigdecimal_commit(result, temporary);
        return BIGDECIMAL_OK;
    }

    bigdecimal_destroy(temporary);
    return status;
}

static BigDecimalStatus validate_precision(
    int64_t digits,
    BigDecimalRoundingMode rounding,
    int64_t *work_digits
)
{
    if (digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (!bigdecimal_i64_add(
            digits, BIGDECIMAL_TRANSCENDENTAL_GUARD_DIGITS, work_digits))
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    return BIGDECIMAL_OK;
}

/* ln(x) = 2(z + z^3/3 + z^5/5 + ...), where z = (x - 1)/(x + 1).
 * Repeated square roots first move x into [0.9, 1.1]. */
static BigDecimalStatus logarithm_series(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t work_digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *reduced = bigdecimal_create();
    BigDecimal *one = bigdecimal_create();
    BigDecimal *lower_bound = bigdecimal_create();
    BigDecimal *upper_bound = bigdecimal_create();
    BigDecimal *numerator = bigdecimal_create();
    BigDecimal *denominator = bigdecimal_create();
    BigDecimal *z = bigdecimal_create();
    BigDecimal *z_squared = bigdecimal_create();
    BigDecimal *term = bigdecimal_create();
    BigDecimal *fraction = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimal *previous = bigdecimal_create();
    BigDecimal *order = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    unsigned reductions = 0U;
    uint64_t index = 1U;
    int comparison = 0;

    if (reduced == NULL || one == NULL || lower_bound == NULL || upper_bound == NULL ||
        numerator == NULL || denominator == NULL || z == NULL || z_squared == NULL ||
        term == NULL || fraction == NULL || sum == NULL || previous == NULL || order == NULL)
    {
        goto cleanup;
    }

    TRANSCENDENTAL_TRY(bigdecimal_copy(reduced, value));
    TRANSCENDENTAL_TRY(bigdecimal_set_string(one, "1"));
    TRANSCENDENTAL_TRY(bigdecimal_set_string(lower_bound, "0.9"));
    TRANSCENDENTAL_TRY(bigdecimal_set_string(upper_bound, "1.1"));

    for (;;)
    {
        int lower;
        int upper;

        TRANSCENDENTAL_TRY(bigdecimal_compare(&lower, reduced, lower_bound));
        TRANSCENDENTAL_TRY(bigdecimal_compare(&upper, reduced, upper_bound));

        if (lower >= 0 && upper <= 0)
        {
            break;
        }

        if (reductions == BIGDECIMAL_TRANSCENDENTAL_MAX_REDUCTIONS)
        {
            status = BIGDECIMAL_VALUE_TOO_LARGE;
            goto cleanup;
        }

        if (!numforge_budget_check())
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        TRANSCENDENTAL_TRY(
            bigdecimal_sqrt(reduced, reduced, work_digits, rounding));
        reductions++;
    }

    TRANSCENDENTAL_TRY(bigdecimal_sub(numerator, reduced, one));
    TRANSCENDENTAL_TRY(bigdecimal_add(denominator, reduced, one));
    TRANSCENDENTAL_TRY(bigdecimal_div_significant(
        z, numerator, denominator, work_digits, rounding));
    TRANSCENDENTAL_TRY(multiply_significant(
        z_squared, z, z, work_digits, rounding));
    TRANSCENDENTAL_TRY(bigdecimal_copy(term, z));
    TRANSCENDENTAL_TRY(bigdecimal_copy(sum, z));

    for (;;)
    {
        if (!numforge_budget_check())
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        if (index > UINT64_MAX - 2U)
        {
            status = BIGDECIMAL_VALUE_TOO_LARGE;
            goto cleanup;
        }

        index += 2U;
        TRANSCENDENTAL_TRY(multiply_significant(
            term, term, z_squared, work_digits, rounding));
        TRANSCENDENTAL_TRY(set_integer(order, index));
        TRANSCENDENTAL_TRY(bigdecimal_div_significant(
            fraction, term, order, work_digits, rounding));
        TRANSCENDENTAL_TRY(bigdecimal_copy(previous, sum));
        TRANSCENDENTAL_TRY(add_significant(
            sum, sum, fraction, work_digits, rounding));
        TRANSCENDENTAL_TRY(bigdecimal_compare(&comparison, sum, previous));

        if (comparison == 0)
        {
            break;
        }
    }

    TRANSCENDENTAL_TRY(bigdecimal_set_string(order, "2"));
    TRANSCENDENTAL_TRY(multiply_significant(
        sum, sum, order, work_digits, rounding));

    for (unsigned index_reduction = 0U; index_reduction < reductions; index_reduction++)
    {
        TRANSCENDENTAL_TRY(add_significant(
            sum, sum, sum, work_digits, rounding));
    }

    TRANSCENDENTAL_TRY(bigdecimal_copy(result, sum));

cleanup:
    bigdecimal_destroy(reduced);
    bigdecimal_destroy(one);
    bigdecimal_destroy(lower_bound);
    bigdecimal_destroy(upper_bound);
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(z);
    bigdecimal_destroy(z_squared);
    bigdecimal_destroy(term);
    bigdecimal_destroy(fraction);
    bigdecimal_destroy(sum);
    bigdecimal_destroy(previous);
    bigdecimal_destroy(order);

    return status;
}

static BigDecimalStatus exponential_series(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t work_digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *reduced = bigdecimal_create();
    BigDecimal *absolute = bigdecimal_create();
    BigDecimal *half = bigdecimal_create();
    BigDecimal *term = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimal *previous = bigdecimal_create();
    BigDecimal *order = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    unsigned reductions = 0U;
    uint64_t index = 0U;
    int comparison = 0;

    if (reduced == NULL || absolute == NULL || half == NULL || term == NULL ||
        sum == NULL || previous == NULL || order == NULL)
    {
        goto cleanup;
    }

    TRANSCENDENTAL_TRY(bigdecimal_copy(reduced, value));
    TRANSCENDENTAL_TRY(bigdecimal_set_string(half, "0.5"));
    TRANSCENDENTAL_TRY(bigdecimal_set_string(order, "2"));

    for (;;)
    {
        TRANSCENDENTAL_TRY(bigdecimal_abs(absolute, reduced));
        TRANSCENDENTAL_TRY(bigdecimal_compare(&comparison, absolute, half));

        if (comparison <= 0)
        {
            break;
        }

        if (reductions == BIGDECIMAL_TRANSCENDENTAL_MAX_REDUCTIONS)
        {
            status = BIGDECIMAL_VALUE_TOO_LARGE;
            goto cleanup;
        }

        TRANSCENDENTAL_TRY(bigdecimal_div_exact_or_significant(
            reduced, reduced, order, work_digits, rounding));
        reductions++;
    }

    TRANSCENDENTAL_TRY(bigdecimal_set_string(term, "1"));
    TRANSCENDENTAL_TRY(bigdecimal_set_string(sum, "1"));
    TRANSCENDENTAL_TRY(bigdecimal_set_string(order, "2"));

    for (;;)
    {
        if (!numforge_budget_check() || index == UINT64_MAX)
        {
            status = index == UINT64_MAX ? BIGDECIMAL_VALUE_TOO_LARGE
                                         : BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        index++;
        TRANSCENDENTAL_TRY(multiply_significant(
            term, term, reduced, work_digits, rounding));
        TRANSCENDENTAL_TRY(set_integer(order, index));
        TRANSCENDENTAL_TRY(bigdecimal_div_significant(
            term, term, order, work_digits, rounding));
        TRANSCENDENTAL_TRY(bigdecimal_copy(previous, sum));
        TRANSCENDENTAL_TRY(add_significant(
            sum, sum, term, work_digits, rounding));
        TRANSCENDENTAL_TRY(bigdecimal_compare(&comparison, sum, previous));

        if (comparison == 0)
        {
            break;
        }
    }

    for (unsigned reduction = 0U; reduction < reductions; reduction++)
    {
        TRANSCENDENTAL_TRY(multiply_significant(
            sum, sum, sum, work_digits, rounding));
    }

    TRANSCENDENTAL_TRY(bigdecimal_copy(result, sum));

cleanup:
    bigdecimal_destroy(reduced);
    bigdecimal_destroy(absolute);
    bigdecimal_destroy(half);
    bigdecimal_destroy(term);
    bigdecimal_destroy(sum);
    bigdecimal_destroy(previous);
    bigdecimal_destroy(order);

    return status;
}

BigDecimalStatus bigdecimal_exp(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *temporary;
    BigDecimalStatus status;
    int64_t work_digits;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = validate_precision(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return set_exact_result(result, "1");
    }

    temporary = bigdecimal_create();

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = exponential_series(
        temporary, value, work_digits, BIGDECIMAL_ROUND_HALF_EVEN);

    if (status == BIGDECIMAL_OK)
    {
        status = round_significant(temporary, temporary, digits, rounding);
    }

    if (status == BIGDECIMAL_OK)
    {
        bigdecimal_commit(result, temporary);
        temporary = NULL;
    }

    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_ln(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *temporary;
    BigDecimalStatus status;
    int64_t work_digits;
    int sign = 0;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = validate_precision(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    status = bigdecimal_sign(&sign, value);

    if (status != BIGDECIMAL_OK || sign <= 0)
    {
        return status == BIGDECIMAL_OK ? BIGDECIMAL_INVALID_ARGUMENT : status;
    }

    if (value->scale == 0 && bigint_is_one(value->coefficient))
    {
        return set_exact_result(result, "0");
    }

    temporary = bigdecimal_create();

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = logarithm_series(
        temporary, value, work_digits, BIGDECIMAL_ROUND_HALF_EVEN);

    if (status == BIGDECIMAL_OK)
    {
        status = round_significant(temporary, temporary, digits, rounding);
    }

    if (status == BIGDECIMAL_OK)
    {
        bigdecimal_commit(result, temporary);
        temporary = NULL;
    }

    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_log10(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *base;
    BigDecimalStatus status;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    base = bigdecimal_create();

    if (base == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_set_string(base, "10");

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_log(result, value, base, digits, rounding);
    }

    bigdecimal_destroy(base);
    return status;
}

BigDecimalStatus bigdecimal_log(
    BigDecimal *result,
    const BigDecimal *value,
    const BigDecimal *base,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *numerator;
    BigDecimal *denominator;
    BigDecimal *temporary;
    BigDecimalStatus status;
    int64_t work_digits;
    int value_sign = 0;
    int base_sign = 0;
    int base_comparison = 0;
    int value_comparison = 0;
    int values_equal = 0;

    if (result == NULL || value == NULL || base == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = validate_precision(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    status = bigdecimal_sign(&value_sign, value);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_sign(&base_sign, base);
    }

    if (status == BIGDECIMAL_OK)
    {
        BigDecimal *one = bigdecimal_create();

        if (one == NULL)
        {
            return BIGDECIMAL_OUT_OF_MEMORY;
        }

        status = bigdecimal_set_string(one, "1");

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_compare(&base_comparison, base, one);
        }

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_compare(&value_comparison, value, one);
        }

        bigdecimal_destroy(one);
    }

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (value_sign <= 0 || base_sign <= 0 || base_comparison == 0)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }


    if (value_comparison == 0)
    {
        return set_exact_result(result, "0");
    }

    status = bigdecimal_compare(&values_equal, value, base);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (values_equal == 0)
    {
        return set_exact_result(result, "1");
    }

    numerator = bigdecimal_create();
    denominator = bigdecimal_create();
    temporary = bigdecimal_create();

    if (numerator == NULL || denominator == NULL || temporary == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    TRANSCENDENTAL_TRY(logarithm_series(
        numerator, value, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    TRANSCENDENTAL_TRY(logarithm_series(
        denominator, base, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    TRANSCENDENTAL_TRY(bigdecimal_div_significant(
        temporary, numerator, denominator, digits, rounding));
    bigdecimal_commit(result, temporary);
    temporary = NULL;

cleanup:
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(temporary);

    return status;
}

#undef TRANSCENDENTAL_TRY
