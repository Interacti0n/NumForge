#include <numforge/bigrational.h>

#include "../bigdecimal/bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"

#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    An exact, reduced fraction. Every public mutation builds a complete
    temporary and exchanges ownership only after all operations succeed.
------------------------------------------------------------------------------------------------------------------------------
*/
struct BigRational
{
    BigInt *numerator;
    BigInt *denominator;
};

static BigRationalStatus rational_from_integer_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK:
            return BIGRATIONAL_OK;
        case BIGINT_NULL_ARGUMENT:
            return BIGRATIONAL_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return BIGRATIONAL_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO:
            return BIGRATIONAL_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE:
            return BIGRATIONAL_VALUE_TOO_LARGE;
        default:
            return BIGRATIONAL_INVALID_ARGUMENT;
    }
}

static BigRationalStatus rational_from_decimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return BIGRATIONAL_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return BIGRATIONAL_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return BIGRATIONAL_OUT_OF_MEMORY;
        case BIGDECIMAL_DIVISION_BY_ZERO:
            return BIGRATIONAL_DIVISION_BY_ZERO;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return BIGRATIONAL_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return BIGRATIONAL_SCALE_OVERFLOW;
        default:
            return BIGRATIONAL_INVALID_ARGUMENT;
    }
}

const char *bigrational_status_to_string(BigRationalStatus status)
{
    switch (status)
    {
        case BIGRATIONAL_OK:
            return "success";
        case BIGRATIONAL_NULL_ARGUMENT:
            return "null argument";
        case BIGRATIONAL_OUT_OF_MEMORY:
            return "out of memory";
        case BIGRATIONAL_INVALID_ARGUMENT:
            return "invalid argument";
        case BIGRATIONAL_DIVISION_BY_ZERO:
            return "division by zero";
        case BIGRATIONAL_VALUE_TOO_LARGE:
            return "value too large";
        case BIGRATIONAL_SCALE_OVERFLOW:
            return "scale overflow";
        default:
            return "unknown status";
    }
}

BigRational *bigrational_create(void)
{
    BigRational *value = numforge_malloc(sizeof(*value));
    if (value == NULL)
    {
        return NULL;
    }
    value->numerator = bigint_create();
    value->denominator = bigint_create();
    if (value->numerator == NULL || value->denominator == NULL ||
        bigint_set_string(value->denominator, "1") != BIGINT_OK)
    {
        bigrational_destroy(value);
        return NULL;
    }
    return value;
}

void bigrational_destroy(BigRational *value)
{
    if (value == NULL)
    {
        return;
    }
    bigint_destroy(value->numerator);
    bigint_destroy(value->denominator);
    free(value);
}

static void rational_commit(BigRational *destination, BigRational *temporary)
{
    BigInt *old_numerator = destination->numerator;
    BigInt *old_denominator = destination->denominator;
    destination->numerator = temporary->numerator;
    destination->denominator = temporary->denominator;
    temporary->numerator = old_numerator;
    temporary->denominator = old_denominator;
    bigrational_destroy(temporary);
}

BigRationalStatus bigrational_copy(BigRational *result, const BigRational *value)
{
    BigRational *temporary;
    BigRationalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    if (result == value)
    {
        return BIGRATIONAL_OK;
    }
    temporary = bigrational_create();
    if (temporary == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = rational_from_integer_status(bigint_copy(temporary->numerator, value->numerator));
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_copy(temporary->denominator, value->denominator));
    }
    if (status == BIGRATIONAL_OK)
    {
        rational_commit(result, temporary);
    }
    else
    {
        bigrational_destroy(temporary);
    }
    return status;
}

