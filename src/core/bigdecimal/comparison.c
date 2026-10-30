#include "src/core/bigdecimal/bigdecimal_internal.h"
#include "src/runtime/numforge_alloc.h"

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Comparison and inspection functions for BigDecimal. */

BigDecimalStatus bigdecimal_compare(
    int *comparison,
    const BigDecimal *a,
    const BigDecimal *b
)
{
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

    // Equal scales need no decimal serialization or coefficient alignment.
    if (a->scale == b->scale)
    {
        if (!numforge_budget_check())
        {
            return BIGDECIMAL_OUT_OF_MEMORY;
        }
        *comparison = bigint_compare(a->coefficient, b->coefficient);
        return BIGDECIMAL_OK;
    }

    size_t a_low = 0, a_high = 0, b_low = 0, b_high = 0;
    BigDecimalStatus status = bigdecimal_from_bigint_status(bigint_decimal_digit_bounds(a->coefficient, &a_low, &a_high));
    if (status == BIGDECIMAL_OK) status = bigdecimal_from_bigint_status(bigint_decimal_digit_bounds(b->coefficient, &b_low, &b_high));
    if (status != BIGDECIMAL_OK) return status;
    /* Prove disjoint exponent intervals before asking for exact digit counts.
     * This is integer arithmetic, including scale extremes; no float estimate
     * or textual prefix decides the comparison. */
    if (bigdecimal_compare_exponents(bigdecimal_decimal_exponent(a_low, a->scale),
                                    bigdecimal_decimal_exponent(b_high, b->scale)) > 0)
        magnitude_comparison = 1;
    else if (bigdecimal_compare_exponents(bigdecimal_decimal_exponent(a_high, a->scale),
                                         bigdecimal_decimal_exponent(b_low, b->scale)) < 0)
        magnitude_comparison = -1;
    if (magnitude_comparison) {
        *comparison = a_negative ? -magnitude_comparison : magnitude_comparison;
        return BIGDECIMAL_OK;
    }
    a_length = a_low; b_length = b_low;
    if (a_low != a_high) status = bigdecimal_from_bigint_status(bigint_decimal_digits(a->coefficient, &a_length));
    if (status == BIGDECIMAL_OK && b_low != b_high) status = bigdecimal_from_bigint_status(bigint_decimal_digits(b->coefficient, &b_length));
    if (status != BIGDECIMAL_OK) return status;
    a_exponent = bigdecimal_decimal_exponent(a_length, a->scale);
    b_exponent = bigdecimal_decimal_exponent(b_length, b->scale);
    magnitude_comparison = bigdecimal_compare_exponents(a_exponent, b_exponent);

    if (magnitude_comparison == 0)
    {
        BigInt aligned = {0};
        if (a_length < b_length) {
            status = bigdecimal_align_coefficient_digits(&aligned, a->coefficient, b_length - a_length);
            if (status == BIGDECIMAL_OK) magnitude_comparison = bigint_compare_abs(&aligned, b->coefficient);
        } else if (b_length < a_length) {
            status = bigdecimal_align_coefficient_digits(&aligned, b->coefficient, a_length - b_length);
            if (status == BIGDECIMAL_OK) magnitude_comparison = bigint_compare_abs(a->coefficient, &aligned);
        } else magnitude_comparison = bigint_compare_abs(a->coefficient, b->coefficient);
        free(aligned.limbs);
        if (status != BIGDECIMAL_OK) return status;
    }

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
