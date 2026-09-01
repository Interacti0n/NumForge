#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <numforge/bigdecimal.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Storage note: BigDecimal stores value = coefficient * 10^(-scale).

    Canonical form: non-zero coefficients are not divisible by ten, and zero
    always has scale 0. Every mutating public operation computes into a
    temporary object and commits only after success, preserving the destination
    on every failure path.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for BigDecimal operations.
------------------------------------------------------------------------------------------------------------------------------
*/

typedef struct BigDecimalExponent
{
    bool negative;
    bool high;
    uint64_t magnitude;
} BigDecimalExponent;

static BigDecimalStatus bigdecimal_from_bigint_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK: return BIGDECIMAL_OK;
        case BIGINT_NULL_ARGUMENT: return BIGDECIMAL_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY: return BIGDECIMAL_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO: return BIGDECIMAL_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE: return BIGDECIMAL_VALUE_TOO_LARGE;
        default: return BIGDECIMAL_INVALID_ARGUMENT;
    }
}

static uint64_t bigdecimal_abs_i64(int64_t value)
{
    return value < 0 ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
}

static bool bigdecimal_size_add(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b)
    {
        return false;
    }

    *result = a + b;
    return true;
}

static bool bigdecimal_i64_add(int64_t a, int64_t b, int64_t *result)
{
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b))
    {
        return false;
    }

    *result = a + b;
    return true;
}

static bool bigdecimal_i64_sub(int64_t a, int64_t b, int64_t *result)
{
    if ((b > 0 && a < INT64_MIN + b) ||
        (b < 0 && a > INT64_MAX + b))
    {
        return false;
    }

    *result = a - b;
    return true;
}

static BigDecimalStatus bigdecimal_scale_difference(
    int64_t larger,
    int64_t smaller,
    uint64_t *difference
)
{
    if (larger < smaller)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (smaller >= 0)
    {
        *difference = (uint64_t)(larger - smaller);
    }
    else if (larger < 0)
    {
        *difference = bigdecimal_abs_i64(smaller) - bigdecimal_abs_i64(larger);
    }
    else
    {
        *difference = (uint64_t)larger + bigdecimal_abs_i64(smaller);
    }

    return BIGDECIMAL_OK;
}

static bool bigdecimal_valid_rounding(BigDecimalRoundingMode rounding)
{
    return rounding >= BIGDECIMAL_ROUND_TOWARD_ZERO &&
           rounding <= BIGDECIMAL_ROUND_HALF_EVEN;
}

static void bigdecimal_commit(BigDecimal *destination, BigDecimal *temporary)
{
    BigInt *old_coefficient = destination->coefficient;

    destination->coefficient = temporary->coefficient;
    destination->scale = temporary->scale;
    temporary->coefficient = old_coefficient;

    bigdecimal_destroy(temporary);
}

