#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/bigint.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Arithmetic and division functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

BigIntStatus bigint_add( /*Add two BigInts (a+b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    BigIntStatus status;
    bool negative;

    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (a->is_negative && b->is_negative)
    {
        negative = true;
        status = bigint_add_abs(result, a, b);
    }
    else if (a->is_negative || b->is_negative)
    {
        int comparison = bigint_compare_abs(a, b);

        if (a->is_negative)
        {
            negative = comparison > 0;
            status = comparison > 0
                ? bigint_subtract_abs(result, a, b)
                : bigint_subtract_abs(result, b, a);
        }
        else
        {
            negative = comparison < 0;
            status = comparison >= 0
                ? bigint_subtract_abs(result, a, b)
                : bigint_subtract_abs(result, b, a);
        }
    }
    else
    {
        negative = false;
        status = bigint_add_abs(result, a, b);
    }

    if (status == BIGINT_OK)
    {
        result->is_negative = negative;
        bigint_normalize(result);
    }

    return status;
}

BigIntStatus bigint_abs( /*Absolute value of a BigInt (|a|)*/
    BigInt *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    BigIntStatus status = bigint_copy(result, value);

    if (status != BIGINT_OK)
    {
        return status;
    }

    result->is_negative = false;

    return BIGINT_OK;
}

BigIntStatus bigint_negate( /*Negate a BigInt (-a)*/
    BigInt *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    BigIntStatus status = bigint_copy(result, value);

    if (status != BIGINT_OK)
    {
        return status;
    }

    result->is_negative = !value->is_negative;

    // Zero has no sign - flipping it must not produce a "negative zero"
    // that would compare unequal to a plain zero.
    bigint_normalize(result);

    return BIGINT_OK;
}

