#include "src/core/bigdecimal/bigdecimal_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>
#include <string.h>

void bigdecimal_workspace_clear(BigDecimalWorkspace *workspace)
{
    free(workspace->quotient.limbs); free(workspace->remainder.limbs);
    free(workspace->aligned.limbs); free(workspace->scaled.limbs);
    free(workspace->power.limbs); free(workspace->twice.limbs);
    free(workspace->arithmetic.limbs);
    free(workspace->division.numerator.limbs); free(workspace->division.divisor.limbs);
    memset(workspace, 0, sizeof(*workspace));
}

/* Copy only after every fallible normalization/scale operation is complete.
 * bigint_copy grows before writing, so even input/output aliases are atomic. */
static BigDecimalStatus publish(BigDecimal *result, const BigDecimal *temporary)
{
    BigDecimalStatus status = bigdecimal_from_bigint_status(
        bigint_copy(result->coefficient, temporary->coefficient));
    if (status == BIGDECIMAL_OK) result->scale = temporary->scale;
    return status;
}

static BigDecimalStatus cached_power_unprofiled(BigDecimalWorkspace *workspace, uint64_t exponent)
{
    if (exponent > SIZE_MAX - 2U) return BIGDECIMAL_VALUE_TOO_LARGE;
    if (workspace->power_valid && workspace->power_exponent == exponent) return BIGDECIMAL_OK;
    workspace->power_valid = false;
    /* Preserve the existing early allocation/budget rejection for compact
     * inputs with extreme exponents instead of starting an enormous chunk loop. */
    if (exponent > UINT64_C(65536))
    {
        BigDecimalStatus status = bigdecimal_set_power_of_ten(&workspace->power, exponent);
        if (status == BIGDECIMAL_OK)
        {
            workspace->power_exponent = exponent;
            workspace->power_valid = true;
        }
        return status;
    }
    BigIntStatus status = bigint_set_uint64(&workspace->power, 1U);
    /* Private coefficient: failure may leave partial scratch, never an output.
     * Chunked multiplication also avoids constructing/parsing decimal text. */
    uint64_t remaining = exponent, last = 1U;
    while (status == BIGINT_OK && remaining >= 19U)
    {
        if (!numforge_budget_check()) return BIGDECIMAL_OUT_OF_MEMORY;
        status = bigint_multiply_by_uint64(&workspace->power, UINT64_C(10000000000000000000));
        remaining -= 19U;
    }
    if (status != BIGINT_OK) return bigdecimal_from_bigint_status(status);
    while (remaining > 0U) { last *= 10U; remaining--; }
    if (status == BIGINT_OK) status = bigint_multiply_by_uint64(&workspace->power, last);
    if (status == BIGINT_OK)
    {
        workspace->power_exponent = exponent;
        workspace->power_valid = true;
    }
    return bigdecimal_from_bigint_status(status);
}

static BigDecimalStatus cached_power(BigDecimalWorkspace *workspace, uint64_t exponent)
{
    NumForgeDecimalPhase previous = numforge_decimal_enter(NUMFORGE_DECIMAL_ALIGNMENT);
    BigDecimalStatus status = cached_power_unprofiled(workspace, exponent);
    numforge_decimal_leave(previous);
    return status;
}

static BigDecimalStatus scale_coefficient(BigInt *result, const BigInt *value, uint64_t places)
{
    if (places > UINT64_C(65536)) return bigdecimal_multiply_power_of_ten(result, value, places);
    return bigdecimal_align_coefficient_digits(result, value, places);
}

static BigDecimalStatus divide_and_round(BigDecimalWorkspace *workspace,
    const BigInt *numerator, const BigInt *divisor, BigDecimalRoundingMode rounding)
{
    bool negative = bigint_is_negative(numerator) != bigint_is_negative(divisor);
    BigDecimalStatus status = bigdecimal_from_bigint_status(bigint_divmod_abs_workspace(
        &workspace->quotient, &workspace->remainder, numerator, divisor, &workspace->division));
    if (status != BIGDECIMAL_OK) return status;
    workspace->quotient.is_negative = negative && workspace->quotient.size != 0U;
    workspace->remainder.is_negative = false;
    return bigdecimal_round_quotient_scratch(&workspace->quotient, &workspace->remainder,
        divisor, negative, rounding, &workspace->twice);
}

