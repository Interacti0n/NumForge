#include <numforge/bigdecimal.h>
#include "bigdecimal_internal.h"
#include "../bigint/bigint_internal.h"
#include "../internal/numforge_alloc.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Root helpers. Only integer arithmetic is used. Newton iterates down from
    an upper bound; a non-decreasing next iterate identifies the floor root.
    An optional caller-owned runtime budget can cover temporary operations;
    ordinary public calls remain unrestricted.
------------------------------------------------------------------------------------------------------------------------------
*/
static BigIntStatus integer_root(BigInt *root, const BigInt *number, uint32_t degree)
{
    size_t bits;
    BigInt *power;
    BigInt *next;
    BigInt *order;
    BigInt *previous_order;
    BigIntStatus status = BIGINT_OUT_OF_MEMORY;
    char text[16];

    if (bigint_is_zero(number))
    {
        return bigint_set_string(root, "0");
    }

    bits = (number->size - 1U) * 64U;

    for (uint64_t top = number->limbs[number->size - 1U]; top != 0; top >>= 1U)
    {
        bits++;
    }

    if (bits <= degree)
    {
        return bigint_set_string(root, "1");
    }

    power = bigint_create();
    next = bigint_create();
    order = bigint_create();
    previous_order = bigint_create();

    if (power == NULL || next == NULL || order == NULL || previous_order == NULL)
    {
        goto cleanup;
    }

#define INTEGER_TRY(operation)              \
    do                                      \
    {                                       \
        status = (operation);               \
        if (status != BIGINT_OK)            \
        {                                   \
            goto cleanup;                   \
        }                                   \
    } while (0)

    (void)snprintf(text, sizeof(text), "%" PRIu32, degree);
    INTEGER_TRY(bigint_set_string(order, text));
    (void)snprintf(text, sizeof(text), "%" PRIu32, degree - 1U);
    INTEGER_TRY(bigint_set_string(previous_order, text));
    INTEGER_TRY(bigint_set_string(root, "1"));
    INTEGER_TRY(bigint_shift_left(root, root, bits / degree + (bits % degree != 0U)));
    for (;;)
    {
        if (!numforge_budget_check())
        {
            status = BIGINT_OUT_OF_MEMORY;
            break;
        }

        INTEGER_TRY(bigint_pow(power, root, previous_order));
        INTEGER_TRY(bigint_div(next, number, power));
        INTEGER_TRY(bigint_mul(power, root, previous_order));
        INTEGER_TRY(bigint_add(next, next, power));
        INTEGER_TRY(bigint_div(next, next, order));

        if (bigint_compare(next, root) >= 0)
        {
            break;
        }

        INTEGER_TRY(bigint_copy(root, next));
    }

cleanup:
    bigint_destroy(power);
    bigint_destroy(next);
    bigint_destroy(order);
    bigint_destroy(previous_order);

    return status;

#undef INTEGER_TRY
}

static BigDecimalStatus decimal_status(BigIntStatus status)
{
    if (status == BIGINT_OK)
    {
        return BIGDECIMAL_OK;
    }

    if (status == BIGINT_OUT_OF_MEMORY)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    if (status == BIGINT_VALUE_TOO_LARGE)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    return BIGDECIMAL_INVALID_ARGUMENT;
}

