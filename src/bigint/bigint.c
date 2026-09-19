#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/bigint.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

#define BIGINT_LIMB_BITS 64U
#define BIGINT_MAX_LIMBS (SIZE_MAX / BIGINT_LIMB_BITS)

/*
------------------------------------------------------------------------------------------------------------------------------
    Storage note: limbs are base-2^64, little-endian (limbs[0] is the least
    significant 64 bits).

    Canonical form: every BigInt this file hands back to a caller has no
    trailing (most significant) zero limbs, and is never "negative zero"
    (size == 0 implies is_negative == false). bigint_normalize() is the
    single place that enforces this.

    Aliasing: see the header for the guarantee. The general technique used
    to deliver it is "compute into a fresh buffer or a fresh local BigInt,
    then commit into `result` only once every read from the input
    operands is done" - this is what bigint_mul, the bitwise ops, the
    shifts, and bigint_div_mod all do. It's provably safe: nothing writes
    into memory that could still be read afterward.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for BigInt operations.
------------------------------------------------------------------------------------------------------------------------------
*/

static uint64_t bigint_divide_128_by_u64( /*Divide a 128-bit value by a uint64_t*/
    uint64_t high,
    uint64_t low,
    uint64_t divisor,
    uint64_t *remainder
)
{
    // Preconditions this bit-serial algorithm actually depends on:
    //  - divisor must be non-zero.
    //  - high must be strictly less than divisor. This function computes
    //    floor((high*2^64+low)/divisor); that value only fits in a
    //    uint64_t quotient when high < divisor. Violate this and the loop
    //    below still runs and still returns *something*, but it's a
    //    silently wrong, wrapped result rather than the real quotient.
    // bigint_divide_by_uint64 satisfies this by construction: it seeds
    // `high` with 0 and thereafter uses
    // with the previous step's remainder, which is always < divisor.
    assert(divisor != 0);
    assert(high < divisor);

#if defined(__SIZEOF_INT128__)
    {
        __uint128_t dividend = ((__uint128_t)high << 64) | (__uint128_t)low;

        *remainder = (uint64_t)(dividend % divisor);
        return (uint64_t)(dividend / divisor);
    }
#elif defined(_MSC_VER) && defined(_M_X64)
    return _udiv128(high, low, divisor, remainder);
#else

    uint64_t quotient = 0;
    uint64_t current_remainder = high;

    for (int bit = 63; bit >= 0; --bit)
    {
        uint64_t input_bit = (low >> bit) & 1ULL;

        uint64_t divisor_minus_remainder =
            divisor - current_remainder;

        if (current_remainder >= divisor_minus_remainder ||
            (input_bit &&
             current_remainder == divisor_minus_remainder - 1))
        {
            current_remainder =
                current_remainder - divisor_minus_remainder + input_bit;

            quotient = (quotient << 1) | 1ULL;
        }
        else
        {
            current_remainder =
                (current_remainder << 1) | input_bit;

            quotient <<= 1;
        }
    }

    *remainder = current_remainder;

    return quotient;
#endif
}

BigIntStatus bigint_size_add( /*Overflow-checked size_t addition: out = a+b*/
    size_t a,
    size_t b,
    size_t *out
)
{
    if (a > SIZE_MAX - b)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    *out = a + b;

    return BIGINT_OK;
}

BigIntStatus bigint_size_mul( /*Overflow-checked size_t multiplication: out = a*b*/
    size_t a,
    size_t b,
    size_t *out
)
{
    if (a != 0 && b > SIZE_MAX / a)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    *out = a * b;

    return BIGINT_OK;
}

uint64_t bigint_divide_by_uint64( /*Divide a BigInt by a uint64_t in place, returning the remainder*/
    BigInt *value,
    uint64_t divisor
)
{
    if (value == NULL || divisor == 0)
    {
        return 0;
    }

    uint64_t remainder = 0;

    for (size_t i = value->size; i > 0; i--)
    {
        size_t index = i - 1;

        uint64_t quotient = bigint_divide_128_by_u64(
            remainder,
            value->limbs[index],
            divisor,
            &remainder);

        value->limbs[index] = quotient;
    }

    while (value->size > 0 &&
           value->limbs[value->size - 1] == 0)
    {
        value->size--;
    }

    return remainder;
}

