#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Text parsing and plain-string conversion for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_set_string(
    BigDecimal *value,
    const char *string
)
{
    const char *cursor;
    char *digits;
    size_t input_length;
    size_t digit_count = 0;
    size_t fractional_digits = 0;
    bool negative = false;
    bool decimal_point_seen = false;
    int64_t exponent = 0;
    int64_t scale;
    BigDecimal *temporary;
    BigDecimalStatus status;

    if (value == NULL || string == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    input_length = strlen(string);
    if (input_length > SIZE_MAX - 2U)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    digits = numforge_malloc(input_length + 2U);
    if (digits == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    cursor = string;
    if (*cursor == '+' || *cursor == '-')
    {
        negative = *cursor == '-';
        cursor++;
    }

    while (*cursor != '\0' && *cursor != 'e' && *cursor != 'E')
    {
        if (*cursor >= '0' && *cursor <= '9')
        {
            digits[digit_count++] = *cursor;
            if (decimal_point_seen)
            {
                if (fractional_digits == SIZE_MAX)
                {
                    free(digits);
                    return BIGDECIMAL_VALUE_TOO_LARGE;
                }
                fractional_digits++;
            }
        }
        else if (*cursor == '.' && !decimal_point_seen)
        {
            decimal_point_seen = true;
        }
        else
        {
            free(digits);
            return BIGDECIMAL_INVALID_ARGUMENT;
        }

        cursor++;
    }

    if (digit_count == 0)
    {
        free(digits);
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (*cursor == 'e' || *cursor == 'E')
    {
        status = bigdecimal_parse_exponent(cursor + 1, &exponent);
        if (status != BIGDECIMAL_OK)
        {
            free(digits);
            return status;
        }
    }

#if SIZE_MAX > INT64_MAX
    if (fractional_digits > (size_t)INT64_MAX)
    {
        free(digits);
        return BIGDECIMAL_SCALE_OVERFLOW;
    }
#endif
    if (!bigdecimal_i64_sub((int64_t)fractional_digits, exponent, &scale))
    {
        free(digits);
        return BIGDECIMAL_SCALE_OVERFLOW;
    }

    temporary = bigdecimal_create();
    if (temporary == NULL)
    {
        free(digits);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    if (negative)
    {
        memmove(digits + 1, digits, digit_count);
        digits[0] = '-';
        digit_count++;
    }
    digits[digit_count] = '\0';

    status = bigdecimal_from_bigint_status(bigint_set_string(temporary->coefficient, digits));
    free(digits);
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = scale;
        status = bigdecimal_finish(value, temporary);
    }
    else
    {
        bigdecimal_destroy(temporary);
    }

    return status;
}

BigDecimalStatus bigdecimal_to_string(
    const BigDecimal *value,
    char **result
)
{
    char *coefficient;
    char *formatted;
    const char *digits;
    size_t digits_length;
    size_t prefix_length;
    size_t total_length;
    size_t scale;
    bool negative;

    if (value == NULL || result == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    coefficient = bigint_to_string(value->coefficient);
    if (coefficient == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    negative = coefficient[0] == '-';
    digits = coefficient + (negative ? 1 : 0);
    digits_length = strlen(digits);
    prefix_length = negative ? 1U : 0U;

    if (value->scale < 0)
    {
        uint64_t zeros = bigdecimal_abs_i64(value->scale);

        if (zeros > SIZE_MAX || !bigdecimal_size_add(prefix_length, digits_length, &total_length) ||
            !bigdecimal_size_add(total_length, (size_t)zeros, &total_length) ||
            total_length == SIZE_MAX)
        {
            free(coefficient);
            return BIGDECIMAL_VALUE_TOO_LARGE;
        }

        formatted = numforge_malloc(total_length + 1U);
        if (formatted != NULL)
        {
            size_t offset = 0;

            if (negative)
            {
                formatted[offset++] = '-';
            }

            memcpy(formatted + offset, digits, digits_length);
            memset(formatted + offset + digits_length, '0', (size_t)zeros);
            formatted[total_length] = '\0';
        }
    }
    else
    {
        if ((uint64_t)value->scale > SIZE_MAX)
        {
            free(coefficient);
            return BIGDECIMAL_VALUE_TOO_LARGE;
        }

        scale = (size_t)value->scale;
        if (scale == 0)
        {
            total_length = prefix_length + digits_length;
            formatted = numforge_malloc(total_length + 1U);
            if (formatted != NULL)
            {
                memcpy(formatted, coefficient, total_length + 1U);
            }
        }
        else if (scale >= digits_length)
        {
            size_t zero_count = scale - digits_length;

            if (!bigdecimal_size_add(prefix_length, 2U, &total_length) ||
                !bigdecimal_size_add(total_length, zero_count, &total_length) ||
                !bigdecimal_size_add(total_length, digits_length, &total_length) ||
                total_length == SIZE_MAX)
            {
                free(coefficient);
                return BIGDECIMAL_VALUE_TOO_LARGE;
            }

            formatted = numforge_malloc(total_length + 1U);
            if (formatted != NULL)
            {
                size_t offset = 0;

                if (negative)
                {
                    formatted[offset++] = '-';
                }

                formatted[offset++] = '0';
                formatted[offset++] = '.';
                memset(formatted + offset, '0', zero_count);
                offset += zero_count;
                memcpy(formatted + offset, digits, digits_length);
                formatted[total_length] = '\0';
            }
        }
        else
        {
            total_length = prefix_length + digits_length + 1U;
            formatted = numforge_malloc(total_length + 1U);
            if (formatted != NULL)
            {
                size_t before_point = digits_length - scale;
                size_t offset = 0;

                if (negative)
                {
                    formatted[offset++] = '-';
                }

                memcpy(formatted + offset, digits, before_point);
                offset += before_point;
                formatted[offset++] = '.';
                memcpy(formatted + offset, digits + before_point, scale);
                formatted[total_length] = '\0';
            }
        }
    }

    free(coefficient);
    if (formatted == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    *result = formatted;
    return BIGDECIMAL_OK;
}
