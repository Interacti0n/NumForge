#include "../internal/benchmark_profile.h"
#include <numforge/bigdecimal.h>

#include "../internal/numforge_alloc.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BIGDECIMAL_AUTO_MAX_CHARS 80U

typedef struct DecimalNotation
{
    bool negative;
    bool exponent_negative;
    uint64_t exponent_magnitude;
    char *digits;
    size_t count;
} DecimalNotation;

static BigDecimalStatus decimal_notation_parse(const char *source, DecimalNotation *notation)
{
    const char *mantissa = source + (source[0] == '-');
    const char *marker = strchr(mantissa, 'E');
    size_t length = marker == NULL ? strlen(mantissa) : (size_t)(marker - mantissa);
    size_t point = length;
    size_t leading = 0U;
    size_t count = 0U;

    memset(notation, 0, sizeof(*notation));
    notation->negative = source[0] == '-';
    notation->digits = numforge_malloc(length + 1U);
    if (notation->digits == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }
    for (size_t index = 0U; index < length; index++)
    {
        if (mantissa[index] == '.')
        {
            point = index;
        }
        else
        {
            notation->digits[count++] = mantissa[index];
        }
    }
    if (point == length)
    {
        point = count;
    }
    while (leading < count && notation->digits[leading] == '0')
    {
        leading++;
    }
    if (leading == count)
    {
        notation->count = 0U;
        return BIGDECIMAL_OK;
    }
    memmove(notation->digits, notation->digits + leading, count - leading);
    notation->count = count - leading;
    while (notation->count > 1U && notation->digits[notation->count - 1U] == '0')
    {
        notation->count--;
    }
    notation->digits[notation->count] = '\0';

    if (marker != NULL)
    {
        char *end;
        const char *number = marker + 2U;
        errno = 0;
        notation->exponent_magnitude = strtoull(number, &end, 10);
        if (errno != 0 || *end != '\0' ||
            (marker[1] != '+' && marker[1] != '-'))
        {
            return BIGDECIMAL_VALUE_TOO_LARGE;
        }
        notation->exponent_negative = marker[1] == '-';
        return BIGDECIMAL_OK;
    }

    {
        int64_t exponent = (int64_t)point - (int64_t)leading - 1;
        notation->exponent_negative = exponent < 0;
        notation->exponent_magnitude = exponent < 0 ? (uint64_t)(-exponent) : (uint64_t)exponent;
    }
    return BIGDECIMAL_OK;
}

static BigDecimalStatus decimal_notation_scientific(
    const DecimalNotation *notation, bool mathematical, size_t limit, char **result)
{
    const char *separator = mathematical ? " × 10^" : "E";
    int exponent_chars = mathematical && !notation->exponent_negative
        ? snprintf(NULL, 0, "%" PRIu64, notation->exponent_magnitude)
        : snprintf(NULL, 0, "%c%" PRIu64,
            notation->exponent_negative ? '-' : '+', notation->exponent_magnitude);
    size_t length;
    char *output;
    size_t offset = 0U;

    if (exponent_chars < 0 || notation->count > SIZE_MAX - 32U)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }
    length = (notation->negative ? 1U : 0U) + notation->count +
        (notation->count > 1U ? 1U : 0U) + strlen(separator) + (size_t)exponent_chars;
    if (length > limit)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }
    output = numforge_malloc(length + 1U);
    if (output == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }
    if (notation->negative)
    {
        output[offset++] = '-';
    }
    output[offset++] = notation->digits[0];
    if (notation->count > 1U)
    {
        output[offset++] = '.';
        memcpy(output + offset, notation->digits + 1U, notation->count - 1U);
        offset += notation->count - 1U;
    }
    memcpy(output + offset, separator, strlen(separator));
    offset += strlen(separator);
    if (mathematical && !notation->exponent_negative)
    {
        (void)snprintf(output + offset, (size_t)exponent_chars + 1U,
            "%" PRIu64, notation->exponent_magnitude);
    }
    else
    {
        (void)snprintf(output + offset, (size_t)exponent_chars + 1U, "%c%" PRIu64,
            notation->exponent_negative ? '-' : '+', notation->exponent_magnitude);
    }
    *result = output;
    return BIGDECIMAL_OK;
}