BigIntStatus bigint_strip_decimal_zeros( /*Remove trailing decimal zeros from a BigInt, keeps the number of decimal zeros in *removed*/
    BigInt *value,
    uint64_t *removed
)
{
    const uint64_t block = UINT64_C(10000000000000000000);
    unsigned int remainder10;
    *removed = 0U;
    if (value->size == 0U) return BIGINT_OK;
    /* 2^(64*i) mod 10 is 6 for every i >= 1. Reject the common
     * already-normalized case without division or temporary allocation. */
    remainder10 = (unsigned int)(value->limbs[0] % 10U);
    for (size_t i = 1U; i < value->size; i++)
        remainder10 = (remainder10 + 6U * (unsigned int)(value->limbs[i] % 10U)) % 10U;
    if (remainder10 != 0U) return BIGINT_OK;

    for (;;)
    {
        uint64_t remainder = 0U;
        uint64_t divisor = 1U;
        unsigned int count = 0U;
        if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
        for (size_t i = value->size; i > 0U; i--)
            (void)bigint_divide_128_by_u64(remainder, value->limbs[i - 1U], block, &remainder);
        if (remainder == 0U)
        {
            count = 19U;
            divisor = block;
        }
        else
        {
            while (remainder % 10U == 0U)
            {
                count++;
                divisor *= 10U;
                remainder /= 10U;
            }
            if (count == 0U) return BIGINT_OK;
        }
        if (*removed > UINT64_MAX - count) return BIGINT_VALUE_TOO_LARGE;
        (void)bigint_divide_by_uint64(value, divisor);
        *removed += count;
        if (remainder != 0U) return BIGINT_OK;
    }
}

void bigint_normalize( /*Trim trailing zero limbs and clear the sign on zero - the single source of truth for canonical form*/
    BigInt *value
)
{
    if (value == NULL)
    {
        return;
    }

    while (value->size > 0 && value->limbs[value->size - 1] == 0)
    {
        value->size--;
    }

    if (value->size == 0)
    {
        value->is_negative = false;
    }
}

void bigint_commit( /*Move a temporary BigInt into a destination, leaving the temporary empty and safe to destroy*/
    BigInt *destination,
    BigInt *temporary
)
{
    free(destination->limbs);
    destination->limbs = temporary->limbs;
    destination->size = temporary->size;
    destination->capacity = temporary->capacity;
    destination->is_negative = temporary->is_negative;

    temporary->limbs = NULL;
    temporary->size = 0U;
    temporary->capacity = 0U;
    temporary->is_negative = false;
}

int bigint_wrap_uint64( /*Wrap a single uint64_t in a read-only, stack-backed BigInt (no allocation)*/
    BigInt *wrapped,
    uint64_t *storage,
    uint64_t value
)
{
    *storage = value;
    wrapped->limbs = storage;
    wrapped->size = (value != 0) ? 1 : 0;
    wrapped->capacity = 1;
    wrapped->is_negative = false;

    // Only ever safe to pass as a read-only `a`/`b` argument - never as a
    // `result`/`destination`, since nothing in this file may call
    // realloc() or free() on it, and every arithmetic function here only
    // ever does that to `result`, never to its input operands.
    return 1;
}

