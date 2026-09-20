#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <numforge/bigdecimal.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BIGDECIMAL_HYPERBOLIC_GUARD_DIGITS INT64_C(24)

#define HYPERBOLIC_TRY(operation)             \
    do                                       \
    {                                        \
        status = (operation);                \
        if (status != BIGDECIMAL_OK)         \
        {                                    \
            goto cleanup;                    \
        }                                    \
    } while (0)

/*
------------------------------------------------------------------------------------------------------------------------------
    Guarded helpers. Intermediate arithmetic uses half-even rounding; the
    caller-selected mode is applied only once to the public result.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigDecimalStatus hyperbolic_work_digits(
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
            digits, BIGDECIMAL_HYPERBOLIC_GUARD_DIGITS, work_digits))
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    return BIGDECIMAL_OK;
}

static BigDecimalStatus hyperbolic_set_u64(
    BigDecimal *value,
    uint64_t integer
)
{
    char text[32];

    (void)snprintf(text, sizeof(text), "%" PRIu64, integer);
    return bigdecimal_set_string(value, text);
}

static BigDecimalStatus hyperbolic_binary_rounded(
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

static BigDecimalStatus hyperbolic_multiply_rounded(
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

static BigDecimalStatus hyperbolic_finish(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *rounded = bigdecimal_create();
    BigDecimalStatus status;

    if (rounded == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_round_significant(rounded, value, digits, rounding);

    if (status == BIGDECIMAL_OK)
    {
        bigdecimal_commit(result, rounded);
        rounded = NULL;
    }

    bigdecimal_destroy(rounded);
    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Stable power series around zero.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigDecimalStatus hyperbolic_sinh_series(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits
)
{
    BigDecimal *square = bigdecimal_create();
    BigDecimal *term = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimal *next = bigdecimal_create();
    BigDecimal *divisor = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    uint64_t limit = (uint64_t)digits + UINT64_C(64);
    int comparison = 0;

    if (square == NULL || term == NULL || sum == NULL || next == NULL || divisor == NULL)
    {
        goto cleanup;
    }

    HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
        square, value, value, digits));
    HYPERBOLIC_TRY(bigdecimal_copy(term, value));
    HYPERBOLIC_TRY(bigdecimal_copy(sum, value));

    for (uint64_t iteration = 1U; iteration <= limit; iteration++)
    {
        uint64_t even;

        if (!numforge_budget_check() || iteration > (UINT64_MAX - 1U) / 2U)
        {
            status = iteration > (UINT64_MAX - 1U) / 2U
                         ? BIGDECIMAL_VALUE_TOO_LARGE
                         : BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        even = iteration * 2U;
        HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
            term, term, square, digits));
        HYPERBOLIC_TRY(hyperbolic_set_u64(divisor, even));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            term, term, divisor, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(hyperbolic_set_u64(divisor, even + 1U));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            term, term, divisor, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            next, sum, term, digits, false));
        HYPERBOLIC_TRY(bigdecimal_compare(&comparison, next, sum));
        HYPERBOLIC_TRY(bigdecimal_copy(sum, next));

        if (comparison == 0)
        {
            HYPERBOLIC_TRY(bigdecimal_copy(result, sum));
            goto cleanup;
        }
    }

    status = BIGDECIMAL_VALUE_TOO_LARGE;

cleanup:
    bigdecimal_destroy(square);
    bigdecimal_destroy(term);
    bigdecimal_destroy(sum);
    bigdecimal_destroy(next);
    bigdecimal_destroy(divisor);
    return status;
}

static BigDecimalStatus hyperbolic_asinh_series(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits
)
{
    BigDecimal *square = bigdecimal_create();
    BigDecimal *term = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimal *next = bigdecimal_create();
    BigDecimal *factor = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    uint64_t limit = (uint64_t)digits + UINT64_C(64);
    int comparison = 0;

    if (square == NULL || term == NULL || sum == NULL || next == NULL || factor == NULL)
    {
        goto cleanup;
    }

    HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
        square, value, value, digits));
    HYPERBOLIC_TRY(bigdecimal_copy(term, value));
    HYPERBOLIC_TRY(bigdecimal_copy(sum, value));

    for (uint64_t iteration = 0U; iteration < limit; iteration++)
    {
        uint64_t odd;
        uint64_t even;

        if (!numforge_budget_check() || iteration > (UINT64_MAX - 3U) / 2U)
        {
            status = iteration > (UINT64_MAX - 3U) / 2U
                         ? BIGDECIMAL_VALUE_TOO_LARGE
                         : BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        odd = iteration * 2U + 1U;
        even = (iteration + 1U) * 2U;
        HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
            term, term, square, digits));
        HYPERBOLIC_TRY(hyperbolic_set_u64(factor, odd));
        HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
            term, term, factor, digits));
        HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
            term, term, factor, digits));
        HYPERBOLIC_TRY(hyperbolic_set_u64(factor, even));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            term, term, factor, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(hyperbolic_set_u64(factor, odd + 2U));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            term, term, factor, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(bigdecimal_negate(term, term));
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            next, sum, term, digits, false));
        HYPERBOLIC_TRY(bigdecimal_compare(&comparison, next, sum));
        HYPERBOLIC_TRY(bigdecimal_copy(sum, next));

        if (comparison == 0)
        {
            HYPERBOLIC_TRY(bigdecimal_copy(result, sum));
            goto cleanup;
        }
    }

    status = BIGDECIMAL_VALUE_TOO_LARGE;

cleanup:
    bigdecimal_destroy(square);
    bigdecimal_destroy(term);
    bigdecimal_destroy(sum);
    bigdecimal_destroy(next);
    bigdecimal_destroy(factor);
    return status;
}

static BigDecimalStatus hyperbolic_atanh_series(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits
)
{
    BigDecimal *square = bigdecimal_create();
    BigDecimal *power = bigdecimal_create();
    BigDecimal *term = bigdecimal_create();
    BigDecimal *sum = bigdecimal_create();
    BigDecimal *next = bigdecimal_create();
    BigDecimal *divisor = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    uint64_t limit = (uint64_t)digits + UINT64_C(64);
    int comparison = 0;

    if (square == NULL || power == NULL || term == NULL || sum == NULL ||
        next == NULL || divisor == NULL)
    {
        goto cleanup;
    }

    HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
        square, value, value, digits));
    HYPERBOLIC_TRY(bigdecimal_copy(power, value));
    HYPERBOLIC_TRY(bigdecimal_copy(sum, value));

    for (uint64_t iteration = 1U; iteration <= limit; iteration++)
    {
        uint64_t denominator;

        if (!numforge_budget_check() || iteration > (UINT64_MAX - 1U) / 2U)
        {
            status = iteration > (UINT64_MAX - 1U) / 2U
                         ? BIGDECIMAL_VALUE_TOO_LARGE
                         : BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        denominator = iteration * 2U + 1U;
        HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
            power, power, square, digits));
        HYPERBOLIC_TRY(hyperbolic_set_u64(divisor, denominator));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            term, power, divisor, digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            next, sum, term, digits, false));
        HYPERBOLIC_TRY(bigdecimal_compare(&comparison, next, sum));
        HYPERBOLIC_TRY(bigdecimal_copy(sum, next));

        if (comparison == 0)
        {
            HYPERBOLIC_TRY(bigdecimal_copy(result, sum));
            goto cleanup;
        }
    }

    status = BIGDECIMAL_VALUE_TOO_LARGE;

cleanup:
    bigdecimal_destroy(square);
    bigdecimal_destroy(power);
    bigdecimal_destroy(term);
    bigdecimal_destroy(sum);
    bigdecimal_destroy(next);
    bigdecimal_destroy(divisor);
    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Forward hyperbolic functions.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigDecimalStatus hyperbolic_sinh_cosh(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding,
    bool cosine
)
{
    BigDecimal *absolute = NULL;
    BigDecimal *half = NULL;
    BigDecimal *exponential = NULL;
    BigDecimal *reciprocal = NULL;
    BigDecimal *one = NULL;
    BigDecimal *two = NULL;
    BigDecimal *combined = NULL;
    BigDecimalStatus status;
    int64_t work_digits;
    int comparison = 0;
    int sign = 0;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = hyperbolic_work_digits(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_set_string(result, cosine ? "1" : "0");
    }

    absolute = bigdecimal_create();
    half = bigdecimal_create();
    exponential = bigdecimal_create();
    reciprocal = bigdecimal_create();
    one = bigdecimal_create();
    two = bigdecimal_create();
    combined = bigdecimal_create();

    if (absolute == NULL || half == NULL || exponential == NULL || reciprocal == NULL ||
        one == NULL || two == NULL || combined == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_abs(absolute, value));
    HYPERBOLIC_TRY(bigdecimal_set_string(half, "0.5"));
    HYPERBOLIC_TRY(bigdecimal_compare(&comparison, absolute, half));

    if (!cosine && comparison <= 0)
    {
        HYPERBOLIC_TRY(hyperbolic_sinh_series(combined, value, work_digits));
        HYPERBOLIC_TRY(hyperbolic_finish(result, combined, digits, rounding));
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_exp(
        exponential, absolute, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    HYPERBOLIC_TRY(bigdecimal_set_string(one, "1"));
    HYPERBOLIC_TRY(bigdecimal_set_string(two, "2"));
    HYPERBOLIC_TRY(bigdecimal_div_significant(
        reciprocal, one, exponential, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    HYPERBOLIC_TRY(hyperbolic_binary_rounded(
        combined, exponential, reciprocal, work_digits, !cosine));
    HYPERBOLIC_TRY(bigdecimal_div_significant(
        combined, combined, two, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    HYPERBOLIC_TRY(bigdecimal_sign(&sign, value));

    if (!cosine && sign < 0)
    {
        HYPERBOLIC_TRY(bigdecimal_negate(combined, combined));
    }

    HYPERBOLIC_TRY(hyperbolic_finish(result, combined, digits, rounding));

cleanup:
    bigdecimal_destroy(absolute);
    bigdecimal_destroy(half);
    bigdecimal_destroy(exponential);
    bigdecimal_destroy(reciprocal);
    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(combined);
    return status;
}

BigDecimalStatus bigdecimal_sinh(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return hyperbolic_sinh_cosh(result, value, digits, rounding, false);
}

BigDecimalStatus bigdecimal_cosh(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return hyperbolic_sinh_cosh(result, value, digits, rounding, true);
}

BigDecimalStatus bigdecimal_tanh(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *absolute = NULL;
    BigDecimal *half = NULL;
    BigDecimal *threshold = NULL;
    BigDecimal *sinh_value = NULL;
    BigDecimal *square = NULL;
    BigDecimal *cosh_value = NULL;
    BigDecimal *one = NULL;
    BigDecimal *two = NULL;
    BigDecimal *exponent = NULL;
    BigDecimal *exponential = NULL;
    BigDecimal *numerator = NULL;
    BigDecimal *denominator = NULL;
    BigDecimal *temporary = NULL;
    BigDecimalStatus status;
    int64_t work_digits;
    int comparison = 0;
    int sign = 0;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = hyperbolic_work_digits(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_set_string(result, "0");
    }

    absolute = bigdecimal_create();
    half = bigdecimal_create();
    threshold = bigdecimal_create();
    sinh_value = bigdecimal_create();
    square = bigdecimal_create();
    cosh_value = bigdecimal_create();
    one = bigdecimal_create();
    two = bigdecimal_create();
    exponent = bigdecimal_create();
    exponential = bigdecimal_create();
    numerator = bigdecimal_create();
    denominator = bigdecimal_create();
    temporary = bigdecimal_create();

    if (absolute == NULL || half == NULL || threshold == NULL || sinh_value == NULL ||
        square == NULL || cosh_value == NULL || one == NULL || two == NULL ||
        exponent == NULL || exponential == NULL || numerator == NULL ||
        denominator == NULL || temporary == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_abs(absolute, value));
    HYPERBOLIC_TRY(bigdecimal_sign(&sign, value));
    HYPERBOLIC_TRY(bigdecimal_set_string(half, "0.5"));
    HYPERBOLIC_TRY(bigdecimal_set_string(one, "1"));
    HYPERBOLIC_TRY(bigdecimal_set_string(two, "2"));
    HYPERBOLIC_TRY(bigdecimal_compare(&comparison, absolute, half));

    if (comparison <= 0)
    {
        HYPERBOLIC_TRY(hyperbolic_sinh_series(
            sinh_value, absolute, work_digits));
        HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
            square, sinh_value, sinh_value, work_digits));
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            square, one, square, work_digits, false));
        HYPERBOLIC_TRY(bigdecimal_sqrt(
            cosh_value, square, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            temporary, sinh_value, cosh_value, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    }
    else
    {
        if (work_digits <= INT64_MAX / 2)
        {
            HYPERBOLIC_TRY(hyperbolic_set_u64(
                threshold, (uint64_t)work_digits * 2U));
            HYPERBOLIC_TRY(bigdecimal_compare(&comparison, absolute, threshold));
        }

        if (work_digits <= INT64_MAX / 2 && comparison > 0)
        {
            HYPERBOLIC_TRY(bigdecimal_copy(temporary, one));
        }
        else
        {
            HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
                exponent, absolute, two, work_digits));
            HYPERBOLIC_TRY(bigdecimal_negate(exponent, exponent));
            HYPERBOLIC_TRY(bigdecimal_exp(
                exponential, exponent, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
            HYPERBOLIC_TRY(hyperbolic_binary_rounded(
                numerator, one, exponential, work_digits, true));
            HYPERBOLIC_TRY(hyperbolic_binary_rounded(
                denominator, one, exponential, work_digits, false));
            HYPERBOLIC_TRY(bigdecimal_div_significant(
                temporary, numerator, denominator, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        }
    }

    if (sign < 0)
    {
        HYPERBOLIC_TRY(bigdecimal_negate(temporary, temporary));
    }

    HYPERBOLIC_TRY(hyperbolic_finish(result, temporary, digits, rounding));

cleanup:
    bigdecimal_destroy(absolute);
    bigdecimal_destroy(half);
    bigdecimal_destroy(threshold);
    bigdecimal_destroy(sinh_value);
    bigdecimal_destroy(square);
    bigdecimal_destroy(cosh_value);
    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(exponent);
    bigdecimal_destroy(exponential);
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(temporary);
    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Inverse hyperbolic functions.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_asinh(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *absolute = NULL;
    BigDecimal *half = NULL;
    BigDecimal *one = NULL;
    BigDecimal *two = NULL;
    BigDecimal *threshold = NULL;
    BigDecimal *reciprocal = NULL;
    BigDecimal *square = NULL;
    BigDecimal *root = NULL;
    BigDecimal *factor = NULL;
    BigDecimal *logarithm = NULL;
    BigDecimal *temporary = NULL;
    BigDecimalStatus status;
    int64_t work_digits;
    int comparison = 0;
    int sign = 0;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = hyperbolic_work_digits(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_set_string(result, "0");
    }

    absolute = bigdecimal_create();
    half = bigdecimal_create();
    one = bigdecimal_create();
    two = bigdecimal_create();
    threshold = bigdecimal_create();
    reciprocal = bigdecimal_create();
    square = bigdecimal_create();
    root = bigdecimal_create();
    factor = bigdecimal_create();
    logarithm = bigdecimal_create();
    temporary = bigdecimal_create();

    if (absolute == NULL || half == NULL || one == NULL || two == NULL || threshold == NULL ||
        reciprocal == NULL || square == NULL || root == NULL || factor == NULL ||
        logarithm == NULL || temporary == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_abs(absolute, value));
    HYPERBOLIC_TRY(bigdecimal_sign(&sign, value));
    HYPERBOLIC_TRY(bigdecimal_set_string(half, "0.5"));
    HYPERBOLIC_TRY(bigdecimal_set_string(one, "1"));
    HYPERBOLIC_TRY(bigdecimal_set_string(two, "2"));
    HYPERBOLIC_TRY(bigdecimal_compare(&comparison, absolute, half));

    if (comparison <= 0)
    {
        HYPERBOLIC_TRY(hyperbolic_asinh_series(
            temporary, value, work_digits));
        HYPERBOLIC_TRY(hyperbolic_finish(result, temporary, digits, rounding));
        goto cleanup;
    }

    if (work_digits <= INT64_MAX / 2)
    {
        HYPERBOLIC_TRY(hyperbolic_set_u64(
            threshold, (uint64_t)work_digits * 2U));
        HYPERBOLIC_TRY(bigdecimal_compare(&comparison, absolute, threshold));
    }

    HYPERBOLIC_TRY(bigdecimal_ln(
        logarithm, absolute, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));

    if (work_digits <= INT64_MAX / 2 && comparison > 0)
    {
        HYPERBOLIC_TRY(bigdecimal_ln(
            factor, two, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    }
    else
    {
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            reciprocal, one, absolute, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
            square, reciprocal, reciprocal, work_digits));
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            square, one, square, work_digits, false));
        HYPERBOLIC_TRY(bigdecimal_sqrt(
            root, square, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            factor, one, root, work_digits, false));
        HYPERBOLIC_TRY(bigdecimal_ln(
            factor, factor, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    }

    HYPERBOLIC_TRY(hyperbolic_binary_rounded(
        temporary, logarithm, factor, work_digits, false));

    if (sign < 0)
    {
        HYPERBOLIC_TRY(bigdecimal_negate(temporary, temporary));
    }

    HYPERBOLIC_TRY(hyperbolic_finish(result, temporary, digits, rounding));

cleanup:
    bigdecimal_destroy(absolute);
    bigdecimal_destroy(half);
    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(threshold);
    bigdecimal_destroy(reciprocal);
    bigdecimal_destroy(square);
    bigdecimal_destroy(root);
    bigdecimal_destroy(factor);
    bigdecimal_destroy(logarithm);
    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_acosh(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *one = NULL;
    BigDecimal *two = NULL;
    BigDecimal *reduced = NULL;
    BigDecimal *root = NULL;
    BigDecimal *temporary = NULL;
    BigDecimalStatus status;
    int64_t work_digits;
    int comparison = 0;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = hyperbolic_work_digits(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    one = bigdecimal_create();
    two = bigdecimal_create();
    reduced = bigdecimal_create();
    root = bigdecimal_create();
    temporary = bigdecimal_create();

    if (one == NULL || two == NULL || reduced == NULL || root == NULL || temporary == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_set_string(one, "1"));
    HYPERBOLIC_TRY(bigdecimal_compare(&comparison, value, one));

    if (comparison < 0)
    {
        status = BIGDECIMAL_INVALID_ARGUMENT;
        goto cleanup;
    }

    if (comparison == 0)
    {
        HYPERBOLIC_TRY(bigdecimal_set_string(temporary, "0"));
        HYPERBOLIC_TRY(bigdecimal_copy(result, temporary));
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_set_string(two, "2"));
    HYPERBOLIC_TRY(hyperbolic_binary_rounded(
        reduced, value, one, work_digits, true));
    HYPERBOLIC_TRY(bigdecimal_div_significant(
        reduced, reduced, two, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    HYPERBOLIC_TRY(bigdecimal_sqrt(
        root, reduced, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    HYPERBOLIC_TRY(bigdecimal_asinh(
        temporary, root, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    HYPERBOLIC_TRY(hyperbolic_multiply_rounded(
        temporary, temporary, two, work_digits));
    HYPERBOLIC_TRY(hyperbolic_finish(result, temporary, digits, rounding));

cleanup:
    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(reduced);
    bigdecimal_destroy(root);
    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_atanh(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *absolute = NULL;
    BigDecimal *half = NULL;
    BigDecimal *one = NULL;
    BigDecimal *two = NULL;
    BigDecimal *numerator = NULL;
    BigDecimal *denominator = NULL;
    BigDecimal *ratio = NULL;
    BigDecimal *temporary = NULL;
    BigDecimalStatus status;
    int64_t work_digits;
    int comparison = 0;
    int sign = 0;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = hyperbolic_work_digits(digits, rounding, &work_digits);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_set_string(result, "0");
    }

    absolute = bigdecimal_create();
    half = bigdecimal_create();
    one = bigdecimal_create();
    two = bigdecimal_create();
    numerator = bigdecimal_create();
    denominator = bigdecimal_create();
    ratio = bigdecimal_create();
    temporary = bigdecimal_create();

    if (absolute == NULL || half == NULL || one == NULL || two == NULL ||
        numerator == NULL || denominator == NULL || ratio == NULL || temporary == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_abs(absolute, value));
    HYPERBOLIC_TRY(bigdecimal_sign(&sign, value));
    HYPERBOLIC_TRY(bigdecimal_set_string(half, "0.5"));
    HYPERBOLIC_TRY(bigdecimal_set_string(one, "1"));
    HYPERBOLIC_TRY(bigdecimal_set_string(two, "2"));
    HYPERBOLIC_TRY(bigdecimal_compare(&comparison, absolute, one));

    if (comparison >= 0)
    {
        status = BIGDECIMAL_INVALID_ARGUMENT;
        goto cleanup;
    }

    HYPERBOLIC_TRY(bigdecimal_compare(&comparison, absolute, half));

    if (comparison <= 0)
    {
        HYPERBOLIC_TRY(hyperbolic_atanh_series(
            temporary, value, work_digits));
    }
    else
    {
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            numerator, one, absolute, work_digits, false));
        HYPERBOLIC_TRY(hyperbolic_binary_rounded(
            denominator, one, absolute, work_digits, true));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            ratio, numerator, denominator, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(bigdecimal_ln(
            temporary, ratio, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        HYPERBOLIC_TRY(bigdecimal_div_significant(
            temporary, temporary, two, work_digits, BIGDECIMAL_ROUND_HALF_EVEN));

        if (sign < 0)
        {
            HYPERBOLIC_TRY(bigdecimal_negate(temporary, temporary));
        }
    }

    HYPERBOLIC_TRY(hyperbolic_finish(result, temporary, digits, rounding));

cleanup:
    bigdecimal_destroy(absolute);
    bigdecimal_destroy(half);
    bigdecimal_destroy(one);
    bigdecimal_destroy(two);
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    bigdecimal_destroy(ratio);
    bigdecimal_destroy(temporary);
    return status;
}

#undef HYPERBOLIC_TRY