BigDecimalStatus bigdecimal_round_significant_workspace(BigDecimal *result,
    const BigDecimal *value, int64_t digits, BigDecimalRoundingMode rounding,
    BigDecimalWorkspace *workspace)
{
    if (result == NULL || value == NULL || workspace == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (digits < 1 || !bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return BIGDECIMAL_OUT_OF_MEMORY;
    if (bigint_is_zero(value->coefficient)) return publish(result, value);
    size_t length;
    BigDecimalStatus status = bigdecimal_from_bigint_status(bigint_decimal_digits(value->coefficient, &length));
    if (status != BIGDECIMAL_OK) return status;
    if ((uint64_t)length <= (uint64_t)digits) return publish(result, value);
    size_t discarded = length - (size_t)digits;
#if SIZE_MAX > INT64_MAX
    if (discarded > (size_t)INT64_MAX) return BIGDECIMAL_SCALE_OVERFLOW;
#endif
    int64_t target;
    if (!bigdecimal_i64_sub(value->scale, (int64_t)discarded, &target)) return BIGDECIMAL_SCALE_OVERFLOW;
    BigInt small_power;
    uint64_t storage = 1U;
    const BigInt *divisor = &workspace->power;
    if (discarded <= 19U)
    {
        for (size_t i = 0; i < discarded; i++) storage *= 10U;
        (void)bigint_wrap_uint64(&small_power, &storage, storage);
        divisor = &small_power;
    }
    else status = cached_power(workspace, (uint64_t)discarded);
    if (status == BIGDECIMAL_OK)
        status = divide_and_round(workspace, value->coefficient, divisor, rounding);
    BigDecimal temporary = { &workspace->quotient, target };
    if (status == BIGDECIMAL_OK) status = bigdecimal_normalize(&temporary);
    if (status == BIGDECIMAL_OK) status = publish(result, &temporary);
    return status;
}

BigDecimalStatus bigdecimal_div_significant_workspace(BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits,
    BigDecimalRoundingMode rounding, BigDecimalWorkspace *workspace)
{
    if (result == NULL || a == NULL || b == NULL || workspace == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (digits < 1 || !bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    if (bigint_is_zero(b->coefficient)) return BIGDECIMAL_DIVISION_BY_ZERO;
    if (!numforge_budget_check()) return BIGDECIMAL_OUT_OF_MEMORY;
    if (bigint_is_zero(a->coefficient)) return publish(result, a);
    int64_t target;
    BigDecimalStatus status = bigdecimal_significant_target(a, b, digits, &workspace->aligned, &target);
    if (status != BIGDECIMAL_OK) return status;
    const BigInt *numerator = a->coefficient, *divisor = b->coefficient;
    if (target != 0)
    {
        uint64_t places = bigdecimal_abs_i64(target);
        if (places > SIZE_MAX - 2U) return BIGDECIMAL_VALUE_TOO_LARGE;
        status = scale_coefficient(&workspace->scaled,
            target > 0 ? numerator : divisor, places);
        if (target > 0) numerator = &workspace->scaled;
        else divisor = &workspace->scaled;
    }
    if (status == BIGDECIMAL_OK) status = divide_and_round(workspace, numerator, divisor, rounding);
    BigDecimal temporary = { &workspace->quotient, target };
    if (status == BIGDECIMAL_OK) status = bigdecimal_normalize(&temporary);
    if (status == BIGDECIMAL_OK && !bigint_is_zero(temporary.coefficient))
    {
        int64_t scale;
        if (!((bigdecimal_i64_add(temporary.scale, a->scale, &scale) &&
               bigdecimal_i64_sub(scale, b->scale, &scale)) ||
              (bigdecimal_i64_sub(temporary.scale, b->scale, &scale) &&
               bigdecimal_i64_add(scale, a->scale, &scale)) ||
              (bigdecimal_i64_sub(a->scale, b->scale, &scale) &&
               bigdecimal_i64_add(scale, temporary.scale, &scale))))
            status = BIGDECIMAL_SCALE_OVERFLOW;
        else temporary.scale = scale;
    }
    if (status == BIGDECIMAL_OK) status = publish(result, &temporary);
    return status;
}

/* Private raw arithmetic and alignment survive the whole series. Normalize
 * before rounding, exactly as the public operation followed by rounding does. */
BigDecimalStatus bigdecimal_mul_significant_workspace(BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits,
    BigDecimalRoundingMode rounding, BigDecimalWorkspace *workspace)
{
    if (result == NULL || a == NULL || b == NULL || workspace == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (digits < 1 || !bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return BIGDECIMAL_OUT_OF_MEMORY;
    BigDecimalStatus status = bigdecimal_from_bigint_status(
        bigint_mul(&workspace->arithmetic, a->coefficient, b->coefficient));
    BigDecimal temporary = { &workspace->arithmetic, 0 };
    if (status == BIGDECIMAL_OK) status = bigdecimal_normalize(&temporary);
    if (status == BIGDECIMAL_OK && !bigint_is_zero(temporary.coefficient))
    {
        int64_t scale;
        if (!((bigdecimal_i64_add(a->scale, temporary.scale, &scale) &&
               bigdecimal_i64_add(scale, b->scale, &scale)) ||
              (bigdecimal_i64_add(b->scale, temporary.scale, &scale) &&
               bigdecimal_i64_add(scale, a->scale, &scale)) ||
              (bigdecimal_i64_add(a->scale, b->scale, &scale) &&
               bigdecimal_i64_add(scale, temporary.scale, &scale))))
            status = BIGDECIMAL_SCALE_OVERFLOW;
        else temporary.scale = scale;
    }
    if (status == BIGDECIMAL_OK)
        status = bigdecimal_round_significant_workspace(result, &temporary, digits, rounding, workspace);
    return status;
}

BigDecimalStatus bigdecimal_add_significant_workspace(BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits,
    BigDecimalRoundingMode rounding, BigDecimalWorkspace *workspace)
{
    if (result == NULL || a == NULL || b == NULL || workspace == NULL) return BIGDECIMAL_NULL_ARGUMENT;
    if (digits < 1 || !bigdecimal_valid_rounding(rounding)) return BIGDECIMAL_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return BIGDECIMAL_OUT_OF_MEMORY;
    if (bigint_is_zero(a->coefficient))
        return bigdecimal_round_significant_workspace(result, b, digits, rounding, workspace);
    if (bigint_is_zero(b->coefficient))
        return bigdecimal_round_significant_workspace(result, a, digits, rounding, workspace);
    int64_t scale = a->scale >= b->scale ? a->scale : b->scale;
    const BigInt *left = a->coefficient, *right = b->coefficient;
    BigDecimalStatus status = BIGDECIMAL_OK;
    if (a->scale != b->scale)
    {
        uint64_t places;
        status = bigdecimal_scale_difference(scale, a->scale < b->scale ? a->scale : b->scale, &places);
        if (status == BIGDECIMAL_OK && places > SIZE_MAX - 2U) status = BIGDECIMAL_VALUE_TOO_LARGE;
        if (status == BIGDECIMAL_OK)
            status = scale_coefficient(&workspace->scaled,
                a->scale < b->scale ? left : right, places);
        if (a->scale < b->scale) left = &workspace->scaled;
        else right = &workspace->scaled;
    }
    if (status != BIGDECIMAL_OK) return status;
    BigIntStatus integer_status;
    bool negative;
    if (left->is_negative == right->is_negative)
    {
        negative = left->is_negative;
        integer_status = bigint_add_abs(&workspace->arithmetic, left, right);
    }
    else if (bigint_compare_abs(left, right) >= 0)
    {
        negative = left->is_negative;
        integer_status = bigint_subtract_abs(&workspace->arithmetic, left, right);
    }
    else
    {
        negative = right->is_negative;
        integer_status = bigint_subtract_abs(&workspace->arithmetic, right, left);
    }
    status = bigdecimal_from_bigint_status(integer_status);
    if (status != BIGDECIMAL_OK) return status;
    workspace->arithmetic.is_negative = negative && workspace->arithmetic.size != 0U;
    BigDecimal temporary = { &workspace->arithmetic, scale };
    status = bigdecimal_normalize(&temporary);
    if (status == BIGDECIMAL_OK)
        status = bigdecimal_round_significant_workspace(result, &temporary, digits, rounding, workspace);
    return status;
}