static BigIntStatus bigint_ensure_capacity( /*Grow a BigInt's limb buffer to hold at least `needed` limbs, with overflow checks at every step*/
    BigInt *value,
    size_t needed
)
{
    if (value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }
    if (needed > BIGINT_MAX_LIMBS)
    {
        return BIGINT_VALUE_TOO_LARGE;
    }

    if (value->capacity >= needed)
    {
        return BIGINT_OK;
    }

    size_t new_capacity = (value->capacity == 0) ? 1 : value->capacity;

    while (new_capacity < needed)
    {
        size_t doubled;

        if (bigint_size_mul(new_capacity, 2, &doubled) != BIGINT_OK ||
            doubled > BIGINT_MAX_LIMBS)
        {
            // Doubling would overflow size_t; jump straight to exactly
            // what's needed instead of looping forever.
            new_capacity = needed;
            break;
        }

        new_capacity = doubled;
    }

    size_t new_capacity_bytes;

    if (bigint_size_mul(new_capacity, sizeof(uint64_t), &new_capacity_bytes) != BIGINT_OK)
    {
        return BIGINT_OUT_OF_MEMORY; // the requested limb count can't be expressed as a byte size on this platform
    }

    uint64_t *new_limbs = numforge_realloc(value->limbs, new_capacity_bytes);

    if (new_limbs == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    value->limbs = new_limbs;
    value->capacity = new_capacity;

    return BIGINT_OK;
}

BigIntStatus bigint_add_uint64( /*Add a uint64_t to a BigInt (magnitude only - caller manages sign)*/
    BigInt *value,
    uint64_t amount
)
{
    if (value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (amount == 0)
    {
        return BIGINT_OK;
    }

    if (value->size == 0)
    {
        BigIntStatus status = bigint_ensure_capacity(value, 1);

        if (status != BIGINT_OK)
        {
            return status;
        }

        value->limbs[0] = amount;
        value->size = 1;

        return BIGINT_OK;
    }

    uint64_t old = value->limbs[0];

    value->limbs[0] += amount;

    if (value->limbs[0] >= old)
    {
        return BIGINT_OK;
    }

    size_t index = 1;

    while (index < value->size)
    {
        old = value->limbs[index];

        value->limbs[index] += 1;

        if (value->limbs[index] >= old)
        {
            return BIGINT_OK;
        }

        index++;
    }

    size_t needed;
    BigIntStatus status = bigint_size_add(value->size, 1, &needed);

    if (status == BIGINT_OK)
    {
        status = bigint_ensure_capacity(value, needed);
    }

    if (status != BIGINT_OK)
    {
        return status;
    }

    value->limbs[value->size] = 1;
    value->size++;

    return BIGINT_OK;
}

void bigint_multiply_u64_u64( /*Multiply two uint64_t values and return the high and low parts of the result*/
    uint64_t a,
    uint64_t b,
    uint64_t *high,
    uint64_t *low
)
{
    uint64_t a0 = (uint32_t)a;
    uint64_t a1 = a >> 32;

    uint64_t b0 = (uint32_t)b;
    uint64_t b1 = b >> 32;

    uint64_t p00 = a0 * b0;
    uint64_t p01 = a0 * b1;
    uint64_t p10 = a1 * b0;
    uint64_t p11 = a1 * b1;

    uint64_t middle =
        (p00 >> 32) +
        (uint32_t)p01 +
        (uint32_t)p10;

    *low =
        (p00 & 0xFFFFFFFFULL) |
        (middle << 32);

    *high =
        p11 +
        (p01 >> 32) +
        (p10 >> 32) +
        (middle >> 32);
}

BigIntStatus bigint_multiply_by_uint64( /*Multiply a BigInt by a uint64_t (magnitude only - caller manages sign)*/
    BigInt *value,
    uint64_t multiplier
)
{
    if (value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (value->size == 0)
    {
        return BIGINT_OK;
    }

    if (multiplier == 0)
    {
        value->size = 0;
        bigint_normalize(value);
        return BIGINT_OK;
    }

    uint64_t carry = 0;

    for (size_t i = 0; i < value->size; i++)
    {
        uint64_t high;
        uint64_t low;

        bigint_multiply_u64_u64(
            value->limbs[i],
            multiplier,
            &high,
            &low
        );

        uint64_t old_low = low;

        low += carry;

        if (low < old_low)
        {
            high++;
        }

        value->limbs[i] = low;
        carry = high;
    }

    if (carry != 0)
    {
        size_t needed;
        BigIntStatus status = bigint_size_add(value->size, 1, &needed);

        if (status == BIGINT_OK)
        {
            status = bigint_ensure_capacity(value, needed);
        }

        if (status != BIGINT_OK)
        {
            return status;
        }

        value->limbs[value->size] = carry;
        value->size++;
    }

    return BIGINT_OK;
}

BigIntStatus bigint_add_abs( /*Add the absolute values of two BigInts. Safe for result aliasing a and/or b.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    size_t max_size =
        (a->size > b->size) ? a->size : b->size;

    size_t needed;
    BigIntStatus status = bigint_size_add(max_size, 1, &needed);

    if (status == BIGINT_OK)
    {
        status = bigint_ensure_capacity(result, needed);
    }

    if (status != BIGINT_OK)
    {
        return status;
    }

    // Aliasing note: for each index i, both operand limbs are read before
    // result->limbs[i] is written, and no later index depends on an
    // earlier one except through the scalar `carry` - so this is correct
    // even when result aliases a and/or b.
    uint64_t carry = 0;

    for (size_t i = 0; i < max_size; i++)
    {
        uint64_t limb_a =
            (i < a->size) ? a->limbs[i] : 0;

        uint64_t limb_b =
            (i < b->size) ? b->limbs[i] : 0;

        uint64_t sum = limb_a + limb_b;
        uint64_t new_carry = (sum < limb_a);

        uint64_t final_sum = sum + carry;

        if (final_sum < sum)
        {
            new_carry = 1;
        }

        carry = new_carry;
        result->limbs[i] = final_sum;
    }

    result->size = max_size;

    if (carry)
    {
        result->limbs[result->size++] = carry;
    }

    // add_abs on two already-canonical inputs can never produce a trailing
    // zero limb (the top limb of the longer operand is nonzero by
    // assumption, and carries only ever grow the result), so no
    // bigint_normalize() call is needed here.

    return BIGINT_OK;
}

BigIntStatus bigint_subtract_abs( /*Subtract the absolute values of two BigInts (|a|-|b|, assumes |a|>=|b|). Safe for result aliasing a and/or b.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (bigint_compare_abs(a, b) < 0)
    {
        return bigint_subtract_abs(result,b,a);
    }

    BigIntStatus status = bigint_ensure_capacity(result, a->size);

    if (status != BIGINT_OK)
    {
        return status;
    }

    // Same aliasing argument as bigint_add_abs: each index's operand limbs
    // are read before result->limbs[i] is written.
    uint64_t borrow = 0;

    for (size_t i = 0; i < a->size; i++)
    {
        uint64_t limb_a = a->limbs[i];
        uint64_t limb_b = (i < b->size) ? b->limbs[i] : 0;

        uint64_t temp = limb_a - limb_b - borrow;

        if (limb_a < limb_b || (borrow && limb_a == limb_b))
        {
            borrow = 1;
        }
        else
        {
            borrow = 0;
        }

        result->limbs[i] = temp;
    }

    result->size = a->size;

    bigint_normalize(result);

    return BIGINT_OK;
}

BigIntStatus bigint_set_uint64( /*Set a BigInt to a small non-negative value, growing capacity as needed*/
    BigInt *value,
    uint64_t amount
)
{
    if (value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (amount == 0)
    {
        value->size = 0;
        value->is_negative = false;
        return BIGINT_OK;
    }

    BigIntStatus status = bigint_ensure_capacity(value, 1);

    if (status != BIGINT_OK)
    {
        return status;
    }

    value->limbs[0] = amount;
    value->size = 1;
    value->is_negative = false;

    return BIGINT_OK;
}

size_t bigint_bit_length( /*Number of bits needed to represent |value| (0 for zero)*/
    const BigInt *value
)
{
    if (value == NULL || value->size == 0)
    {
        return 0;
    }

    uint64_t top = value->limbs[value->size - 1];
    size_t bits = (value->size - 1U) * BIGINT_LIMB_BITS;

    while (top != 0)
    {
        bits++;
        top >>= 1;
    }

    return bits;
}

int bigint_get_bit( /*Read a single bit (0 or 1) of |value|*/
    const BigInt *value,
    size_t bit_index
)
{
    if (value == NULL)
    {
        return 0;
    }

    size_t limb_index = bit_index / BIGINT_LIMB_BITS;

    if (limb_index >= value->size)
    {
        return 0;
    }

    size_t bit_offset = bit_index % BIGINT_LIMB_BITS;

    return (int)((value->limbs[limb_index] >> bit_offset) & 1ULL);
}

BigIntStatus bigint_set_bit( /*Set a single bit of |value|, growing the BigInt as needed*/
    BigInt *value,
    size_t bit_index
)
{
    size_t limb_index = bit_index / BIGINT_LIMB_BITS;
    size_t bit_offset = bit_index % BIGINT_LIMB_BITS;

    size_t needed;
    BigIntStatus status = bigint_size_add(limb_index, 1, &needed);

    if (status == BIGINT_OK)
    {
        status = bigint_ensure_capacity(value, needed);
    }

    if (status != BIGINT_OK)
    {
        return status;
    }
    if (value->limbs == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    while (value->size <= limb_index)
    {
        value->limbs[value->size] = 0;
        value->size++;
    }

    value->limbs[limb_index] |= (1ULL << bit_offset);

    return BIGINT_OK;
}

static BigIntStatus bigint_shift_left_one_bit( /*Multiply |value| by 2 in place*/
    BigInt *value
)
{
    if (value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (value->size == 0)
    {
        return BIGINT_OK;
    }

    uint64_t overflow = value->limbs[value->size - 1] >> 63;

    if (overflow)
    {
        size_t needed;
        BigIntStatus status = bigint_size_add(value->size, 1, &needed);

        if (status == BIGINT_OK)
        {
            status = bigint_ensure_capacity(value, needed);
        }

        if (status != BIGINT_OK)
        {
            return status;
        }
    }

    for (size_t i = value->size; i > 1; i--)
    {
        value->limbs[i - 1] =
            (value->limbs[i - 1] << 1) | (value->limbs[i - 2] >> 63);
    }

    value->limbs[0] <<= 1;

    if (overflow)
    {
        value->limbs[value->size] = overflow;
        value->size++;
    }

    return BIGINT_OK;
}

BigIntStatus bigint_divmod_abs( /*Long division on magnitudes only: quotient = |a|/|b|, remainder = |a|%|b|.
                                          Always called with a fresh, non-aliased quotient/remainder pair - see bigint_div_mod.*/
    BigInt *quotient,
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
)
{
    quotient->size = 0;
    remainder->size = 0;

    size_t bits = bigint_bit_length(a);

    if (bits == 0)
    {
        return BIGINT_OK;
    }

    BigIntStatus status = bigint_ensure_capacity(quotient, a->size);

    if (status != BIGINT_OK)
    {
        return status;
    }

    for (size_t i = bits; i > 0; i--)
    {
        if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
        size_t bit_index = i - 1;

        status = bigint_shift_left_one_bit(remainder);

        if (status != BIGINT_OK)
        {
            return status;
        }

        status = bigint_add_uint64(remainder, (uint64_t)bigint_get_bit(a, bit_index));

        if (status != BIGINT_OK)
        {
            return status;
        }

        if (bigint_compare_abs(remainder, b) >= 0)
        {
            status = bigint_subtract_abs(remainder, remainder, b);

            if (status != BIGINT_OK)
            {
                return status;
            }

            status = bigint_set_bit(quotient, bit_index);

            if (status != BIGINT_OK)
            {
                return status;
            }
        }
    }

    bigint_normalize(quotient);

    return BIGINT_OK;
}

int bigint_compare_abs( /*Compare two absolute values of BigInts*/
    const BigInt *a,
    const BigInt *b
)
{
    assert(a != NULL);
    assert(b != NULL);

    if (a->size < b->size)
    {
        return -1;
    }
    else if (a->size > b->size)
    {
        return 1;
    }

    for (size_t i = a->size; i > 0; i--)
    {
        size_t index = i - 1;

        if (a->limbs[index] < b->limbs[index])
        {
            return -1;
        }
        else if (a->limbs[index] > b->limbs[index])
        {
            return 1;
        }
    }

    return 0;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Status and lifecycle functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

const char *bigint_status_to_string( /*Human-readable description of a BigIntStatus*/
    BigIntStatus status
)
{
    switch (status)
    {
        case BIGINT_OK:
            return "success";
        case BIGINT_NULL_ARGUMENT:
            return "a required argument was NULL";
        case BIGINT_OUT_OF_MEMORY:
            return "out of memory";
        case BIGINT_DIVISION_BY_ZERO:
            return "division by zero";
        case BIGINT_INVALID_ARGUMENT:
            return "invalid argument";
        case BIGINT_NEGATIVE_ARGUMENT:
            return "this operation requires a non-negative argument";
        case BIGINT_VALUE_TOO_LARGE:
            return "value is too large for this operation";
        default:
            return "unknown BigIntStatus";
    }
}

BigInt *bigint_create( /*Create a new BigInt*/
    void
)
{
    BigInt *value = numforge_malloc(sizeof(BigInt));

    if (value == NULL)
    {
        return NULL;
    }

    value->limbs = NULL;
    value->size = 0;
    value->capacity = 0;
    value->is_negative = false;

    return value;
}

void bigint_destroy( /*Free the memory allocated for a BigInt*/
    BigInt *value
)
{
    if (value == NULL)
    {
        return;
    }

    free(value->limbs);
    free(value);
}

BigIntStatus bigint_copy( /*Create a copy of a BigInt*/
    BigInt *destination,
    const BigInt *source
)
{
    if (destination == NULL || source == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (destination == source)
    {
        return BIGINT_OK;
    }

    if (source->size == 0)
    {
        destination->size = 0;
        destination->is_negative = false;
        return BIGINT_OK;
    }

    BigIntStatus status = bigint_ensure_capacity(destination, source->size);

    if (status != BIGINT_OK)
    {
        return status;
    }

    memcpy(
        destination->limbs,
        source->limbs,
        source->size * sizeof(uint64_t)
    );

    destination->size = source->size;
    destination->is_negative = source->is_negative;

    return BIGINT_OK;
}
