#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Division, significant-digit rounding, and rescaling for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_round_significant(
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

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

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

#if SIZE_MAX > INT64_MAX
    if (discarded > (size_t)INT64_MAX)
    {
        return BIGDECIMAL_SCALE_OVERFLOW;
    }
#endif

    if (!bigdecimal_i64_sub(value->scale, (int64_t)discarded, &target_scale))
    {
        return BIGDECIMAL_SCALE_OVERFLOW;
    }

    return bigdecimal_rescale(result, value, target_scale, rounding);
}

// A reduced denominator has a finite decimal expansion exactly when its only
// prime factors are two and five. Scales do not affect this property. Detect
// termination before selecting a precision; never silently fall back to
// rounding after an allocation failure or cancellation.
BigDecimalStatus bigdecimal_div_exact_or_significant(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    BigInt *denominator;
    BigInt *factor;
    BigInt *quotient;
    BigInt *remainder;
    BigDecimal *temporary = NULL;
    BigDecimalStatus status;
    int64_t counts[2] = { 0, 0 };
    bool finite = false;

    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (bigint_is_zero(b->coefficient))
    {
        return BIGDECIMAL_DIVISION_BY_ZERO;
    }

    if (bigint_is_zero(a->coefficient))
    {
        return bigdecimal_copy(result, a);
    }

    denominator = bigint_create();
    factor = bigint_create();
    quotient = bigint_create();
    remainder = bigint_create();
    if (denominator == NULL || factor == NULL || quotient == NULL || remainder == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }
    status = bigdecimal_from_bigint_status(
        bigint_gcd(factor, a->coefficient, b->coefficient));

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(bigint_div(denominator, b->coefficient, factor));
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(bigint_abs(denominator, denominator));
    }

    for (size_t index = 0; index < 2U && status == BIGDECIMAL_OK; index++)
    {
        status = bigdecimal_from_bigint_status(bigint_set_string(factor, index == 0 ? "2" : "5"));
        while (status == BIGDECIMAL_OK && !bigint_is_one(denominator))
        {
            if (!numforge_budget_check())
            {
                status = BIGDECIMAL_OUT_OF_MEMORY;
                break;
            }
            status = bigdecimal_from_bigint_status(
                bigint_div_mod(quotient, remainder, denominator, factor));

            if (status != BIGDECIMAL_OK || !bigint_is_zero(remainder))
            {
                break;
            }

            if (counts[index] == INT64_MAX)
            {
                status = BIGDECIMAL_SCALE_OVERFLOW;
                break;
            }

            counts[index]++;
            status = bigdecimal_from_bigint_status(bigint_copy(denominator, quotient));
        }
    }

    if (status != BIGDECIMAL_OK)
    {
        goto cleanup;
    }

    finite = bigint_is_one(denominator);

    if (finite)
    {
        BigDecimal numerator = { a->coefficient, 0 };
        BigDecimal divisor = { b->coefficient, 0 };
        int64_t scale;
        int64_t target = counts[0] > counts[1] ? counts[0] : counts[1];

        temporary = bigdecimal_create();

        if (temporary == NULL)
        {
            status = BIGDECIMAL_OUT_OF_MEMORY;
            goto cleanup;
        }

        status = bigdecimal_div(temporary, &numerator, &divisor, target, rounding);

        // Normalize coefficient division before combining extreme scales.
        if (status == BIGDECIMAL_OK)
        {
            if (!((bigdecimal_i64_add(temporary->scale, a->scale, &scale) &&
                   bigdecimal_i64_sub(scale, b->scale, &scale)) ||
                  (bigdecimal_i64_sub(temporary->scale, b->scale, &scale) &&
                   bigdecimal_i64_add(scale, a->scale, &scale)) ||
                  (bigdecimal_i64_sub(a->scale, b->scale, &scale) &&
                   bigdecimal_i64_add(scale, temporary->scale, &scale))))
            {
                status = BIGDECIMAL_SCALE_OVERFLOW;
            }
            else
            {
                temporary->scale = scale;
            }
        }
    }