// Use the normal decimal parser to retain canonical coefficients and scales.
// A sticky digit encodes a nonzero tail only for the subsequent rounding.
static BigDecimalStatus set_root(
    BigDecimal *result,
    const BigInt *root,
    bool negative,
    int64_t scale,
    bool sticky
)
{
    char *coefficient;
    size_t capacity;
    char *text;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;

    coefficient = bigint_to_string(root);

    if (coefficient == NULL)
    {
        return BIGDECIMAL_OUT_OF_MEMORY;
    }

    capacity = strlen(coefficient) + 32U;
    text = numforge_malloc(capacity);

    if (text != NULL)
    {
        (void)snprintf(
            text,
            capacity,
            "%s%s%sE%" PRId64,
            negative ? "-" : "", coefficient, sticky ? "1" : "", -scale);
        status = bigdecimal_set_string(result, text);
    }

    free(text);
    free(coefficient);

    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Real-root operation. Split the decimal exponent by the degree before
    scaling, so compact inputs such as 1E100000 never expand into that many
    digits. One guard digit plus a nonzero-tail marker suffices for all six
    rounding modes; full output does not imply an infinite irrational result.
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_root(
    BigDecimal *result,
    const BigDecimal *value,
    uint32_t degree,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    bool negative;
    BigInt *number;
    BigInt *root;
    BigInt *power;
    BigInt *order;
    BigDecimal *temporary;
    char *coefficient = NULL;
    char *scaled = NULL;
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;

    if (result == NULL || value == NULL)
    {
        return BIGDECIMAL_NULL_ARGUMENT;
    }

    if (degree == 0 || digits < 1 ||
        rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (digits > INT64_MAX / 2 - 1)
    {
        return BIGDECIMAL_VALUE_TOO_LARGE;
    }

    negative = bigint_is_negative(value->coefficient);

    if (negative && degree % 2U == 0U)
    {
        return BIGDECIMAL_INVALID_ARGUMENT;
    }

    if (degree == 1 || bigint_is_zero(value->coefficient))
    {
        return bigdecimal_copy(result, value);
    }

    number = bigint_create();
    root = bigint_create();
    power = bigint_create();
    order = bigint_create();
    temporary = bigdecimal_create();

    if (number == NULL || root == NULL || power == NULL ||
        order == NULL || temporary == NULL)
    {
        goto cleanup;
    }

#define ROOT_TRY(operation)                  \
    do                                      \
    {                                       \
        status = (operation);               \
        if (status != BIGDECIMAL_OK)        \
        {                                   \
            goto cleanup;                   \
        }                                   \
    } while (0)

    ROOT_TRY(decimal_status(bigint_abs(number, value->coefficient)));

    if (value->scale % degree == 0)
    {
        char text[16];
        ROOT_TRY(decimal_status(integer_root(root, number, degree)));
        (void)snprintf(text, sizeof(text), "%" PRIu32, degree);
        ROOT_TRY(decimal_status(bigint_set_string(order, text)));
        ROOT_TRY(decimal_status(bigint_pow(power, root, order)));
        if (bigint_compare(power, number) == 0)
        {
            ROOT_TRY(set_root(temporary, root, negative, value->scale / degree, false));
            ROOT_TRY(bigdecimal_copy(result, temporary));
            goto cleanup;
        }
    }

    coefficient = bigint_to_string(number);

    if (coefficient == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    size_t length = strlen(coefficient);

    // Calculate floor((length - 1 - scale) / degree) without overflowing
    // when the scale is INT64_MIN.
    int64_t adjustment = (int64_t)(length - 1U) - value->scale % degree;
    int64_t quotient = adjustment / degree;

    if (adjustment % degree < 0)
    {
        quotient--;
    }

    int64_t exponent = -(value->scale / degree) + quotient;
    int64_t remainder = adjustment - quotient * degree;

    if ((uint64_t)digits > (UINT64_MAX - (uint64_t)remainder - 1U) / degree)
    {
        status = BIGDECIMAL_VALUE_TOO_LARGE;
        goto cleanup;
    }

    uint64_t required = (uint64_t)degree * (uint64_t)digits + (uint64_t)remainder + 1U;

    if (required >= SIZE_MAX)
    {
        status = BIGDECIMAL_VALUE_TOO_LARGE;
        goto cleanup;
    }

    size_t count = (size_t)required;

    scaled = numforge_malloc(count + 1U);

    if (scaled == NULL)
    {
        status = BIGDECIMAL_OUT_OF_MEMORY;
        goto cleanup;
    }

    size_t copied = length < count ? length : count;

    memcpy(scaled, coefficient, copied);
    memset(scaled + copied, '0', count - copied);
    scaled[count] = '\0';
    ROOT_TRY(decimal_status(bigint_set_string(number, scaled)));
    ROOT_TRY(decimal_status(integer_root(root, number, degree)));

    // The exact-finite branch above failed, so the omitted tail is nonzero.
    // Truncating the radicand first cannot change the floor integer root.
    ROOT_TRY(set_root(temporary, root, negative, digits - exponent + 1, true));
    ROOT_TRY(bigdecimal_rescale(result, temporary, digits - 1 - exponent, rounding));

cleanup:
    free(coefficient);
    free(scaled);
    bigint_destroy(number);
    bigint_destroy(root);
    bigint_destroy(power);
    bigint_destroy(order);
    bigdecimal_destroy(temporary);

    return status;

#undef ROOT_TRY
}

BigDecimalStatus bigdecimal_sqrt(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return bigdecimal_root(result, value, 2, digits, rounding);
}

BigDecimalStatus bigdecimal_cbrt(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
)
{
    return bigdecimal_root(result, value, 3, digits, rounding);
}
