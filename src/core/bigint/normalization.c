#include "src/core/bigint/bigint_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>

/* Trial exact division by 10^block = 2^block * 5^block. After the binary
 * shift, each quotient limb is fixed by the inverse of the odd divisor
 * modulo 2^64. A zero final carry proves divisibility; rejected candidates
 * never replace the input. No hardware integer division is needed. */
BigIntStatus bigint_strip_decimal_zeros_inverse(BigInt *value, uint64_t *removed, unsigned block)
{
    if (!value || !removed) return BIGINT_NULL_ARGUMENT;
    if (block != 19U && block != 27U) return BIGINT_INVALID_ARGUMENT;
    *removed = 0U;
    if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
    uint64_t mask = (UINT64_C(1) << block) - 1U;
    if (!value->size || (value->limbs[0] & mask))
        return bigint_strip_decimal_zeros_classic(value, removed);
    /* The binary divisibility test alone also admits powers of two. Since
     * 2^64 == 1 (mod 5), reject coefficients without a factor of five before
     * requesting scratch. Such values have no decimal zeros to remove. */
    unsigned remainder5 = 0U;
    for (size_t i = 0U; i < value->size; ++i)
    {
        if (!(i & 255U) && !numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
        remainder5 = (remainder5 + (unsigned)(value->limbs[i] % 5U)) % 5U;
    }
    if (remainder5) return BIGINT_OK;
    size_t bytes;
    if (bigint_size_mul(value->size, sizeof(uint64_t), &bytes) != BIGINT_OK)
        return BIGINT_OUT_OF_MEMORY;
    if (!numforge_budget_can_allocate_pair(bytes, 0U))
        return bigint_strip_decimal_zeros_classic(value, removed);
    BigInt trial = {0};
    trial.capacity = value->size;
    trial.limbs = numforge_malloc(bytes);
    if (!trial.limbs) return BIGINT_OUT_OF_MEMORY;
    uint64_t divisor = 1U, inverse = 1U;
    for (unsigned i = 0U; i < block; ++i) divisor *= 5U;
    for (unsigned i = 0U; i < 6U; ++i) inverse *= 2U - divisor * inverse;
    BigIntStatus status = BIGINT_OK;
    while (value->size && !(value->limbs[0] & mask))
    {
        uint64_t carry = 0U;
        for (size_t i = 0U; i < value->size; ++i)
        {
            if (!(i & 255U) && !numforge_budget_check())
            { status = BIGINT_OUT_OF_MEMORY; break; }
            uint64_t word = value->limbs[i] >> block;
            if (i + 1U < value->size) word |= value->limbs[i + 1U] << (64U - block);
            uint64_t quotient = (word - carry) * inverse, high, low;
            bigint_multiply_u64_u64(quotient, divisor, &high, &low);
            (void)low;
            trial.limbs[i] = quotient;
            carry = high + (word < carry);
        }
        if (status != BIGINT_OK || carry) break;
        if (*removed > UINT64_MAX - block) { status = BIGINT_VALUE_TOO_LARGE; break; }
        trial.size = value->size;
        trial.is_negative = value->is_negative;
        bigint_normalize(&trial);
        BigInt previous = *value; *value = trial; trial = previous;
        *removed += block;
    }
    free(trial.limbs);
    if (status == BIGINT_OK)
    {
        uint64_t tail = 0U;
        status = bigint_strip_decimal_zeros_classic(value, &tail);
        if (status == BIGINT_OK && *removed > UINT64_MAX - tail) return BIGINT_VALUE_TOO_LARGE;
        *removed += tail;
    }
    return status;
}
