#include "formatter.h"

#include "../bigdecimal/bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

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

static bool calculator_valid_rounding(BigDecimalRoundingMode rounding)
{
    return rounding >= BIGDECIMAL_ROUND_TOWARD_ZERO &&
           rounding <= BIGDECIMAL_ROUND_HALF_EVEN;
}

static bool calculator_should_round_scientific(
    const char *significand,
    size_t digit_count,
    size_t kept_digits,
    bool negative,
    BigDecimalRoundingMode rounding
)
{
    bool discarded_non_zero = false;
    size_t index;

    for (index = kept_digits; index < digit_count; index++)
    {
        if (significand[index] != '0')
        {
            discarded_non_zero = true;
            break;
        }
    }
    if (!discarded_non_zero)
    {
        return false;
    }

    switch (rounding)
    {
        case BIGDECIMAL_ROUND_TOWARD_ZERO:
            return false;
        case BIGDECIMAL_ROUND_AWAY_FROM_ZERO:
            return true;
        case BIGDECIMAL_ROUND_FLOOR:
            return negative;
        case BIGDECIMAL_ROUND_CEILING:
            return !negative;
        case BIGDECIMAL_ROUND_HALF_UP:
        case BIGDECIMAL_ROUND_HALF_EVEN:
        {
            char first_discarded = significand[kept_digits];

            if (first_discarded != '5')
            {
                return first_discarded > '5';
            }
            for (index = kept_digits + 1U; index < digit_count; index++)
            {
                if (significand[index] != '0')
                {
                    return true;
                }
            }
            return rounding == BIGDECIMAL_ROUND_HALF_UP ||
                   ((significand[kept_digits - 1U] - '0') % 2 != 0);
        }
        default:
            return false;
    }
}

typedef struct CalculatorScientificExponent
{
    bool negative;
    uint64_t magnitude;
} CalculatorScientificExponent;

static bool calculator_scientific_exponent(
    size_t digit_count,
    int64_t scale,
    CalculatorScientificExponent *exponent
)
{
    uint64_t decimal_position = (uint64_t)(digit_count - 1U);

    if (scale >= 0)
    {
        uint64_t scale_magnitude = (uint64_t)scale;

        exponent->negative = scale_magnitude > decimal_position;
        exponent->magnitude = exponent->negative
            ? scale_magnitude - decimal_position
            : decimal_position - scale_magnitude;
        return true;
    }

    {
        uint64_t scale_magnitude = scale == INT64_MIN
            ? (uint64_t)INT64_MAX + 1U
            : (uint64_t)(-scale);

        if (decimal_position > UINT64_MAX - scale_magnitude)
        {
            return false;
        }
        exponent->negative = false;
        exponent->magnitude = decimal_position + scale_magnitude;
        return true;
    }
}

static bool calculator_increment_scientific_exponent(CalculatorScientificExponent *exponent)
{
    if (exponent->negative)
    {
        if (exponent->magnitude > 0U)
        {
            exponent->magnitude--;
        }
        if (exponent->magnitude == 0U)
        {
            exponent->negative = false;
        }
        return true;
    }

    if (exponent->magnitude == UINT64_MAX)
    {
        return false;
    }
    exponent->magnitude++;
    return true;
}