// Build 10^exponent through BigInt's decimal parser. This is intentionally
// kept behind one helper so a cached/power-by-squaring implementation can
// replace it later without changing arithmetic code.
static BigDecimalStatus bigdecimal_set_power_of_ten(BigInt *value, uint64_t exponent)
{
    size_t exponent_size;
    size_t length;
    char *text;
    BigDecimalStatus status;

    if (exponent > SIZE_MAX - 2U)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    exponent_size = (size_t)exponent;
    length = exponent_size + 2U;
    text = numforge_malloc(length);
    if (text == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    text[0] = '1';
    memset(text + 1, '0', exponent_size);
    text[exponent_size + 1U] = '\0';

    status = bigdecimal_from_bigint_status(bigint_set_string(value, text));
    free(text);
    return status;
}

static BigDecimalStatus bigdecimal_multiply_power_of_ten(
    BigInt *result,
    const BigInt *value,
    uint64_t exponent
)
{
    BigInt *power;
    BigDecimalStatus status;

    if (exponent == 0)
    {
        return bigdecimal_from_bigint_status(bigint_copy(result, value));
    }

    power = bigint_create();
    if (power == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_set_power_of_ten(power, exponent);
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(bigint_mul(result, value, power));
    }

    bigint_destroy(power);
    return status;
}

static BigDecimalStatus bigdecimal_normalize(BigDecimal *value)
{
    BigInt *ten;
    BigInt *quotient;
    BigInt *remainder;
    BigDecimalStatus status = BIGDECIMAL_OK;

    if (bigint_is_zero(value->coefficient))
    {
        value->scale = 0;
        return BIGDECIMAL_OK;
    }

    ten = bigint_create();
    quotient = bigint_create();
    remainder = bigint_create();
    if (ten == NULL || quotient == NULL || remainder == NULL)
    {
        bigint_destroy(ten);
        bigint_destroy(quotient);
        bigint_destroy(remainder);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_from_bigint_status(bigint_set_string(ten, "10"));
    while (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(
            bigint_div_mod(quotient, remainder, value->coefficient, ten));
        if (status != BIGDECIMAL_OK || !bigint_is_zero(remainder))
        {
            break;
        }

        if (value->scale == INT64_MIN)
        {
            status = BIGDECIMAL_SCALE_OVERFLOW;
            break;
        }

        {
            BigInt *old_coefficient = value->coefficient;
            value->coefficient = quotient;
            quotient = old_coefficient;
            value->scale--;
        }
    }

    bigint_destroy(ten);
    bigint_destroy(quotient);
    bigint_destroy(remainder);
    return status;
}

// Normalize a temporary result before atomically replacing destination.
static BigDecimalStatus bigdecimal_finish(
    BigDecimal *destination,
    BigDecimal *temporary
)
{
    BigDecimalStatus status = bigdecimal_normalize(temporary);

    if (status == BIGDECIMAL_OK)
    {
        bigdecimal_commit(destination, temporary);
    }
    else
    {
        bigdecimal_destroy(temporary);
    }

    return status;
}

// Parse only the exponent suffix. The caller has already consumed e/E.
static BigDecimalStatus bigdecimal_parse_exponent(const char *text, int64_t *result)
{
    bool negative = false;
    uint64_t magnitude = 0;
    uint64_t limit;

    if (*text == '+' || *text == '-')
    {
        negative = *text == '-';
        text++;
    }

    if (*text == '\0')
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    limit = negative ? (uint64_t)INT64_MAX + 1U : (uint64_t)INT64_MAX;
    while (*text != '\0')
    {
        uint64_t digit;

        if (*text < '0' || *text > '9')
        {
            return BIGDECIMAL_INVALID_ARGUMENT;
        }

        digit = (uint64_t)(*text - '0');
        if (magnitude > (limit - digit) / 10U)
        {
            return BIGDECIMAL_SCALE_OVERFLOW;
        }

        magnitude = magnitude * 10U + digit;
        text++;
    }

    if (!negative)
    {
        *result = (int64_t)magnitude;
    }
    else if (magnitude == (uint64_t)INT64_MAX + 1U)
    {
        *result = INT64_MIN;
    }
    else
    {
        *result = -(int64_t)magnitude;
    }

    return BIGDECIMAL_OK;
}

// Compare the decimal position of each first significant digit without
// constructing an aligned coefficient, even for very different scales.
static BigDecimalExponent bigdecimal_decimal_exponent(size_t digits, int64_t scale)
{
    BigDecimalExponent result;

    if (scale >= 0 && (uint64_t)digits < (uint64_t)scale)
    {
        result.negative = true;
        result.high = false;
        result.magnitude = (uint64_t)scale - (uint64_t)digits;
        return result;
    }

    result.negative = false;
    if (scale >= 0)
    {
        result.high = false;
        result.magnitude = (uint64_t)digits - (uint64_t)scale;
    }
    else
    {
        uint64_t scale_magnitude = bigdecimal_abs_i64(scale);

        result.high = (uint64_t)digits > UINT64_MAX - scale_magnitude;
        result.magnitude = (uint64_t)digits + scale_magnitude;
    }

    return result;
}

static int bigdecimal_compare_exponents(BigDecimalExponent a, BigDecimalExponent b)
{
    if (a.negative != b.negative)
    {
        return a.negative ? -1 : 1;
    }

    if (a.high != b.high)
    {
        return a.high ? 1 : -1;
    }

    if (a.magnitude == b.magnitude)
    {
        return 0;
    }

    if (a.negative)
    {
        return a.magnitude > b.magnitude ? -1 : 1;
    }

    return a.magnitude < b.magnitude ? -1 : 1;
}

// Apply one rounding decision to a truncation-toward-zero quotient.
static BigDecimalStatus bigdecimal_round_quotient(
    BigInt *quotient,
    const BigInt *remainder,
    const BigInt *divisor,
    bool negative,
    BigDecimalRoundingMode rounding
)
{
    bool adjust = false;
    BigDecimalStatus status = BIGDECIMAL_OK;

    if (bigint_is_zero(remainder) || rounding == BIGDECIMAL_ROUND_TOWARD_ZERO)
    {
        return BIGDECIMAL_OK;
    }

    switch (rounding)
    {
        case BIGDECIMAL_ROUND_AWAY_FROM_ZERO:
            adjust = true;
            break;
        case BIGDECIMAL_ROUND_FLOOR:
            adjust = negative;
            break;
        case BIGDECIMAL_ROUND_CEILING:
            adjust = !negative;
            break;
        case BIGDECIMAL_ROUND_HALF_UP:
        case BIGDECIMAL_ROUND_HALF_EVEN:
        {
            BigInt *absolute_remainder = bigint_create();
            BigInt *absolute_divisor = bigint_create();
            BigInt *two = bigint_create();
            BigInt *twice_remainder = bigint_create();
            int comparison;

            if (absolute_remainder == NULL || absolute_divisor == NULL ||
                two == NULL || twice_remainder == NULL)
            {
                bigint_destroy(absolute_remainder);
                bigint_destroy(absolute_divisor);
                bigint_destroy(two);
                bigint_destroy(twice_remainder);
                return BIGDECIMAL_OUT_OF_MEMORY;
            }

            status = bigdecimal_from_bigint_status(bigint_abs(absolute_remainder, remainder));
            if (status == BIGDECIMAL_OK)
            {
                status = bigdecimal_from_bigint_status(bigint_abs(absolute_divisor, divisor));
            }
            if (status == BIGDECIMAL_OK)
            {
                status = bigdecimal_from_bigint_status(bigint_set_string(two, "2"));
            }
            if (status == BIGDECIMAL_OK)
            {
                status = bigdecimal_from_bigint_status(bigint_mul(twice_remainder, absolute_remainder, two));
            }

            if (status == BIGDECIMAL_OK)
            {
                comparison = bigint_compare(twice_remainder, absolute_divisor);
                adjust = comparison > 0 ||
                         (comparison == 0 &&
                          (rounding == BIGDECIMAL_ROUND_HALF_UP ||
                           !bigint_is_even(quotient)));
            }

            bigint_destroy(absolute_remainder);
            bigint_destroy(absolute_divisor);
            bigint_destroy(two);
            bigint_destroy(twice_remainder);
            break;
        }
        default:
            return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (status != BIGDECIMAL_OK || !adjust)
    {
        return status;
    }

    {
        BigInt *one = bigint_create();

        if (one == NULL)
        {
            return BIGDECIMAL_OUT_OF_MEMORY;
        }

        status = bigdecimal_from_bigint_status(bigint_set_string(one, "1"));
        if (status == BIGDECIMAL_OK)
        {
            status = bigdecimal_from_bigint_status(
                negative ? bigint_sub(quotient, quotient, one)
                         : bigint_add(quotient, quotient, one));
        }

        bigint_destroy(one);
    }

    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Lifecycle and conversion functions for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

const char *bigdecimal_status_to_string(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK: return "success";
        case BIGDECIMAL_NULL_ARGUMENT: return "null argument";
        case BIGDECIMAL_OUT_OF_MEMORY: return "out of memory";
        case BIGDECIMAL_INVALID_ARGUMENT: return "invalid argument";
        case BIGDECIMAL_DIVISION_BY_ZERO: return "division by zero";
        case BIGDECIMAL_VALUE_TOO_LARGE: return "value too large";
        case BIGDECIMAL_SCALE_OVERFLOW: return "scale overflow";
        default: return "unknown status";
    }
}

BigDecimal *bigdecimal_create(void)
{
    BigDecimal *value = numforge_malloc(sizeof(*value));

    if (value == NULL)
    {
        return NULL;
    }

    value->coefficient = bigint_create();
    if (value->coefficient == NULL)
    {
        free(value);
        return NULL;
    }

    value->scale = 0;
    return value;
}

void bigdecimal_destroy(BigDecimal *value)
{
    if (value != NULL)
    {
        bigint_destroy(value->coefficient);
        free(value);
    }
}

BigDecimalStatus bigdecimal_copy(BigDecimal *destination, const BigDecimal *source)
{
    BigDecimal *temporary;
    BigDecimalStatus status;

    if (destination == NULL || source == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }
    if (destination == source)
    {
        return BIGDECIMAL_OK;
    }

    temporary = bigdecimal_create();
    if (temporary == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_from_bigint_status(bigint_copy(temporary->coefficient, source->coefficient));
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = source->scale;
        bigdecimal_commit(destination, temporary);
    }
    else
    {
        bigdecimal_destroy(temporary);
    }

    return status;
}

BigDecimalStatus bigdecimal_set_string(BigDecimal *value, const char *string)
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

    if (fractional_digits > INT64_MAX)
    {
        free(digits);
        return BIGDECIMAL_SCALE_OVERFLOW;
    }
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

BigDecimalStatus bigdecimal_to_string(const BigDecimal *value, char **result)
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
            !bigdecimal_size_add(total_length, (size_t)zeros, &total_length))
        {
            free(coefficient);
            return BIGDECIMAL_VALUE_TOO_LARGE;
        }

        formatted = numforge_malloc(total_length + 1U);
        if (formatted != NULL)
        {
            size_t offset = 0;
            if (negative) formatted[offset++] = '-';
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
                !bigdecimal_size_add(total_length, digits_length, &total_length))
            {
                free(coefficient);
                return BIGDECIMAL_VALUE_TOO_LARGE;
            }

            formatted = numforge_malloc(total_length + 1U);
            if (formatted != NULL)
            {
                size_t offset = 0;
                if (negative) formatted[offset++] = '-';
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
                if (negative) formatted[offset++] = '-';
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

/*
------------------------------------------------------------------------------------------------------------------------------
    Comparison and inspection functions for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_compare(int *comparison, const BigDecimal *a, const BigDecimal *b)
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
        if (bigint_is_zero(a->coefficient) && bigint_is_zero(b->coefficient)) *comparison = 0;
        else *comparison = bigint_is_zero(a->coefficient) ? -1 : 1;
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

BigDecimalStatus bigdecimal_is_zero(bool *result, const BigDecimal *value)
{
    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    *result = bigint_is_zero(value->coefficient);
    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_is_negative(bool *result, const BigDecimal *value)
{
    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    *result = bigint_is_negative(value->coefficient);
    return BIGDECIMAL_OK;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Exact arithmetic operation functions for BigDecimal.
------------------------------------------------------------------------------------------------------------------------------
*/

BigDecimalStatus bigdecimal_abs(BigDecimal *result, const BigDecimal *value)
{
    BigDecimal *temporary;
    BigDecimalStatus status;

    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    temporary = bigdecimal_create();
    if (temporary == NULL) return BIGDECIMAL_OUT_OF_MEMORY;
    status = bigdecimal_from_bigint_status(bigint_abs(temporary->coefficient, value->coefficient));
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = value->scale;
        bigdecimal_commit(result, temporary);
    }
    else bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_negate(BigDecimal *result, const BigDecimal *value)
{
    BigDecimal *temporary;
    BigDecimalStatus status;

    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    temporary = bigdecimal_create();
    if (temporary == NULL) return BIGDECIMAL_OUT_OF_MEMORY;
    status = bigdecimal_from_bigint_status(bigint_negate(temporary->coefficient, value->coefficient));
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = value->scale;
        bigdecimal_commit(result, temporary);
    }
    else bigdecimal_destroy(temporary);
    return status;
}

static BigDecimalStatus bigdecimal_add_or_subtract(
    BigDecimal *result,
    const BigDecimal *a,
    const BigDecimal *b,
    bool subtract
)
{
    BigDecimal *temporary;
    BigInt *scaled_a = NULL;
    BigInt *scaled_b = NULL;
    int64_t scale = a->scale >= b->scale ? a->scale : b->scale;
    uint64_t difference;
    BigDecimalStatus status;

    if (bigint_is_zero(a->coefficient))
    {
        return subtract ? bigdecimal_negate(result, b) : bigdecimal_copy(result, b);
    }
    if (bigint_is_zero(b->coefficient))
    {
        return bigdecimal_copy(result, a);
    }

    temporary = bigdecimal_create();
    scaled_a = bigint_create();
    scaled_b = bigint_create();
    if (temporary == NULL || scaled_a == NULL || scaled_b == NULL)
    {
        bigdecimal_destroy(temporary);
        bigint_destroy(scaled_a);
        bigint_destroy(scaled_b);
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    status = bigdecimal_scale_difference(scale, a->scale, &difference);
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_multiply_power_of_ten(scaled_a, a->coefficient, difference);
    }
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_scale_difference(scale, b->scale, &difference);
    }
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_multiply_power_of_ten(scaled_b, b->coefficient, difference);
    }
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(
            subtract ? bigint_sub(temporary->coefficient, scaled_a, scaled_b)
                     : bigint_add(temporary->coefficient, scaled_a, scaled_b));
    }

    bigint_destroy(scaled_a);
    bigint_destroy(scaled_b);
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = scale;
        return bigdecimal_finish(result, temporary);
    }

    bigdecimal_destroy(temporary);
    return status;
}