static size_t decimal_notation_plain_length(const DecimalNotation *notation, size_t limit)
{
    uint64_t position;
    uint64_t body;
    if (notation->exponent_magnitude > (uint64_t)SIZE_MAX - 2U - (uint64_t)notation->count ||
        (limit != SIZE_MAX && notation->exponent_magnitude > (uint64_t)limit + 1U))
    {
        return SIZE_MAX;
    }
    if (notation->exponent_negative)
    {
        body = notation->exponent_magnitude + 1U + (uint64_t)notation->count;
    }
    else
    {
        position = notation->exponent_magnitude + 1U;
        body = position >= notation->count ? position : (uint64_t)notation->count + 1U;
    }
    if (body > (uint64_t)limit || body > SIZE_MAX - (notation->negative ? 1U : 0U))
    {
        return SIZE_MAX;
    }
    return (size_t)body + (notation->negative ? 1U : 0U);
}

static BigDecimalStatus decimal_notation_plain(
    const DecimalNotation *notation, size_t limit, char **result)
{
    size_t length = decimal_notation_plain_length(notation, limit);
    size_t position;
    size_t offset = 0U;
    char *output;
    if (length == SIZE_MAX)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }
    output = numforge_malloc(length + 1U);
    if (output == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }
    if (notation->negative)
    {
        output[offset++] = '-';
    }
    if (notation->exponent_negative)
    {
        output[offset++] = '0';
        output[offset++] = '.';
        memset(output + offset, '0', (size_t)notation->exponent_magnitude - 1U);
        offset += (size_t)notation->exponent_magnitude - 1U;
        memcpy(output + offset, notation->digits, notation->count);
        offset += notation->count;
    }
    else
    {
        position = (size_t)notation->exponent_magnitude + 1U;
        if (position >= notation->count)
        {
            memcpy(output + offset, notation->digits, notation->count);
            offset += notation->count;
            memset(output + offset, '0', position - notation->count);
            offset += position - notation->count;
        }
        else
        {
            memcpy(output + offset, notation->digits, position);
            offset += position;
            output[offset++] = '.';
            memcpy(output + offset, notation->digits + position, notation->count - position);
            offset += notation->count - position;
        }
    }
    output[offset] = '\0';
    *result = output;
    return BIGDECIMAL_OK;
}

static BigDecimalStatus bigdecimal_format_mode_profile_impl(
    const BigDecimal *value, int64_t places, BigDecimalRoundingMode rounding,
    BigDecimalFormatMode mode, size_t max_output_bytes, char **result)
{
    char *canonical = NULL;
    char *formatted = NULL;
    DecimalNotation notation;
    BigDecimalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }
    if (mode < BIGDECIMAL_FORMAT_AUTO || mode > BIGDECIMAL_FORMAT_MATHEMATICAL)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }
    status = bigdecimal_format(value, places, rounding, &canonical);
    if (status != BIGDECIMAL_OK)
    {
        return status;
    }
    status = decimal_notation_parse(canonical, &notation);
    if (status == BIGDECIMAL_OK && notation.count == 0U)
    {
        if (max_output_bytes < 1U)
        {
            status = BIGDECIMAL_VALUE_TOO_LARGE;
        }
        else
        {
            formatted = numforge_malloc(2U);
            if (formatted == NULL)
            {
                status = BIGDECIMAL_OUT_OF_MEMORY;
            }
            else
            {
                memcpy(formatted, "0", 2U);
            }
        }
    }
    else if (status == BIGDECIMAL_OK)
    {
        if (mode == BIGDECIMAL_FORMAT_AUTO)
        {
            mode = notation.exponent_magnitude >= 10U ||
                decimal_notation_plain_length(&notation, BIGDECIMAL_AUTO_MAX_CHARS) > BIGDECIMAL_AUTO_MAX_CHARS
                ? BIGDECIMAL_FORMAT_SCIENTIFIC : BIGDECIMAL_FORMAT_PLAIN;
        }
        status = mode == BIGDECIMAL_FORMAT_PLAIN
            ? decimal_notation_plain(&notation, max_output_bytes, &formatted)
            : decimal_notation_scientific(&notation,
                mode == BIGDECIMAL_FORMAT_MATHEMATICAL, max_output_bytes, &formatted);
    }
    free(notation.digits);
    free(canonical);
    if (status == BIGDECIMAL_OK)
    {
        *result = formatted;
    }
    else
    {
        free(formatted);
    }
    return status;
}

BigDecimalStatus bigdecimal_format_mode(
    const BigDecimal *value, int64_t places, BigDecimalRoundingMode rounding,
    BigDecimalFormatMode mode, size_t max_output_bytes, char **result)
{
    NumForgeProfilePhase previous = numforge_profile_enter(NUMFORGE_PHASE_COMPOSE);
    BigDecimalStatus result_status = bigdecimal_format_mode_profile_impl(value, places, rounding, mode, max_output_bytes, result);
    numforge_profile_leave(previous);
    return result_status;
}