BigRationalStatus bigrational_set_fraction(BigRational *result, const BigInt *numerator, const BigInt *denominator)
{
    BigRational *temporary;
    BigInt *factor;
    BigRationalStatus status;
    if (result == NULL || numerator == NULL || denominator == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    if (bigint_is_zero(denominator))
    {
        return BIGRATIONAL_DIVISION_BY_ZERO;
    }
    temporary = bigrational_create();
    factor = bigint_create();
    if (temporary == NULL || factor == NULL)
    {
        bigrational_destroy(temporary);
        bigint_destroy(factor);
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = rational_from_integer_status(bigint_gcd(factor, numerator, denominator));
    if (status == BIGRATIONAL_OK && !bigint_is_zero(numerator))
    {
        status = rational_from_integer_status(bigint_div(temporary->numerator, numerator, factor));
        if (status == BIGRATIONAL_OK)
        {
            status = rational_from_integer_status(bigint_div(temporary->denominator, denominator, factor));
        }
        if (status == BIGRATIONAL_OK && bigint_is_negative(temporary->denominator))
        {
            status = rational_from_integer_status(bigint_negate(temporary->numerator, temporary->numerator));
            if (status == BIGRATIONAL_OK)
            {
                status = rational_from_integer_status(bigint_abs(temporary->denominator, temporary->denominator));
            }
        }
    }
    bigint_destroy(factor);
    if (status == BIGRATIONAL_OK)
    {
        rational_commit(result, temporary);
    }
    else
    {
        bigrational_destroy(temporary);
    }
    return status;
}

BigRationalStatus bigrational_from_bigint(BigRational *result, const BigInt *value)
{
    BigRational *temporary;
    BigRationalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    temporary = bigrational_create();
    if (temporary == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = rational_from_integer_status(bigint_copy(temporary->numerator, value));
    if (status == BIGRATIONAL_OK)
    {
        rational_commit(result, temporary);
    }
    else
    {
        bigrational_destroy(temporary);
    }
    return status;
}

BigRationalStatus bigrational_from_bigdecimal(BigRational *result, const BigDecimal *value)
{
    BigRational *temporary;
    BigRationalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    temporary = bigrational_create();
    if (temporary == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = rational_from_integer_status(bigint_copy(temporary->numerator, value->coefficient));
    if (status == BIGRATIONAL_OK && value->scale > 0)
    {
        status =
            rational_from_decimal_status(bigdecimal_set_power_of_ten(temporary->denominator, (uint64_t)value->scale));
    }
    else if (status == BIGRATIONAL_OK && value->scale < 0)
    {
        status = rational_from_decimal_status(bigdecimal_multiply_power_of_ten(
            temporary->numerator, temporary->numerator, bigdecimal_abs_i64(value->scale)));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = bigrational_set_fraction(result, temporary->numerator, temporary->denominator);
    }
    bigrational_destroy(temporary);
    return status;
}

BigRationalStatus bigrational_get_numerator(BigInt *result, const BigRational *value)
{
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_from_integer_status(bigint_copy(result, value->numerator));
}

BigRationalStatus bigrational_get_denominator(BigInt *result, const BigRational *value)
{
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_from_integer_status(bigint_copy(result, value->denominator));
}

BigRationalStatus bigrational_to_string(const BigRational *value, char **result)
{
    char *numerator;
    char *denominator = NULL;
    char *text;
    size_t first, second = 0U;
    if (value == NULL || result == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    numerator = bigint_to_string(value->numerator);
    if (numerator == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    if (!bigint_is_one(value->denominator))
    {
        denominator = bigint_to_string(value->denominator);
        if (denominator == NULL)
        {
            free(numerator);
            return BIGRATIONAL_OUT_OF_MEMORY;
        }
        second = strlen(denominator);
    }
    first = strlen(numerator);
    if (second > SIZE_MAX - 2U || first > SIZE_MAX - second - 2U)
    {
        free(numerator);
        free(denominator);
        return BIGRATIONAL_VALUE_TOO_LARGE;
    }
    text = numforge_malloc(first + second + (denominator == NULL ? 1U : 2U));
    if (text == NULL)
    {
        free(numerator);
        free(denominator);
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    memcpy(text, numerator, first);
    if (denominator != NULL)
    {
        text[first] = '/';
        memcpy(text + first + 1U, denominator, second);
        text[first + second + 1U] = '\0';
    }
    else
    {
        text[first] = '\0';
    }
    free(numerator);
    free(denominator);
    *result = text;
    return BIGRATIONAL_OK;
}

BigRationalStatus bigrational_to_bigdecimal(BigDecimal *result, const BigRational *value, int64_t digits,
                                            BigDecimalRoundingMode rounding)
{
    BigDecimal *numerator, *denominator;
    BigRationalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    if (digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGRATIONAL_INVALID_ARGUMENT;
    }
    numerator = bigdecimal_create();
    denominator = bigdecimal_create();
    if (numerator == NULL || denominator == NULL)
    {
        bigdecimal_destroy(numerator);
        bigdecimal_destroy(denominator);
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = rational_from_decimal_status(bigdecimal_from_bigint(numerator, value->numerator));
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_decimal_status(bigdecimal_from_bigint(denominator, value->denominator));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_decimal_status(
            bigdecimal_div_exact_or_significant(result, numerator, denominator, digits, rounding));
    }
    bigdecimal_destroy(numerator);
    bigdecimal_destroy(denominator);
    return status;
}

BigRationalStatus bigrational_compare(int *comparison, const BigRational *a, const BigRational *b)
{
    BigInt *g = NULL, *left = NULL, *right = NULL, *factor = NULL;
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    if (comparison == NULL || a == NULL || b == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    g = bigint_create();
    left = bigint_create();
    right = bigint_create();
    factor = bigint_create();
    if (g == NULL || left == NULL || right == NULL || factor == NULL)
    {
        goto done;
    }
    status = rational_from_integer_status(bigint_gcd(g, a->denominator, b->denominator));
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(factor, b->denominator, g));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(left, a->numerator, factor));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(factor, a->denominator, g));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(right, b->numerator, factor));
    }
    if (status == BIGRATIONAL_OK)
    {
        *comparison = bigint_compare(left, right);
    }
done:
    bigint_destroy(g);
    bigint_destroy(left);
    bigint_destroy(right);
    bigint_destroy(factor);
    return status;
}

static BigRationalStatus rational_unary(BigRational *result, const BigRational *value, bool negate)
{
    BigRational *temporary;
    BigRationalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    temporary = bigrational_create();
    if (temporary == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = bigrational_copy(temporary, value);
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(negate ? bigint_negate(temporary->numerator, temporary->numerator)
                                                     : bigint_abs(temporary->numerator, temporary->numerator));
    }
    if (status == BIGRATIONAL_OK)
    {
        rational_commit(result, temporary);
    }
    else
    {
        bigrational_destroy(temporary);
    }
    return status;
}

BigRationalStatus bigrational_negate(BigRational *result, const BigRational *value)
{
    return rational_unary(result, value, true);
}

BigRationalStatus bigrational_abs(BigRational *result, const BigRational *value)
{
    return rational_unary(result, value, false);
}

static BigRationalStatus rational_sum(BigRational *result, const BigRational *a, const BigRational *b, bool subtract)
{
    BigInt *g = NULL, *left = NULL, *right = NULL, *u = NULL, *v = NULL, *numerator = NULL, *denominator = NULL;
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    g = bigint_create();
    left = bigint_create();
    right = bigint_create();
    u = bigint_create();
    v = bigint_create();
    numerator = bigint_create();
    denominator = bigint_create();
    if (g == NULL || left == NULL || right == NULL || u == NULL || v == NULL || numerator == NULL ||
        denominator == NULL)
    {
        goto done;
    }
    status = rational_from_integer_status(bigint_gcd(g, a->denominator, b->denominator));
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(u, b->denominator, g));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(v, a->denominator, g));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(left, a->numerator, u));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(right, b->numerator, v));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(subtract ? bigint_sub(numerator, left, right)
                                                       : bigint_add(numerator, left, right));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(denominator, a->denominator, u));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = bigrational_set_fraction(result, numerator, denominator);
    }