cleanup:
    bigint_destroy(denominator);
    bigint_destroy(factor);
    bigint_destroy(quotient);
    bigint_destroy(remainder);

    if (status == BIGDECIMAL_OK && finite)
    {
        bigdecimal_commit(result, temporary);
        return BIGDECIMAL_OK;
    }

    bigdecimal_destroy(temporary);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    return bigdecimal_div_significant(result, a, b, digits, rounding);
}

BigDecimalStatus bigdecimal_div_significant(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    char *a_text;
    char *b_text;
    const char *a_digits;
    const char *b_digits;
    size_t a_length;
    size_t b_length;
    int comparison = 0;
    int64_t exponent;
    int64_t target;
    int64_t scale;
    BigDecimal numerator;
    BigDecimal divisor;
    BigDecimal *temporary;
    BigDecimalStatus status;

    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (bigint_is_zero(b->coefficient))
    {
        return BIGDECIMAL_DIVISION_BY_ZERO;
    }

    if (bigint_is_zero(a->coefficient))
    {
        return bigdecimal_copy(result, a);
    }

    a_text = bigint_to_string(a->coefficient);
    b_text = bigint_to_string(b->coefficient);
    if (a_text == NULL || b_text == NULL)
    {
        free(a_text);
        free(b_text);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }
    a_digits = a_text + (a_text[0] == '-' ? 1U : 0U);
    b_digits = b_text + (b_text[0] == '-' ? 1U : 0U);
    a_length = strlen(a_digits);
    b_length = strlen(b_digits);
#if SIZE_MAX > INT64_MAX
    if (a_length > INT64_MAX || b_length > INT64_MAX)
    {
        free(a_text);
        free(b_text);
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }
#endif
    for (size_t i = 0U; i < a_length || i < b_length; i++)
    {
        char left = i < a_length ? a_digits[i] : '0';
        char right = i < b_length ? b_digits[i] : '0';
        if (left != right)
        {
            comparison = left < right ? -1 : 1;
            break;
        }
    }

    exponent = (int64_t)a_length - (int64_t)b_length;

    if (comparison < 0)
    {
        exponent--;
    }

    free(a_text);
    free(b_text);

    if (!bigdecimal_i64_sub(digits - 1, exponent, &target))
    {
        return BIGDECIMAL_SCALE_OVERFLOW;
    }

    // Divide coefficients, then apply the external scales. This avoids huge
    // powers of ten when the ratio has a tiny or huge but compact magnitude.
    numerator.coefficient = a->coefficient;
    numerator.scale = 0;
    divisor.coefficient = b->coefficient;
    divisor.scale = 0;
    temporary = bigdecimal_create();

    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_div(temporary, &numerator, &divisor, target, rounding);

    if (status == BIGDECIMAL_OK && !bigint_is_zero(temporary->coefficient))
    {
        if (!((bigdecimal_i64_add(temporary->scale, a->scale, &scale) &&
               bigdecimal_i64_sub(scale, b->scale, &scale)) ||
              (bigdecimal_i64_sub(temporary->scale, b->scale, &scale) &&
               bigdecimal_i64_add(scale, a->scale, &scale)) ||
              (bigdecimal_i64_sub(a->scale, b->scale, &scale) &&
               bigdecimal_i64_add(scale, temporary->scale, &scale))))
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

    bigdecimal_destroy(temporary);
    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Rounded arithmetic operation functions for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_rescale(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *temporary;
    BigInt *divisor;
    BigInt *remainder;
    uint64_t difference;
    BigDecimalStatus status;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (!bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (bigint_is_zero(value->coefficient))
    {
        return bigdecimal_copy(result, value);
    }

    if (target_scale >= value->scale)
    {
        return bigdecimal_copy(result, value);
    }

    status = bigdecimal_scale_difference(value->scale, target_scale, &difference);

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    temporary = bigdecimal_create();
    divisor = bigint_create();
    remainder = bigint_create();
    if (temporary == NULL || divisor == NULL || remainder == NULL)
    {
        bigdecimal_destroy(temporary);
        bigint_destroy(divisor);
        bigint_destroy(remainder);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_set_power_of_ten(divisor, difference);
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(
            bigint_div_mod(
                temporary->coefficient,
                remainder,
                value->coefficient,
                divisor));
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_quotient(
            temporary->coefficient,
            remainder,
            divisor,
            bigint_is_negative(value->coefficient),
            rounding);
    }

    bigint_destroy(divisor);
    bigint_destroy(remainder);
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = target_scale;
        return bigdecimal_finish(result, temporary);
    }

    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_floor(
    BigDecimal *result,
    const BigDecimal *value
)
{
    return bigdecimal_rescale(result, value, 0, BIGDECIMAL_ROUND_FLOOR);
}

BigDecimalStatus bigdecimal_ceil(
    BigDecimal *result,
    const BigDecimal *value
)
{
    return bigdecimal_rescale(result, value, 0, BIGDECIMAL_ROUND_CEILING);
}

BigDecimalStatus bigdecimal_trunc(
    BigDecimal *result,
    const BigDecimal *value
)
{
    return bigdecimal_rescale(result, value, 0, BIGDECIMAL_ROUND_TOWARD_ZERO);
}

BigDecimalStatus bigdecimal_round(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t places
)
{
    return bigdecimal_rescale(result, value, places, BIGDECIMAL_ROUND_HALF_EVEN);
}

BigDecimalStatus bigdecimal_div(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    int64_t target_scale,
    BigDecimalRoundingMode rounding
)
{
    BigDecimal *temporary;
    BigInt *numerator;
    BigInt *divisor;
    BigInt *remainder;
    int64_t exponent;
    uint64_t power;
    bool negative;
    BigDecimalStatus status;

    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (!bigdecimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (bigint_is_zero(b->coefficient))
    {
        return BIGDECIMAL_DIVISION_BY_ZERO;
    }

    if (bigint_is_zero(a->coefficient))
    {
        return bigdecimal_copy(result, a);
    }

    // Equal large scales can cancel even when target_scale + b->scale
    // overflows. Try the equivalent ordering before rejecting the result.
    if (!(bigdecimal_i64_add(target_scale, b->scale, &exponent) &&
          bigdecimal_i64_sub(exponent, a->scale, &exponent)) &&
        !(bigdecimal_i64_sub(target_scale, a->scale, &exponent) &&
          bigdecimal_i64_add(exponent, b->scale, &exponent)))
    {
        return BIGDECIMAL_SCALE_OVERFLOW;
    }

    temporary = bigdecimal_create();
    numerator = bigint_create();
    divisor = bigint_create();
    remainder = bigint_create();
    if (temporary == NULL || numerator == NULL || divisor == NULL || remainder == NULL)
    {
        bigdecimal_destroy(temporary);
        bigint_destroy(numerator);
        bigint_destroy(divisor);
        bigint_destroy(remainder);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    if (exponent >= 0)
    {
        status = bigdecimal_multiply_power_of_ten(
            numerator,
            a->coefficient,
            (uint64_t)exponent);

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_from_bigint_status(
                bigint_copy(divisor, b->coefficient));
        }
    }
    else
    {
        power = bigdecimal_abs_i64(exponent);
        status = bigdecimal_from_bigint_status(bigint_copy(numerator, a->coefficient));

        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_multiply_power_of_ten(
                divisor,
                b->coefficient,
                power);
        }
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(
            bigint_div_mod(
                temporary->coefficient,
                remainder,
                numerator,
                divisor));
    }

    negative = bigint_is_negative(a->coefficient) != bigint_is_negative(b->coefficient);

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_quotient(
            temporary->coefficient,
            remainder,
            divisor,
            negative,
            rounding);
    }

    bigint_destroy(numerator);
    bigint_destroy(divisor);
    bigint_destroy(remainder);
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = target_scale;
        return bigdecimal_finish(result, temporary);
    }

    bigdecimal_destroy(temporary);
    return status;
}
