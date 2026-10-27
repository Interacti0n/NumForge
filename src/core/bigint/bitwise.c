#include "src/core/bigint/bigint_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <numforge/bigint.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


typedef enum BigIntBitwiseOperation {
    BIGINT_BITWISE_AND, BIGINT_BITWISE_OR, BIGINT_BITWISE_XOR
} BigIntBitwiseOperation;

static BigIntStatus bigint_bitwise(BigInt *result, const BigInt *a,
    const BigInt *b, BigIntBitwiseOperation operation)
{
    if (result == NULL || a == NULL || b == NULL) return BIGINT_NULL_ARGUMENT;
    if (a->is_negative || b->is_negative) return BIGINT_NEGATIVE_ARGUMENT;
    size_t smaller = a->size < b->size ? a->size : b->size;
    size_t larger = a->size > b->size ? a->size : b->size;
    size_t size = operation == BIGINT_BITWISE_AND ? smaller : larger;
    uint64_t *limbs = NULL;
    if (size > 0) {
        size_t bytes;
        if (bigint_size_mul(size, sizeof(*limbs), &bytes) != BIGINT_OK)
            return BIGINT_OUT_OF_MEMORY;
        limbs = numforge_malloc(bytes);
        if (limbs == NULL) return BIGINT_OUT_OF_MEMORY;

        /* Select once; keep the limb loops free of operation dispatch. */
        switch (operation) {
        case BIGINT_BITWISE_AND:
            for (size_t i = 0; i < size; ++i) limbs[i] = a->limbs[i] & b->limbs[i];
            break;
        case BIGINT_BITWISE_OR:
            for (size_t i = 0; i < size; ++i)
                limbs[i] = (i < a->size ? a->limbs[i] : 0) | (i < b->size ? b->limbs[i] : 0);
            break;
        case BIGINT_BITWISE_XOR:
            for (size_t i = 0; i < size; ++i)
                limbs[i] = (i < a->size ? a->limbs[i] : 0) ^ (i < b->size ? b->limbs[i] : 0);
            break;
        }
    }
    BigInt temporary = {limbs, size, size, false};
    bigint_normalize(&temporary);
    bigint_commit(result, &temporary);
    return BIGINT_OK;
}

BigIntStatus bigint_and(BigInt *result, const BigInt *a, const BigInt *b)
{
    return bigint_bitwise(result, a, b, BIGINT_BITWISE_AND);
}

BigIntStatus bigint_or(BigInt *result, const BigInt *a, const BigInt *b)
{
    return bigint_bitwise(result, a, b, BIGINT_BITWISE_OR);
}

BigIntStatus bigint_xor(BigInt *result, const BigInt *a, const BigInt *b)
{
    return bigint_bitwise(result, a, b, BIGINT_BITWISE_XOR);
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

    BigInt sum = {0};

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

    BigInt temporary = {limbs, new_size, new_size, a->is_negative};
    bigint_normalize(&temporary);
    bigint_commit(result, &temporary);

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

    BigInt temporary = {limbs, new_size, new_size, a->is_negative};
    bigint_normalize(&temporary);
    bigint_commit(result, &temporary);

    return BIGINT_OK;
}