BigDecimalStatus bigdecimal_add(BigDecimal *result, const BigDecimal *a, const BigDecimal *b)
{
    if (result == NULL || a == NULL || b == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    return bigdecimal_add_or_subtract(result, a, b, false);
}

BigDecimalStatus bigdecimal_sub(BigDecimal *result, const BigDecimal *a, const BigDecimal *b)
{
    if (result == NULL || a == NULL || b == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    return bigdecimal_add_or_subtract(result, a, b, true);
}

BigDecimalStatus bigdecimal_mul(BigDecimal *result, const BigDecimal *a, const BigDecimal *b)
{
    BigDecimal *temporary;
    BigDecimalStatus status;
    int64_t scale;

    if (result == NULL || a == NULL || b == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (!bigdecimal_i64_add(a->scale, b->scale, &scale)) return BIGDECIMAL_SCALE_OVERFLOW;
    temporary = bigdecimal_create();
    if (temporary == NULL) return BIGDECIMAL_OUT_OF_MEMORY;
    status = bigdecimal_from_bigint_status(bigint_mul(temporary->coefficient, a->coefficient, b->coefficient));
    if (status == BIGDECIMAL_OK)
    {
        temporary->scale = scale;
        return bigdecimal_finish(result, temporary);
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

    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (!bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    if (bigint_is_zero(value->coefficient)) return bigdecimal_copy(result, value);
    if (target_scale >= value->scale) return bigdecimal_copy(result, value);

    status = bigdecimal_scale_difference(value->scale, target_scale, &difference);
    if (status != BIGDECIMAL_OK) return status;
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
        status = bigdecimal_from_bigint_status(bigint_div_mod(temporary->coefficient, remainder, value->coefficient, divisor));
    }
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_quotient(temporary->coefficient, remainder, divisor,
                                           bigint_is_negative(value->coefficient), rounding);
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

    if (result == NULL || a == NULL || b == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (!bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    if (bigint_is_zero(b->coefficient)) return BIGDECIMAL_DIVISION_BY_ZERO;
    if (bigint_is_zero(a->coefficient)) return bigdecimal_copy(result, a);
    if (!bigdecimal_i64_add(target_scale, b->scale, &exponent) ||
        !bigdecimal_i64_sub(exponent, a->scale, &exponent)) return BIGDECIMAL_SCALE_OVERFLOW;

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
        status = bigdecimal_multiply_power_of_ten(numerator, a->coefficient, (uint64_t)exponent);
        if (status == BIGDECIMAL_OK) status = bigdecimal_from_bigint_status(bigint_copy(divisor, b->coefficient));
    }
    else
    {
        power = bigdecimal_abs_i64(exponent);
        status = bigdecimal_from_bigint_status(bigint_copy(numerator, a->coefficient));
        if (status == BIGDECIMAL_OK) status = bigdecimal_multiply_power_of_ten(divisor, b->coefficient, power);
    }

    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_from_bigint_status(bigint_div_mod(temporary->coefficient, remainder, numerator, divisor));
    }
    negative = bigint_is_negative(a->coefficient) != bigint_is_negative(b->coefficient);
    if (status == BIGDECIMAL_OK)
    {
        status = bigdecimal_round_quotient(temporary->coefficient, remainder, divisor, negative, rounding);
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