BigIntStatus bigint_sub( /*Subtract two BigInts (a-b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    // a - b == a + (-b). Delegating to bigint_add (which already handles
    // every sign combination, and every aliasing combination, correctly)
    // via a fresh negated copy of b decouples this from the original b
    // entirely before result is ever touched.
    BigInt negated_b;
    negated_b.limbs = NULL;
    negated_b.size = 0;
    negated_b.capacity = 0;
    negated_b.is_negative = false;

    BigIntStatus status = bigint_negate(&negated_b, b);

    if (status == BIGINT_OK)
    {
        status = bigint_add(result, a, &negated_b);
    }

    free(negated_b.limbs);

    return status;
}

BigIntStatus bigint_mul( /*Multiply two BigInts (a*b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (a->size == 0 || b->size == 0)
    {
        result->size = 0;
        result->is_negative = false;

        return BIGINT_OK;
    }

    size_t result_size;

    if (bigint_size_add(a->size, b->size, &result_size) != BIGINT_OK ||
        result_size > BIGINT_MAX_LIMBS)
    {
        return BIGINT_VALUE_TOO_LARGE;
    }

    // Accumulate into a fresh buffer rather than result->limbs directly, so
    // this is safe even when result aliases a and/or b (e.g. squaring via
    // bigint_mul(x, x, x)). calloc itself checks result_size*sizeof(uint64_t)
    // for overflow on top of the result_size computation above.
    uint64_t *product = numforge_calloc(result_size, sizeof(uint64_t));

    if (product == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < b->size; i++)
    {
        if (!numforge_budget_check())
        {
            free(product);
            return BIGINT_OUT_OF_MEMORY;
        }
        uint64_t carry = 0;

        for (size_t j = 0; j < a->size; j++)
        {
            uint64_t high;
            uint64_t low;

            bigint_multiply_u64_u64(a->limbs[j], b->limbs[i], &high, &low);

            uint64_t sum = product[i + j] + low;
            uint64_t overflow1 = (sum < low);

            uint64_t sum2 = sum + carry;
            uint64_t overflow2 = (sum2 < sum);

            product[i + j] = sum2;

            // high can be at most UINT64_MAX-1 (achieved only when both
            // operands are UINT64_MAX, which forces low=1 and pins overflow2
            // to 0), so this sum never overflows a uint64_t.
            carry = high + overflow1 + overflow2;
        }

        product[i + a->size] = carry;
    }

    free(result->limbs);
    result->limbs = product;
    result->capacity = result_size;
    result->size = result_size;

    result->is_negative = a->is_negative != b->is_negative;

    bigint_normalize(result);

    return BIGINT_OK;
}

BigIntStatus bigint_div_mod( /*Divide and modulo operation for BigInts (a/b and a%b), truncating toward zero*/
    BigInt *quotient,
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
)
{
    if (quotient == NULL || remainder == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (quotient == remainder)
    {
        return BIGINT_INVALID_ARGUMENT;
    }
    if (b->size == 0)
    {
        return BIGINT_DIVISION_BY_ZERO;
    }

    // Everything is computed into independent local temporaries first, then
    // committed to the caller's quotient and remainder at the very end.
    // This keeps every allowed output/input aliasing combination safe.
    BigInt a_copy;
    a_copy.limbs = NULL;
    a_copy.size = 0;
    a_copy.capacity = 0;
    a_copy.is_negative = false;

    BigInt b_copy;
    b_copy.limbs = NULL;
    b_copy.size = 0;
    b_copy.capacity = 0;
    b_copy.is_negative = false;

    BigInt temp_quotient;
    temp_quotient.limbs = NULL;
    temp_quotient.size = 0;
    temp_quotient.capacity = 0;
    temp_quotient.is_negative = false;

    BigInt temp_remainder;
    temp_remainder.limbs = NULL;
    temp_remainder.size = 0;
    temp_remainder.capacity = 0;
    temp_remainder.is_negative = false;

    bool a_negative = a->is_negative;
    bool b_negative = b->is_negative;

    BigIntStatus status = bigint_copy(&a_copy, a);

    if (status == BIGINT_OK)
    {
        status = bigint_copy(&b_copy, b);
    }

    if (status == BIGINT_OK)
    {
        status = bigint_divmod_abs(&temp_quotient, &temp_remainder, &a_copy, &b_copy);
    }

    if (status == BIGINT_OK)
    {
        temp_quotient.is_negative = (a_negative != b_negative);
        bigint_normalize(&temp_quotient);

        temp_remainder.is_negative = a_negative;
        bigint_normalize(&temp_remainder);

        bigint_commit(quotient, &temp_quotient);
        bigint_commit(remainder, &temp_remainder);
    }

    free(a_copy.limbs);
    free(b_copy.limbs);
    free(temp_quotient.limbs);
    free(temp_remainder.limbs);

    return status;
}

BigIntStatus bigint_div( /*Divide two BigInts (a/b), truncating toward zero*/
    BigInt *quotient,
    const BigInt *a,
    const BigInt *b
)
{
    if (quotient == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    BigInt remainder;
    remainder.limbs = NULL;
    remainder.size = 0;
    remainder.capacity = 0;
    remainder.is_negative = false;

    BigIntStatus status = bigint_div_mod(quotient, &remainder, a, b);

    free(remainder.limbs);

    return status;
}

BigIntStatus bigint_mod( /*Modulo operation for BigInts (a%b)*/
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
)
{
    if (remainder == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    BigInt quotient;
    quotient.limbs = NULL;
    quotient.size = 0;
    quotient.capacity = 0;
    quotient.is_negative = false;

    BigIntStatus status = bigint_div_mod(&quotient, remainder, a, b);

    free(quotient.limbs);

    return status;
}

BigIntStatus bigint_pow( /*Exponentiation for BigInts (base^exponent) via binary exponentiation*/
    BigInt *result,
    const BigInt *base,
    const BigInt *exponent
)
{
    if (result == NULL || base == NULL || exponent == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (exponent->is_negative)
    {
        return BIGINT_NEGATIVE_ARGUMENT;
    }

    if (exponent->size == 0)
    {
        return bigint_set_uint64(result, 1);
    }

    if (base->size == 0)
    {
        result->size = 0;
        result->is_negative = false;
        return BIGINT_OK;
    }

    bool result_is_negative = base->is_negative && bigint_get_bit(exponent, 0) != 0;

    BigInt accumulator;
    accumulator.limbs = NULL;
    accumulator.size = 0;
    accumulator.capacity = 0;
    accumulator.is_negative = false;

    BigInt base_power;
    base_power.limbs = NULL;
    base_power.size = 0;
    base_power.capacity = 0;
    base_power.is_negative = false;

    BigIntStatus status = bigint_set_uint64(&accumulator, 1);

    if (status == BIGINT_OK)
    {
        status = bigint_abs(&base_power, base);
    }

    size_t bits = bigint_bit_length(exponent);

    for (size_t i = 0; status == BIGINT_OK && i < bits; i++)
    {
        if (bigint_get_bit(exponent, i))
        {
            status = bigint_mul(&accumulator, &accumulator, &base_power);
        }

        if (status == BIGINT_OK && i + 1 < bits)
        {
            status = bigint_mul(&base_power, &base_power, &base_power);
        }
    }

    if (status == BIGINT_OK)
    {
        status = bigint_copy(result, &accumulator);
    }

    if (status == BIGINT_OK)
    {
        result->is_negative = result_is_negative;
        bigint_normalize(result);
    }

    free(accumulator.limbs);
    free(base_power.limbs);

    return status;
}