static CalculatorStatus calculator_compose_scientific(
    char *significand,
    size_t digit_count,
    bool negative,
    CalculatorScientificExponent exponent,
    int64_t output_scale,
    BigDecimalRoundingMode rounding,
    char **result
)
{
    char *formatted;
    size_t formatted_length;
    size_t offset = 0U;
    int exponent_length;

    if (output_scale != CALCULATOR_UNLIMITED_OUTPUT_SCALE &&
        (uint64_t)output_scale < (uint64_t)(digit_count - 1U))
    {
        size_t wanted = (size_t)output_scale + 1U;

        if (calculator_should_round_scientific(
                significand, digit_count, wanted, negative, rounding))
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
                if (!calculator_increment_scientific_exponent(&exponent))
                {
                    free(significand);
                    return CALCULATOR_VALUE_TOO_LARGE;
                }
            }
        }
        digit_count = wanted;
    }
    while (digit_count > 1U && significand[digit_count - 1U] == '0') digit_count--;
    significand[digit_count] = '\0';

    exponent_length = snprintf(NULL, 0, "E%c%llu",
                               exponent.negative ? '-' : '+',
                               (unsigned long long)exponent.magnitude);
    if (exponent_length < 0)
    {
        free(significand);
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    formatted_length = negative ? 1U : 0U;
    if (digit_count > SIZE_MAX - formatted_length)
    {
        free(significand);
        return CALCULATOR_VALUE_TOO_LARGE;
    }
    formatted_length += digit_count;
    if (digit_count > 1U)
    {
        if (formatted_length == SIZE_MAX)
        {
            free(significand);
            return CALCULATOR_VALUE_TOO_LARGE;
        }
        formatted_length++;
    }
    if (formatted_length == SIZE_MAX ||
        (size_t)exponent_length > SIZE_MAX - formatted_length - 1U)
    {
        free(significand);
        return CALCULATOR_VALUE_TOO_LARGE;
    }
    formatted_length += (size_t)exponent_length;

    formatted = numforge_malloc(formatted_length + 1U);
    if (formatted == NULL)
    {
        free(significand);
        return CALCULATOR_OUT_OF_MEMORY;
    }

    if (negative) formatted[offset++] = '-';
    formatted[offset++] = significand[0];
    if (digit_count > 1U)
    {
        formatted[offset++] = '.';
        memcpy(formatted + offset, significand + 1U, digit_count - 1U);
        offset += digit_count - 1U;
    }
    (void)snprintf(formatted + offset, (size_t)exponent_length + 1U, "E%c%llu",
                   exponent.negative ? '-' : '+',
                   (unsigned long long)exponent.magnitude);
    free(significand);
    *result = formatted;
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_try_format_scientific(
    const BigDecimal *value,
    int64_t output_scale,
    BigDecimalRoundingMode rounding,
    char **result,
    bool *formatted
)
{
    char *coefficient;
    const char *digits;
    char *significand;
    size_t digit_count;
    bool negative;
    CalculatorScientificExponent exponent;

    *result = NULL;
    *formatted = false;
    if (bigint_is_zero(value->coefficient))
    {
        return CALCULATOR_OK;
    }

    coefficient = bigint_to_string(value->coefficient);
    if (coefficient == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    negative = coefficient[0] == '-';
    digits = coefficient + (negative ? 1U : 0U);
    digit_count = strlen(digits);
    if (!calculator_scientific_exponent(digit_count, value->scale, &exponent))
    {
        free(coefficient);
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    if (exponent.magnitude < CALCULATOR_SCIENTIFIC_EXPONENT_THRESHOLD)
    {
        free(coefficient);
        return CALCULATOR_OK;
    }

    significand = numforge_malloc(digit_count + 1U);
    if (significand == NULL)
    {
        free(coefficient);
        return CALCULATOR_OUT_OF_MEMORY;
    }
    memcpy(significand, digits, digit_count + 1U);
    free(coefficient);

    *formatted = true;
    return calculator_compose_scientific(
        significand, digit_count, negative, exponent, output_scale, rounding, result);
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
    bool used_scientific;

    if (value == NULL || context == NULL || result == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (context->output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (!calculator_valid_rounding(context->rounding))
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    *result = NULL;
    status = calculator_try_format_scientific(
        value, context->output_scale, context->rounding, result, &used_scientific);
    if (status != CALCULATOR_OK || used_scientific)
    {
        return status;
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
        status = calculator_try_format_scientific(
            formatted_value, context->output_scale, context->rounding,
            result, &used_scientific);
    }
    if (status == CALCULATOR_OK && !used_scientific)
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_to_string(formatted_value, result));
    }
    bigdecimal_destroy(formatted_value);

    return status;
}
