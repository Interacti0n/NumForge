#include "bigdecimal_internal.h"
#include "../bigint/bigint_internal.h"
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

BigDecimalStatus bigdecimal_from_bigint_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK:
            return BIGDECIMAL_OK;
        case BIGINT_NULL_ARGUMENT:
            return BIGDECIMAL_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return BIGDECIMAL_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO:
            return BIGDECIMAL_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE:
            return BIGDECIMAL_VALUE_TOO_LARGE;
        default:
            return BIGDECIMAL_INVALID_ARGUMENT;
    }
}

uint64_t bigdecimal_abs_i64(int64_t value)
{
    return value < 0 ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
}

bool bigdecimal_size_add(size_t a, size_t b, size_t *result)
{
    if (a > SIZE_MAX - b)
    {
        return false;
    }

    *result = a + b;
    return true;
}

bool bigdecimal_i64_add(int64_t a, int64_t b, int64_t *result)
{
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b))
    {
        return false;
    }

    *result = a + b;
    return true;
}

bool bigdecimal_i64_sub(int64_t a, int64_t b, int64_t *result)
{
    if ((b > 0 && a < INT64_MIN + b) ||
        (b < 0 && a > INT64_MAX + b))
    {
        return false;
    }

    *result = a - b;
    return true;
}

BigDecimalStatus bigdecimal_scale_difference(
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

bool bigdecimal_valid_rounding(BigDecimalRoundingMode rounding)
{
    return rounding >= BIGDECIMAL_ROUND_TOWARD_ZERO &&
           rounding <= BIGDECIMAL_ROUND_HALF_EVEN;
}

void bigdecimal_commit(BigDecimal *destination, BigDecimal *temporary)
{
    BigInt *old_coefficient = destination->coefficient;

    destination->coefficient = temporary->coefficient;
    destination->scale = temporary->scale;
    temporary->coefficient = old_coefficient;

    bigdecimal_destroy(temporary);
}

// Build 10^exponent through BigInt's decimal parser. Keeping this logic in
// one helper allows a cached or power-by-squaring implementation to replace
// it later without changing the arithmetic modules.
BigDecimalStatus bigdecimal_set_power_of_ten(
    BigInt *value,
    uint64_t exponent
)
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

BigDecimalStatus bigdecimal_multiply_power_of_ten(
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

BigDecimalStatus bigdecimal_normalize(BigDecimal *value)
{
    uint64_t removed;
    BigDecimalStatus status;

    if (bigint_is_zero(value->coefficient))
    {
        value->scale = 0;
        return BIGDECIMAL_OK;
    }

    status = bigdecimal_from_bigint_status(
        bigint_strip_decimal_zeros(value->coefficient, &removed));

    if (status != BIGDECIMAL_OK)
    {
        return status;
    }

    // Subtract in representable chunks without unsigned-to-signed overflow.
    while (removed > INT64_MAX)
    {
        if (!bigdecimal_i64_sub(value->scale, INT64_MAX, &value->scale))
        {
            return BIGDECIMAL_SCALE_OVERFLOW;
        }

        removed -= INT64_MAX;
    }

    if (!bigdecimal_i64_sub(value->scale, (int64_t)removed, &value->scale))
    {
        return BIGDECIMAL_SCALE_OVERFLOW;
    }

    return BIGDECIMAL_OK;
}

// Normalize a temporary result before atomically replacing destination.
BigDecimalStatus bigdecimal_finish(
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
BigDecimalStatus bigdecimal_parse_exponent(
    const char *text,
    int64_t *result
)
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
BigDecimalExponent bigdecimal_decimal_exponent(size_t digits, int64_t scale)
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

int bigdecimal_compare_exponents(BigDecimalExponent a, BigDecimalExponent b)
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
BigDecimalStatus bigdecimal_round_quotient(
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
        case BIGDECIMAL_OK:
            return "success";
        case BIGDECIMAL_NULL_ARGUMENT:
            return "null argument";
        case BIGDECIMAL_OUT_OF_MEMORY:
            return "out of memory";
        case BIGDECIMAL_INVALID_ARGUMENT:
            return "invalid argument";
        case BIGDECIMAL_DIVISION_BY_ZERO:
            return "division by zero";
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return "value too large";
        case BIGDECIMAL_SCALE_OVERFLOW:
            return "scale overflow";
        default:
            return "unknown status";
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

BigDecimalStatus bigdecimal_copy(
    BigDecimal *destination,
    const BigDecimal *source
)
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
