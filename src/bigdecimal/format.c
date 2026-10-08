#include "../internal/benchmark_profile.h"
#include <numforge/bigdecimal.h>

#include "bigdecimal_internal.h"
#include "../bigint/bigint_internal.h"
#include "../internal/numforge_alloc.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BIGDECIMAL_SCIENTIFIC_EXPONENT_THRESHOLD 10

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for result formatting.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigDecimalStatus decimal_from_bigdecimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return BIGDECIMAL_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return BIGDECIMAL_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return BIGDECIMAL_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return BIGDECIMAL_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return BIGDECIMAL_SCALE_OVERFLOW;
        default:
            return BIGDECIMAL_INVALID_ARGUMENT;
    }
}

static bool decimal_valid_rounding(BigDecimalRoundingMode rounding)
{
    return rounding >= BIGDECIMAL_ROUND_TOWARD_ZERO &&
           rounding <= BIGDECIMAL_ROUND_HALF_EVEN;
}

static bool decimal_should_round_scientific(
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

typedef struct DecimalScientificExponent
{
    bool negative;
    uint64_t magnitude;
} DecimalScientificExponent;

/* A 32-bit size_t always fits int64_t. On wider targets, check before the
 * conversion without provoking an always-false warning on 32-bit GCC. */
static bool decimal_size_fits_int64(size_t value)
{
#if SIZE_MAX > INT64_MAX
    return value <= (size_t)INT64_MAX;
#else
    (void)value;
    return true;
#endif
}

/* Return true only when conservative bit-length bounds prove that the decimal
 * exponent lies strictly inside the fixed-notation range. The loose rational
 * bounds surround log10(2); an inconclusive result keeps the exact old path. */
static bool decimal_definitely_fixed(const BigDecimal *value)
{
    size_t bits = bigint_bit_length(value->coefficient);
    size_t minimum_digits;
    size_t maximum_digits;
    int64_t minimum_exponent;
    int64_t maximum_exponent;

    if (bits == 0U || !decimal_size_fits_int64(bits) ||
        bits - 1U > (SIZE_MAX - 99999U) / 30102U ||
        bits > (SIZE_MAX - 99999U) / 30103U)
    {
        return bits == 0U;
    }

    minimum_digits = ((bits - 1U) * 30102U) / 100000U + 1U;
    maximum_digits = (bits * 30103U + 99999U) / 100000U;

    if (!decimal_size_fits_int64(minimum_digits) ||
        !decimal_size_fits_int64(maximum_digits) ||
        !bigdecimal_i64_sub((int64_t)minimum_digits - 1, value->scale, &minimum_exponent) ||
        !bigdecimal_i64_sub((int64_t)maximum_digits - 1, value->scale, &maximum_exponent))
    {
        return false;
    }

    return minimum_exponent > -BIGDECIMAL_SCIENTIFIC_EXPONENT_THRESHOLD &&
           maximum_exponent < BIGDECIMAL_SCIENTIFIC_EXPONENT_THRESHOLD;
}

static bool decimal_scientific_exponent(
    size_t digit_count,
    int64_t scale,
    DecimalScientificExponent *exponent
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

static bool decimal_increment_scientific_exponent(DecimalScientificExponent *exponent)
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

static BigDecimalStatus decimal_compose_scientific_profile_impl(
    char *significand,
    size_t digit_count,
    bool negative,
    DecimalScientificExponent exponent,
    int64_t output_scale,
    BigDecimalRoundingMode rounding,
    char **result
)
{
    char *formatted;
    size_t formatted_length;
    size_t offset = 0U;
    int exponent_length;
    NumForgeProfilePhase round_previous = numforge_profile_enter(NUMFORGE_PHASE_ROUND);

    if (output_scale != (-1) &&
        (uint64_t)output_scale < (uint64_t)(digit_count - 1U))
    {
        size_t wanted = (size_t)output_scale + 1U;

        if (decimal_should_round_scientific(
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
                if (!decimal_increment_scientific_exponent(&exponent))
                {
                    numforge_profile_leave(round_previous);
                    free(significand);
                    return BIGDECIMAL_VALUE_TOO_LARGE;
                }
            }
        }
        digit_count = wanted;
    }

    while (digit_count > 1U && significand[digit_count - 1U] == '0')
    {
        digit_count--;
    }

    significand[digit_count] = '\0';
    numforge_profile_leave(round_previous);

    exponent_length = snprintf(
        NULL,
        0,
        "E%c%llu",
        exponent.negative ? '-' : '+',
        (unsigned long long)exponent.magnitude);
    if (exponent_length < 0)
    {
        free(significand);
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    formatted_length = negative ? 1U : 0U;
    if (digit_count > SIZE_MAX - formatted_length)
    {
        free(significand);
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }
    formatted_length += digit_count;
    if (digit_count > 1U)
    {
        if (formatted_length == SIZE_MAX)
        {
            free(significand);
            return BIGDECIMAL_VALUE_TOO_LARGE;
        }
        formatted_length++;
    }

    if (formatted_length == SIZE_MAX ||
        (size_t)exponent_length > SIZE_MAX - formatted_length - 1U)
    {
        free(significand);
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }
    formatted_length += (size_t)exponent_length;

    formatted = numforge_malloc(formatted_length + 1U);
    if (formatted == NULL)
    {
        free(significand);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    if (negative)
    {
        formatted[offset++] = '-';
    }

    formatted[offset++] = significand[0];
    if (digit_count > 1U)
    {
        formatted[offset++] = '.';
        memcpy(formatted + offset, significand + 1U, digit_count - 1U);
        offset += digit_count - 1U;
    }

    (void)snprintf(
        formatted + offset,
        (size_t)exponent_length + 1U,
        "E%c%llu",
        exponent.negative ? '-' : '+',
        (unsigned long long)exponent.magnitude);

    free(significand);
    *result = formatted;

    return BIGDECIMAL_OK;
}

static BigDecimalStatus decimal_compose_scientific(
    char *significand,
    size_t digit_count,
    bool negative,
    DecimalScientificExponent exponent,
    int64_t output_scale,
    BigDecimalRoundingMode rounding,
    char **result
)
{
    NumForgeProfilePhase previous = numforge_profile_enter(NUMFORGE_PHASE_COMPOSE);
    BigDecimalStatus status = decimal_compose_scientific_profile_impl(significand, digit_count, negative, exponent, output_scale, rounding, result);
    numforge_profile_leave(previous);
    return status;
}

/* Propagate outward-rounded bounds from the leading binary limbs. If both
 * endpoints round to the same requested decimal, all omitted bits are proven
 * irrelevant to that result. Boundary/tie cases fall back to exact conversion.
 * Powers and products retain only places+13 digits, not the full coefficient. */
static BigDecimalStatus decimal_prefix_product(BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits,
    BigDecimalRoundingMode rounding)
{
    BigDecimalStatus status = bigdecimal_mul(result, a, b);
    if (status == BIGDECIMAL_OK)
        status = bigdecimal_round_significant(result, result, digits, rounding);
    return status;
}

static bool decimal_prefix_exponent(const BigDecimal *prefix, int64_t scale,
    size_t length, DecimalScientificExponent *exponent)
{
    uint64_t omitted = bigdecimal_abs_i64(prefix->scale);
    if (prefix->scale > 0 || omitted > SIZE_MAX - length) return false;
    return decimal_scientific_exponent(length + (size_t)omitted, scale, exponent);
}

static BigDecimalStatus decimal_try_prefix(const BigDecimal *value,
    int64_t places, BigDecimalRoundingMode rounding, char **result, bool *formatted)
{
    BigDecimal *low = NULL, *high = NULL, *factor_low = NULL, *factor_high = NULL;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    DecimalScientificExponent low_exponent, high_exponent;
    BigInt prefix;
    char *low_text = NULL, *high_text = NULL;
    int comparison;
    int64_t work_digits;
    size_t kept_limbs;
    size_t omitted_limbs;
    uint64_t power;

    if (places < 0 || places > 128) return BIGDECIMAL_OK;
    /* Larger retained prefixes need more work. A conservative crossover
     * keeps ordinary inputs on the cheaper established conversion path. */
    if (value->coefficient->size < 64U + (size_t)(places * places) / 32U)
        return BIGDECIMAL_OK;
    work_digits = places + 13;
    kept_limbs = ((size_t)work_digits * 4U + 63U) / 64U;
    if (kept_limbs >= value->coefficient->size) return BIGDECIMAL_OK;
    omitted_limbs = value->coefficient->size - kept_limbs;
#if SIZE_MAX > UINT64_MAX / 64U
    if (omitted_limbs > UINT64_MAX / 64U) return BIGDECIMAL_OK;
#endif
    power = (uint64_t)omitted_limbs * 64U;
    prefix = *value->coefficient;
    prefix.limbs += omitted_limbs;
    prefix.size = kept_limbs;
    prefix.capacity = kept_limbs;
    prefix.is_negative = false;
    low = bigdecimal_create(); high = bigdecimal_create();
    factor_low = bigdecimal_create(); factor_high = bigdecimal_create();
    if (!low || !high || !factor_low || !factor_high) goto cleanup;
#define PREFIX_TRY(call) do { status = (call); if (status != BIGDECIMAL_OK) goto cleanup; } while (0)
    PREFIX_TRY(bigdecimal_from_bigint(low, &prefix));
    PREFIX_TRY(bigdecimal_copy(high, low));
    PREFIX_TRY(bigdecimal_set_string(factor_low, "1"));
    PREFIX_TRY(bigdecimal_add(high, high, factor_low));
    PREFIX_TRY(bigdecimal_set_string(factor_low, "2"));
    PREFIX_TRY(bigdecimal_set_string(factor_high, "2"));
    while (power != 0U)
    {
        if (power & 1U)
        {
            PREFIX_TRY(decimal_prefix_product(low, low, factor_low, work_digits, BIGDECIMAL_ROUND_FLOOR));
            PREFIX_TRY(decimal_prefix_product(high, high, factor_high, work_digits, BIGDECIMAL_ROUND_CEILING));
        }
        power >>= 1U;
        if (power != 0U)
        {
            PREFIX_TRY(decimal_prefix_product(factor_low, factor_low, factor_low, work_digits, BIGDECIMAL_ROUND_FLOOR));
            PREFIX_TRY(decimal_prefix_product(factor_high, factor_high, factor_high, work_digits, BIGDECIMAL_ROUND_CEILING));
        }
    }
    low_text = bigint_to_string(low->coefficient);
    high_text = bigint_to_string(high->coefficient);
    if (!low_text || !high_text) { status = BIGDECIMAL_OUT_OF_MEMORY; goto cleanup; }
    if (!decimal_prefix_exponent(low, value->scale, strlen(low_text), &low_exponent) ||
        !decimal_prefix_exponent(high, value->scale, strlen(high_text), &high_exponent) ||
        low_exponent.magnitude < BIGDECIMAL_SCIENTIFIC_EXPONENT_THRESHOLD ||
        low_exponent.negative != high_exponent.negative ||
        low_exponent.magnitude != high_exponent.magnitude) goto cleanup;
    free(low_text); low_text = NULL;
    free(high_text); high_text = NULL;
    if (value->coefficient->is_negative)
    {
        PREFIX_TRY(bigdecimal_negate(low, low));
        PREFIX_TRY(bigdecimal_negate(high, high));
    }
    PREFIX_TRY(bigdecimal_round_significant(low, low, places + 1, rounding));
    PREFIX_TRY(bigdecimal_round_significant(high, high, places + 1, rounding));
    PREFIX_TRY(bigdecimal_compare(&comparison, low, high));
    if (comparison != 0) goto cleanup;
    low_text = bigint_to_string(low->coefficient);
    if (!low_text) { status = BIGDECIMAL_OUT_OF_MEMORY; goto cleanup; }
    {
        bool negative = low_text[0] == '-';
        size_t length = strlen(low_text + negative);
        if (!decimal_prefix_exponent(low, value->scale, length, &low_exponent)) goto cleanup;
        if (negative) memmove(low_text, low_text + 1, length + 1U);
        *formatted = true;
        status = decimal_compose_scientific(low_text, length, negative,
            low_exponent, places, rounding, result);
        low_text = NULL;
    }
cleanup:
    free(low_text); free(high_text);
    bigdecimal_destroy(low); bigdecimal_destroy(high);
    bigdecimal_destroy(factor_low); bigdecimal_destroy(factor_high);
    /* Extreme scales retain the established full-conversion path. */
    if (status == BIGDECIMAL_SCALE_OVERFLOW) status = BIGDECIMAL_OK;
    return status;
#undef PREFIX_TRY
}

static BigDecimalStatus decimal_try_format_scientific(
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
    DecimalScientificExponent exponent;

    *result = NULL;
    *formatted = false;

    if (bigint_is_zero(value->coefficient))
    {
        return BIGDECIMAL_OK;
    }

    {
        BigDecimalStatus status = decimal_try_prefix(value, output_scale, rounding, result, formatted);
        if (status != BIGDECIMAL_OK || *formatted) return status;
    }
    coefficient = bigint_to_string(value->coefficient);
    if (coefficient == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    negative = coefficient[0] == '-';
    digits = coefficient + (negative ? 1U : 0U);
    digit_count = strlen(digits);
    if (!decimal_scientific_exponent(digit_count, value->scale, &exponent))
    {
        free(coefficient);
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    if (exponent.magnitude < BIGDECIMAL_SCIENTIFIC_EXPONENT_THRESHOLD)
    {
        free(coefficient);
        return BIGDECIMAL_OK;
    }

    significand = numforge_malloc(digit_count + 1U);
    if (significand == NULL)
    {
        free(coefficient);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    memcpy(significand, digits, digit_count + 1U);
    free(coefficient);

    *formatted = true;
    return decimal_compose_scientific(
        significand, digit_count, negative, exponent, output_scale, rounding, result);
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Result formatting functions.
------------------------------------------------------------------------------------------------------------------------------
*/

static BigDecimalStatus decimal_format_result_impl(
    const BigDecimal *value,
    int64_t places,
    BigDecimalRoundingMode rounding,
    char **result
)
{
    BigDecimal *formatted_value;
    BigDecimalStatus decimal_status;
    BigDecimalStatus status;
    bool used_scientific;

    if (value == NULL || result == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }
    if (places < (-1))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (places > INT64_MAX - 4)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    if (!decimal_valid_rounding(rounding))
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    *result = NULL;
    used_scientific = false;
    status = BIGDECIMAL_OK;
    if (!decimal_definitely_fixed(value))
    {
        status = decimal_try_format_scientific(
            value, places, rounding, result, &used_scientific);
    }
    if (status != BIGDECIMAL_OK || used_scientific)
    {
        return status;
    }

    formatted_value = bigdecimal_create();
    if (formatted_value == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    NumForgeProfilePhase round_previous = numforge_profile_enter(NUMFORGE_PHASE_ROUND);
    if (places == (-1))
    {
        decimal_status = bigdecimal_copy(formatted_value, value);
    }
    else
    {
        decimal_status = bigdecimal_rescale(formatted_value, value, places, rounding);
    }

    numforge_profile_leave(round_previous);
    status = decimal_from_bigdecimal_status(decimal_status);
    if (status == BIGDECIMAL_OK)
    {
        status = decimal_try_format_scientific(
            formatted_value, places, rounding,
            result, &used_scientific);
    }

    if (status == BIGDECIMAL_OK && !used_scientific)
    {
        status = decimal_from_bigdecimal_status(
            bigdecimal_to_string(formatted_value, result));
    }
    bigdecimal_destroy(formatted_value);

    return status;
}

BigDecimalStatus bigdecimal_format(
    const BigDecimal *value,
    int64_t places,
    BigDecimalRoundingMode rounding,
    char **result
)
{
    char *temporary = NULL;
    BigDecimalStatus status;

    if (result == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    status = decimal_format_result_impl(value, places, rounding, &temporary);

    if (status == BIGDECIMAL_OK)
    {
        *result = temporary;
    }
    else
    {
        free(temporary);
    }

    return status;
}