done:
    bigint_destroy(g);
    bigint_destroy(left);
    bigint_destroy(right);
    bigint_destroy(u);
    bigint_destroy(v);
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    return status;
}

BigRationalStatus bigrational_add(BigRational *result, const BigRational *a, const BigRational *b)
{
    return rational_sum(result, a, b, false);
}

BigRationalStatus bigrational_sub(BigRational *result, const BigRational *a, const BigRational *b)
{
    return rational_sum(result, a, b, true);
}

static BigRationalStatus rational_product(BigRational *result, const BigRational *a, const BigRational *b, bool divide)
{
    BigInt *g1 = NULL, *g2 = NULL, *p = NULL, *q = NULL, *u = NULL, *v = NULL;
    BigInt *numerator = NULL, *denominator = NULL;
    const BigInt *top = divide ? b->denominator : b->numerator;
    const BigInt *bottom = divide ? b->numerator : b->denominator;
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    if (divide && bigint_is_zero(b->numerator))
    {
        return BIGRATIONAL_DIVISION_BY_ZERO;
    }
    g1 = bigint_create();
    g2 = bigint_create();
    p = bigint_create();
    q = bigint_create();
    u = bigint_create();
    v = bigint_create();
    numerator = bigint_create();
    denominator = bigint_create();
    if (g1 == NULL || g2 == NULL || p == NULL || q == NULL || u == NULL || v == NULL || numerator == NULL ||
        denominator == NULL)
    {
        goto done;
    }
    status = rational_from_integer_status(bigint_gcd(g1, a->numerator, bottom));
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_gcd(g2, top, a->denominator));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(p, a->numerator, g1));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(q, top, g2));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(u, a->denominator, g2));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_div(v, bottom, g1));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(numerator, p, q));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(denominator, u, v));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = bigrational_set_fraction(result, numerator, denominator);
    }
done:
    bigint_destroy(g1);
    bigint_destroy(g2);
    bigint_destroy(p);
    bigint_destroy(q);
    bigint_destroy(u);
    bigint_destroy(v);
    bigint_destroy(numerator);
    bigint_destroy(denominator);
    return status;
}

BigRationalStatus bigrational_mul(BigRational *result, const BigRational *a, const BigRational *b)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_product(result, a, b, false);
}

BigRationalStatus bigrational_div(BigRational *result, const BigRational *a, const BigRational *b)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_product(result, a, b, true);
}
