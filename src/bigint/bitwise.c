#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/bigint.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define BIGINT_LIMB_BITS 64U
#define BIGINT_MAX_LIMBS (SIZE_MAX / BIGINT_LIMB_BITS)

/*
------------------------------------------------------------------------------------------------------------------------------
    Bitwise, shift, and sign utility functions for BigInt.

    The public operations are kept in this translation unit so the core
    lifecycle and arithmetic implementation remains easy to navigate.
------------------------------------------------------------------------------------------------------------------------------
*/


/*
------------------------------------------------------------------------------------------------------------------------------
    Bitwise operation functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

BigIntStatus bigint_and( /*Bitwise AND for BigInts (a&b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (a->is_negative || b->is_negative)
    {
        return BIGINT_NEGATIVE_ARGUMENT;
    }

    size_t min_size = (a->size < b->size) ? a->size : b->size;

    uint64_t *limbs = NULL;

    if (min_size > 0)
    {
        size_t bytes;

        if (bigint_size_mul(min_size, sizeof(uint64_t), &bytes) != BIGINT_OK)
        {
            return BIGINT_OUT_OF_MEMORY;
        }

        limbs = numforge_malloc(bytes);

        if (limbs == NULL)
        {
            return BIGINT_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < min_size; i++)
        {
            limbs[i] = a->limbs[i] & b->limbs[i];
        }
    }

    free(result->limbs);
    result->limbs = limbs;
    result->capacity = min_size;
    result->size = min_size;
    result->is_negative = false;

    bigint_normalize(result);

    return BIGINT_OK;
}

BigIntStatus bigint_or( /*Bitwise OR for BigInts (a|b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (a->is_negative || b->is_negative)
    {
        return BIGINT_NEGATIVE_ARGUMENT;
    }

    size_t max_size = (a->size > b->size) ? a->size : b->size;

    uint64_t *limbs = NULL;

    if (max_size > 0)
    {
        size_t bytes;

        if (bigint_size_mul(max_size, sizeof(uint64_t), &bytes) != BIGINT_OK)
        {
            return BIGINT_OUT_OF_MEMORY;
        }

        limbs = numforge_malloc(bytes);

        if (limbs == NULL)
        {
            return BIGINT_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < max_size; i++)
        {
            uint64_t limb_a = (i < a->size) ? a->limbs[i] : 0;
            uint64_t limb_b = (i < b->size) ? b->limbs[i] : 0;

            limbs[i] = limb_a | limb_b;
        }
    }

    free(result->limbs);
    result->limbs = limbs;
    result->capacity = max_size;
    result->size = max_size;
    result->is_negative = false;

    bigint_normalize(result);

    return BIGINT_OK;
}

BigIntStatus bigint_xor( /*Bitwise XOR for BigInts (a^b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (a->is_negative || b->is_negative)
    {
        return BIGINT_NEGATIVE_ARGUMENT;
    }

    size_t max_size = (a->size > b->size) ? a->size : b->size;

    uint64_t *limbs = NULL;

    if (max_size > 0)
    {
        size_t bytes;

        if (bigint_size_mul(max_size, sizeof(uint64_t), &bytes) != BIGINT_OK)
        {
            return BIGINT_OUT_OF_MEMORY;
        }

        limbs = numforge_malloc(bytes);

        if (limbs == NULL)
        {
            return BIGINT_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < max_size; i++)
        {
            uint64_t limb_a = (i < a->size) ? a->limbs[i] : 0;
            uint64_t limb_b = (i < b->size) ? b->limbs[i] : 0;

            limbs[i] = limb_a ^ limb_b;
        }
    }

    free(result->limbs);
    result->limbs = limbs;
    result->capacity = max_size;
    result->size = max_size;
    result->is_negative = false;

    bigint_normalize(result);

    return BIGINT_OK;
}

BigIntStatus bigint_not( /*Bitwise NOT for BigInts (~a), defined arbitrary-precision as -(a+1)*/
    BigInt *result,
    const BigInt *a
)
{
    if (result == NULL || a == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    uint64_t one_storage;
    BigInt one;
    bigint_wrap_uint64(&one, &one_storage, 1);

    BigInt sum;
    sum.limbs = NULL;
    sum.size = 0;
    sum.capacity = 0;
    sum.is_negative = false;

    BigIntStatus status = bigint_add(&sum, a, &one);

    if (status == BIGINT_OK)
    {
        status = bigint_negate(result, &sum);
    }

    free(sum.limbs);

    return status;
}

BigIntStatus bigint_shift_left( /*Left shift for BigInts (a<<n)*/
    BigInt *result,
    const BigInt *a,
    size_t n
)
{
    if (result == NULL || a == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (a->size == 0 || n == 0)
    {
        return bigint_copy(result, a);
    }

    size_t limb_shift = n / BIGINT_LIMB_BITS;
    size_t bit_shift = n % BIGINT_LIMB_BITS;

    size_t new_size;
    size_t new_size_bytes;

    if (bigint_size_add(a->size, limb_shift, &new_size) != BIGINT_OK ||
        bigint_size_add(new_size, 1, &new_size) != BIGINT_OK ||
        new_size > BIGINT_MAX_LIMBS ||
        bigint_size_mul(new_size, sizeof(uint64_t), &new_size_bytes) != BIGINT_OK)
    {
        return BIGINT_VALUE_TOO_LARGE;
    }

    uint64_t *limbs = numforge_calloc(new_size, sizeof(uint64_t));

    if (limbs == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < a->size; i++)
    {
        uint64_t part = a->limbs[i];

        if (bit_shift == 0)
        {
            limbs[i + limb_shift] |= part;
        }
        else
        {
            limbs[i + limb_shift] |= (part << bit_shift);
            limbs[i + limb_shift + 1] |= (part >> (BIGINT_LIMB_BITS - bit_shift));
        }
    }

    free(result->limbs);
    result->limbs = limbs;
    result->capacity = new_size;
    result->size = new_size;
    result->is_negative = a->is_negative;

    bigint_normalize(result);

    return BIGINT_OK;
}

BigIntStatus bigint_shift_right( /*Right shift for BigInts (a>>n), truncating toward zero*/
    BigInt *result,
    const BigInt *a,
    size_t n
)
{
    if (result == NULL || a == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (a->size == 0 || n == 0)
    {
        return bigint_copy(result, a);
    }

    size_t limb_shift = n / BIGINT_LIMB_BITS;

    if (limb_shift >= a->size)
    {
        result->size = 0;
        result->is_negative = false;
        return BIGINT_OK;
    }

    size_t bit_shift = n % BIGINT_LIMB_BITS;
    size_t new_size = a->size - limb_shift; // limb_shift < a->size, just checked above, so this can't underflow

    size_t new_size_bytes;

    if (bigint_size_mul(new_size, sizeof(uint64_t), &new_size_bytes) != BIGINT_OK)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    uint64_t *limbs = numforge_malloc(new_size_bytes);

    if (limbs == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < new_size; i++)
    {
        uint64_t merged = a->limbs[i + limb_shift];

        if (bit_shift != 0)
        {
            merged >>= bit_shift;

            if (i + limb_shift + 1 < a->size)
            {
                merged |= (a->limbs[i + limb_shift + 1] << (BIGINT_LIMB_BITS - bit_shift));
            }
        }

        limbs[i] = merged;
    }

    free(result->limbs);
    result->limbs = limbs;
    result->capacity = new_size;
    result->size = new_size;
    result->is_negative = a->is_negative;

    bigint_normalize(result);

    return BIGINT_OK;
}


