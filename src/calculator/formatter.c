#include "formatter.h"

#include "../bigdecimal/bigdecimal_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALCULATOR_SCIENTIFIC_EXPONENT_THRESHOLD 10

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for result formatting.
------------------------------------------------------------------------------------------------------------------------------
*/
static CalculatorStatus calculator_from_bigdecimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK: return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT: return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}

static char *calculator_compose_scientific(
    char *significand,
    size_t digit_count,
    bool negative,
    int64_t exponent,
    int64_t output_scale
)
{
    char *formatted;
    size_t offset = 0U;
    int exponent_length;

    if (output_scale != CALCULATOR_UNLIMITED_OUTPUT_SCALE &&
        output_scale < (int64_t)digit_count - 1)
    {
        size_t wanted = (size_t)output_scale + 1U;
        size_t round_index = wanted;

        if (significand[round_index] >= '5')
        {
            size_t carry = wanted;

            while (carry > 0U)
            {
                carry--;
                if (significand[carry] < '9')
                {
                    significand[carry]++;
                    break;
                }
                significand[carry] = '0';
            }

            if (carry == 0U && significand[0] == '0')
            {
                significand[0] = '1';
                memset(significand + 1U, '0', wanted - 1U);
                exponent++;
            }
        }
        digit_count = wanted;
    }
    while (digit_count > 1U && significand[digit_count - 1U] == '0') digit_count--;
    significand[digit_count] = '\0';

    exponent_length = snprintf(NULL, 0, "E%+lld", (long long)exponent);
    if (exponent_length < 0)
    {
        free(significand);
        return NULL;
    }

    formatted = malloc((negative ? 1U : 0U) + digit_count +
                       (digit_count > 1U ? 1U : 0U) + (size_t)exponent_length + 1U);
    if (formatted == NULL)
    {
        free(significand);
        return NULL;
    }

    if (negative) formatted[offset++] = '-';
    formatted[offset++] = significand[0];
    if (digit_count > 1U)
    {
        formatted[offset++] = '.';
        memcpy(formatted + offset, significand + 1U, digit_count - 1U);
        offset += digit_count - 1U;
    }
    (void)snprintf(formatted + offset, (size_t)exponent_length + 1U, "E%+lld", (long long)exponent);
    free(significand);
    return formatted;
}

static char *calculator_format_scientific(const char *text, int64_t output_scale)
{
    const char *digits = text;
    const char *point;
    const char *first;
    char *significand;
    size_t digit_count = 0U;
    size_t index;
    bool negative = false;
    int64_t exponent;

    if (text[0] == '-')
    {
        negative = true;
        digits++;
    }

    point = strchr(digits, '.');
    if (point == NULL)
    {
        point = digits + strlen(digits);
    }

    if (point != digits && !(point == digits + 1U && digits[0] == '0'))
    {
        first = digits;
        if ((size_t)(point - digits) > INT64_MAX)
        {
            return NULL;
        }
        exponent = (int64_t)(point - digits) - 1;
    }
    else
    {
        first = point + 1U;
        while (*first == '0') first++;
        if (*first == '\0')
        {
            return NULL;
        }
        if ((size_t)(first - point) > INT64_MAX)
        {
            return NULL;
        }
        exponent = -(int64_t)(first - point);
    }

    if (exponent > -CALCULATOR_SCIENTIFIC_EXPONENT_THRESHOLD &&
        exponent < CALCULATOR_SCIENTIFIC_EXPONENT_THRESHOLD)
    {
        return NULL;
    }

    significand = malloc(strlen(digits) + 1U);
    if (significand == NULL)
    {
        return NULL;
    }

    for (index = (size_t)(first - digits); digits[index] != '\0'; index++)
    {
        if (digits[index] != '.')
        {
            significand[digit_count++] = digits[index];
        }
    }
    return calculator_compose_scientific(significand, digit_count, negative, exponent, output_scale);
}

static char *calculator_format_negative_scale_value(
    const BigDecimal *value,
    int64_t output_scale
)
{
    char *coefficient = bigint_to_string(value->coefficient);
    const char *digits;
    bool negative;
    size_t digit_count;
    int64_t exponent;
    char *significand;

    if (coefficient == NULL)
    {
        return NULL;
    }

    negative = coefficient[0] == '-';
    digits = coefficient + (negative ? 1U : 0U);
    digit_count = strlen(digits);
    if (value->scale == INT64_MIN || digit_count - 1U > (size_t)(INT64_MAX + value->scale))
    {
        free(coefficient);
        return NULL;
    }
    exponent = (int64_t)(digit_count - 1U) - value->scale;
    if (exponent < CALCULATOR_SCIENTIFIC_EXPONENT_THRESHOLD)
    {
        free(coefficient);
        return NULL;
    }

    significand = malloc(digit_count + 1U);
    if (significand != NULL)
    {
        memcpy(significand, digits, digit_count + 1U);
    }
    free(coefficient);
    if (significand == NULL)
    {
        return NULL;
    }

    return calculator_compose_scientific(significand, digit_count, negative, exponent, output_scale);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Result formatting functions.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_format_result(
    const BigDecimal *value,
    const CalculatorContext *context,
    char **result
)
{
    BigDecimal *formatted_value;
    BigDecimalStatus decimal_status;
    CalculatorStatus status;
    char *ordinary = NULL;
    char *scientific;

    if (value == NULL || context == NULL || result == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (context->output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    *result = NULL;
    if (value->scale < 0)
    {
        scientific = calculator_format_negative_scale_value(value, context->output_scale);
        if (scientific != NULL)
        {
            *result = scientific;
            return CALCULATOR_OK;
        }
    }
    formatted_value = bigdecimal_create();
    if (formatted_value == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    if (context->output_scale == CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        decimal_status = bigdecimal_copy(formatted_value, value);
    }
    else
    {
        decimal_status = bigdecimal_rescale(formatted_value, value, context->output_scale, context->rounding);
    }
    status = calculator_from_bigdecimal_status(decimal_status);
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_to_string(formatted_value, &ordinary));
    }
    bigdecimal_destroy(formatted_value);
    if (status != CALCULATOR_OK)
    {
        free(ordinary);
        return status;
    }

    scientific = calculator_format_scientific(ordinary, context->output_scale);
    if (scientific != NULL)
    {
        free(ordinary);
        *result = scientific;
    }
    else
    {
        *result = ordinary;
    }

    return CALCULATOR_OK;
}
