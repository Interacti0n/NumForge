#include "bigdecimal_internal.h"
#include "../internal/numforge_alloc.h"
#include <stdlib.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Public decimal conversions and exact operations. Temporary destinations
    preserve caller values on failure, including output/input aliasing.
------------------------------------------------------------------------------------------------------------------------------
*/
static BigDecimalStatus from_integer_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK: return BIGDECIMAL_OK;
        case BIGINT_NULL_ARGUMENT: return BIGDECIMAL_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY: return BIGDECIMAL_OUT_OF_MEMORY;
        case BIGINT_VALUE_TOO_LARGE: return BIGDECIMAL_VALUE_TOO_LARGE;
        default: return BIGDECIMAL_INVALID_ARGUMENT;
    }
}

BigDecimalStatus bigdecimal_from_bigint(BigDecimal *result, const BigInt *value)
{
    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    char *text = bigint_to_string(value);
    if (text == NULL) return BIGDECIMAL_OUT_OF_MEMORY;
    BigDecimalStatus status = bigdecimal_set_string(result, text);
    free(text);
    return status;
}

BigDecimalStatus bigdecimal_to_bigint(BigInt *result, const BigDecimal *value)
{
    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (value->scale > 0) return BIGDECIMAL_INVALID_ARGUMENT;
    char *text = NULL;
    BigDecimalStatus status = bigdecimal_to_string(value, &text);
    if (status == BIGDECIMAL_OK) status = from_integer_status(bigint_set_string(result, text));
    free(text);
    return status;
}

BigDecimalStatus bigdecimal_is_integer(bool *result, const BigDecimal *value)
{
    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    *result = value->scale <= 0;
    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_sign(int *result, const BigDecimal *value)
{
    if (result == NULL || value == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    *result = bigint_is_zero(value->coefficient) ? 0 : bigint_is_negative(value->coefficient) ? -1 : 1;
    return BIGDECIMAL_OK;
}

BigDecimalStatus bigdecimal_min(BigDecimal *result, const BigDecimal *a, const BigDecimal *b)
{
    if (result == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    int comparison;
    BigDecimalStatus status = bigdecimal_compare(&comparison, a, b);
    return status == BIGDECIMAL_OK ? bigdecimal_copy(result, comparison <= 0 ? a : b) : status;
}

BigDecimalStatus bigdecimal_max(BigDecimal *result, const BigDecimal *a, const BigDecimal *b)
{
    if (result == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    int comparison;
    BigDecimalStatus status = bigdecimal_compare(&comparison, a, b);
    return status == BIGDECIMAL_OK ? bigdecimal_copy(result, comparison >= 0 ? a : b) : status;
}

BigDecimalStatus bigdecimal_pow(BigDecimal *result, const BigDecimal *base, const BigInt *exponent)
{
    if (result == NULL || base == NULL || exponent == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (bigint_is_negative(exponent)) return BIGDECIMAL_INVALID_ARGUMENT;
    BigInt *remaining = bigint_create();
    BigDecimal *accumulator = bigdecimal_create(), *factor = bigdecimal_create();
    BigDecimalStatus status = BIGDECIMAL_OUT_OF_MEMORY;
    if (remaining == NULL || accumulator == NULL || factor == NULL) goto cleanup;
    status = from_integer_status(bigint_copy(remaining, exponent));
    if (status == BIGDECIMAL_OK) status = bigdecimal_set_string(accumulator, "1");
    if (status == BIGDECIMAL_OK) status = bigdecimal_copy(factor, base);
    while (status == BIGDECIMAL_OK && !bigint_is_zero(remaining))
    {
        if (!numforge_budget_check()) { status = BIGDECIMAL_OUT_OF_MEMORY; break; }
        if (bigint_is_odd(remaining)) status = bigdecimal_mul(accumulator, accumulator, factor);
        if (status == BIGDECIMAL_OK) status = from_integer_status(bigint_shift_right(remaining, remaining, 1U));
        if (status == BIGDECIMAL_OK && !bigint_is_zero(remaining)) status = bigdecimal_mul(factor, factor, factor);
    }
    if (status == BIGDECIMAL_OK) status = bigdecimal_copy(result, accumulator);
cleanup:
    bigint_destroy(remaining); bigdecimal_destroy(accumulator); bigdecimal_destroy(factor);
    return status;
}
