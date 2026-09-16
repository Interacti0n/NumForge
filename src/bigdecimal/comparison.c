#include "bigdecimal_internal.h"

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Comparison and inspection functions for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_compare(
    int *comparison,
    const BigDecimal *a,
    const BigDecimal *b
)
{
    char *a_text;
    char *b_text;
    const char *a_digits;
    const char *b_digits;
    size_t a_length;
    size_t b_length;
    bool a_negative;
    bool b_negative;
    int magnitude_comparison = 0;
    BigDecimalExponent a_exponent;
    BigDecimalExponent b_exponent;

    if (comparison == NULL || a == NULL || b == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    a_negative = bigint_is_negative(a->coefficient);
    b_negative = bigint_is_negative(b->coefficient);
    if (a_negative != b_negative)
    {
        *comparison = a_negative ? -1 : 1;
        return BIGDECIMAL_OK;
    }

    if (bigint_is_zero(a->coefficient) || bigint_is_zero(b->coefficient))
    {
        if (bigint_is_zero(a->coefficient) && bigint_is_zero(b->coefficient))
        {
            *comparison = 0;
        }
        else
        {
            *comparison = bigint_is_zero(a->coefficient) ? -1 : 1;
        }

        return BIGDECIMAL_OK;
    }

    a_text = bigint_to_string(a->coefficient);
    b_text = bigint_to_string(b->coefficient);
    if (a_text == NULL || b_text == NULL)
    {
        free(a_text);
        free(b_text);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    a_digits = a_text + (a_negative ? 1 : 0);
    b_digits = b_text + (b_negative ? 1 : 0);
    a_length = strlen(a_digits);
    b_length = strlen(b_digits);
    a_exponent = bigdecimal_decimal_exponent(a_length, a->scale);
    b_exponent = bigdecimal_decimal_exponent(b_length, b->scale);
    magnitude_comparison = bigdecimal_compare_exponents(a_exponent, b_exponent);

    if (magnitude_comparison == 0)
    {
        size_t longest = a_length > b_length ? a_length : b_length;

        for (size_t index = 0; index < longest; index++)
        {
            char a_digit = index < a_length ? a_digits[index] : '0';
            char b_digit = index < b_length ? b_digits[index] : '0';

            if (a_digit != b_digit)
            {
                magnitude_comparison = a_digit < b_digit ? -1 : 1;
                break;
            }
        }
    }

    free(a_text);
    free(b_text);
    *comparison = a_negative ? -magnitude_comparison : magnitude_comparison;
    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_is_zero(
    bool *result,
    const BigDecimal *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    *result = bigint_is_zero(value->coefficient);
    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_is_negative(
    bool *result,
    const BigDecimal *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    *result = bigint_is_negative(value->coefficient);
    return BIGDECIMAL_OK;
}
