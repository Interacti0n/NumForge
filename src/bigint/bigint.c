#include "bigint_internal.h"
#include <numforge/bigint.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
static int bigint_compare_abs(
    const BigInt *a,
    const BigInt *b
);

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
}

static BigIntStatus bigint_size_add( /*Overflow-checked size_t addition: out = a+b*/
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

static BigIntStatus bigint_size_mul( /*Overflow-checked size_t multiplication: out = a*b*/
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

static uint64_t bigint_divide_by_uint64( /*Divide a BigInt by a uint64_t in place, returning the remainder*/
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

static void bigint_normalize( /*Trim trailing zero limbs and clear the sign on zero - the single source of truth for canonical form*/
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

static int bigint_wrap_uint64( /*Wrap a single uint64_t in a read-only, stack-backed BigInt (no allocation)*/
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

    if (value->capacity >= needed)
    {
        return BIGINT_OK;
    }

    size_t new_capacity = (value->capacity == 0) ? 1 : value->capacity;

    while (new_capacity < needed)
    {
        size_t doubled;

        if (bigint_size_mul(new_capacity, 2, &doubled) != BIGINT_OK)
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

    uint64_t *new_limbs = realloc(value->limbs, new_capacity_bytes);

    if (new_limbs == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    value->limbs = new_limbs;
    value->capacity = new_capacity;

    return BIGINT_OK;
}

static BigIntStatus bigint_add_uint64( /*Add a uint64_t to a BigInt (magnitude only - caller manages sign)*/
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

static void bigint_multiply_u64_u64( /*Multiply two uint64_t values and return the high and low parts of the result*/
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

static BigIntStatus bigint_multiply_by_uint64( /*Multiply a BigInt by a uint64_t (magnitude only - caller manages sign)*/
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

static BigIntStatus bigint_add_abs( /*Add the absolute values of two BigInts. Safe for result aliasing a and/or b.*/
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

static BigIntStatus bigint_subtract_abs( /*Subtract the absolute values of two BigInts (|a|-|b|, assumes |a|>=|b|). Safe for result aliasing a and/or b.*/
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

static BigIntStatus bigint_set_uint64( /*Set a BigInt to a small non-negative value, growing capacity as needed*/
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

static size_t bigint_bit_length( /*Number of bits needed to represent |value| (0 for zero)*/
    const BigInt *value
)
{
    if (value == NULL || value->size == 0)
    {
        return 0;
    }

    uint64_t top = value->limbs[value->size - 1];
    size_t bits = (value->size - 1) * 64;

    while (top != 0)
    {
        bits++;
        top >>= 1;
    }

    return bits;
}

static int bigint_get_bit( /*Read a single bit (0 or 1) of |value|*/
    const BigInt *value,
    size_t bit_index
)
{
    if (value == NULL)
    {
        return 0;
    }

    size_t limb_index = bit_index / 64;

    if (limb_index >= value->size)
    {
        return 0;
    }

    size_t bit_offset = bit_index % 64;

    return (int)((value->limbs[limb_index] >> bit_offset) & 1ULL);
}

static BigIntStatus bigint_set_bit( /*Set a single bit of |value|, growing the BigInt as needed*/
    BigInt *value,
    size_t bit_index
)
{
    size_t limb_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

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

static BigIntStatus bigint_divmod_abs( /*Long division on magnitudes only: quotient = |a|/|b|, remainder = |a|%|b|.
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

static int bigint_compare_abs( /*Compare two absolute values of BigInts*/
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
    Operation functions for BigInt.
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
    BigInt *value = malloc(sizeof(BigInt));

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

BigIntStatus bigint_set_string( /*Transform string to BigInt*/
    BigInt *value,
    const char *string
)
{
    if (value == NULL || string == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    size_t len = strlen(string);

    if (len == 0)
    {
        return BIGINT_INVALID_ARGUMENT; // "" is not a number
    }

    size_t start = 0;
    bool negative = false;

    if (string[0] == '-' || string[0] == '+')
    {
        negative = (string[0] == '-');
        start = 1;
    }

    if (start >= len)
    {
        return BIGINT_INVALID_ARGUMENT; // a bare "+" or "-" with no digits
    }

    for (size_t i = start; i < len; i++)
    {
        if (string[i] < '0' || string[i] > '9')
        {
            return BIGINT_INVALID_ARGUMENT;
        }
    }

    // Parse into a temporary value. This keeps the caller's value intact if
    // an allocation fails part-way through a long input.
    BigInt parsed;
    parsed.limbs = NULL;
    parsed.size = 0;
    parsed.capacity = 0;
    parsed.is_negative = false;

    // Parse in chunks of up to 19 decimal digits at a time: value = value * 10^chunk_len + chunk_value.
    // 10^19 fits in a uint64_t, so each chunk is one multiply + one add regardless of how many
    // digits the whole number has, instead of one multiply/add per digit.
    size_t i = start;

    while (i < len)
    {
        size_t chunk_len = (len - i > 19) ? 19 : (len - i);
        uint64_t chunk_value = 0;
        uint64_t chunk_multiplier = 1;

        for (size_t j = 0; j < chunk_len; j++)
        {
            chunk_value = chunk_value * 10 + (uint64_t)(string[i + j] - '0');
            chunk_multiplier *= 10;
        }

        BigIntStatus status = bigint_multiply_by_uint64(&parsed, chunk_multiplier);

        if (status == BIGINT_OK)
        {
            status = bigint_add_uint64(&parsed, chunk_value);
        }

        if (status != BIGINT_OK)
        {
            free(parsed.limbs);
            return status;
        }

        i += chunk_len;
    }

    parsed.is_negative = negative;
    bigint_normalize(&parsed);

    free(value->limbs);
    *value = parsed;

    return BIGINT_OK;
}

char *bigint_to_string( /*Transform BigInt to string*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return NULL;
    }

    if (value->size == 0)
    {
        char *string = malloc(2);

        if (string == NULL)
        {
            return NULL;
        }

        string[0] = '0';
        string[1] = '\0';

        return string;
    }

    // Peel off 19-digit decimal chunks (least significant first) by repeatedly
    // dividing a scratch copy by 10^19, then print the chunks back in order.
    BigInt scratch;
    scratch.limbs = NULL;
    scratch.size = 0;
    scratch.capacity = 0;
    scratch.is_negative = false;

    if (bigint_copy(&scratch, value) != BIGINT_OK)
    {
        return NULL;
    }

    // Each division by 10^19 (> 2^63) removes at least 63 bits, so this many
    // chunks is always enough. Every step below is overflow-checked since
    // value->size is attacker/caller controlled in principle.
    size_t bits_estimate;
    size_t rounded;
    size_t chunks_capacity;
    size_t chunks_bytes;

    if (bigint_size_mul(value->size, 64, &bits_estimate) != BIGINT_OK ||
        bigint_size_add(bits_estimate, 62, &rounded) != BIGINT_OK ||
        bigint_size_add(rounded / 63, 1, &chunks_capacity) != BIGINT_OK ||
        bigint_size_mul(chunks_capacity, sizeof(uint64_t), &chunks_bytes) != BIGINT_OK)
    {
        free(scratch.limbs);
        return NULL;
    }

    uint64_t *chunks = malloc(chunks_bytes);

    if (chunks == NULL)
    {
        free(scratch.limbs);
        return NULL;
    }

    size_t chunk_count = 0;

    while (scratch.size > 0)
    {
        chunks[chunk_count++] =
            bigint_divide_by_uint64(&scratch, 10000000000000000000ULL);
    }

    free(scratch.limbs);

    size_t chunk_digits;
    size_t capacity;

    if (bigint_size_mul(chunk_count, 19, &chunk_digits) != BIGINT_OK ||
        bigint_size_add(chunk_digits, 2, &capacity) != BIGINT_OK ||
        (value->is_negative && bigint_size_add(capacity, 1, &capacity) != BIGINT_OK))
    {
        free(chunks);
        return NULL;
    }

    char *string = malloc(capacity);

    if (string == NULL)
    {
        free(chunks);
        return NULL;
    }

    size_t position = 0;

    if (value->is_negative)
    {
        string[position++] = '-';
    }

    // Most significant chunk first, without leading zeros.
    uint64_t limb = chunks[chunk_count - 1];
    char buffer[20];
    size_t digits = 0;

    do
    {
        buffer[digits++] = (char)('0' + limb % 10);
        limb /= 10;
    } while (limb > 0);

    while (digits > 0)
    {
        string[position++] = buffer[--digits];
    }

    // Remaining chunks, most significant to least, each padded to exactly 19 digits.
    for (size_t i = chunk_count - 1; i > 0; )
    {
        --i;

        limb = chunks[i];

        for (size_t j = 19; j > 0; )
        {
            --j;
            string[position + j] = (char)('0' + limb % 10);
            limb /= 10;
        }

        position += 19;
    }

    string[position] = '\0';

    free(chunks);

    return string;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Comparision functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

int bigint_compare( /*Compare two BigInts*/
    const BigInt *a, 
    const BigInt *b
)
{
    if (a == NULL || b == NULL)
    {
        return 0;
    }

    if (a->is_negative && !b->is_negative)
    {
        return -1;
    }
    else if (!a->is_negative && b->is_negative)
    {
        return 1;
    }

    int cmp = bigint_compare_abs(a, b);

    if (a->is_negative)
    {
        return -cmp;
    }
    else
    {
        return cmp;
    }
}

bool bigint_is_zero( /*Check if a BigInt is zero*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    return value->size == 0;
}

bool bigint_is_one( /*Check if a BigInt is one*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    return value->size == 1 && value->limbs[0] == 1 && !value->is_negative;
}

bool bigint_is_negative( /*Check if a BigInt is negative*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    return value->is_negative;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Arithmetic operation functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

BigIntStatus bigint_add( /*Add two BigInts (a+b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    // Aliasing note: in every branch below, every read of a->is_negative
    // or b->is_negative happens before the first write to
    // result->is_negative, so this is correct even if result aliases a
    // and/or b (that write may be aliased with a future read, but there
    // is no such future read in this function).
    if (a->is_negative && b->is_negative)
    {
        result->is_negative = true;
        return bigint_add_abs(result, a, b);
    }

    int cmp = bigint_compare_abs(a, b);

    if (a->is_negative)
    {
        if (cmp > 0)
        {
            result->is_negative = true;
            return bigint_subtract_abs(result, a, b);
        }

        result->is_negative = false;
        return bigint_subtract_abs(result, b, a);
    }

    if (b->is_negative)
    {
        if (cmp >= 0)
        {
            result->is_negative = false;
            return bigint_subtract_abs(result, a, b);
        }

        result->is_negative = true;
        return bigint_subtract_abs(result, b, a);
    }

    result->is_negative = false;

    return bigint_add_abs(result, a, b);
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

    if (bigint_size_add(a->size, b->size, &result_size) != BIGINT_OK)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    // Accumulate into a fresh buffer rather than result->limbs directly, so
    // this is safe even when result aliases a and/or b (e.g. squaring via
    // bigint_mul(x, x, x)). calloc itself checks result_size*sizeof(uint64_t)
    // for overflow on top of the result_size computation above.
    uint64_t *product = calloc(result_size, sizeof(uint64_t));

    if (product == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < b->size; i++)
    {
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

    if (b->size == 0)
    {
        return BIGINT_DIVISION_BY_ZERO;
    }
    if(quotient == remainder)
    {
        return BIGINT_INVALID_ARGUMENT;
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

        status = bigint_copy(quotient, &temp_quotient);
    }

    if (status == BIGINT_OK)
    {
        status = bigint_copy(remainder, &temp_remainder);
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
        bool exponent_is_odd = bigint_get_bit(exponent, 0) != 0;
        result->is_negative = base->is_negative && exponent_is_odd;
        bigint_normalize(result);
    }

    free(accumulator.limbs);
    free(base_power.limbs);

    return status;
}

BigIntStatus bigint_gcd( /*Greatest common divisor for BigInts (gcd(a,b)) via the Euclidean algorithm*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    BigInt x;
    x.limbs = NULL;
    x.size = 0;
    x.capacity = 0;
    x.is_negative = false;

    BigInt y;
    y.limbs = NULL;
    y.size = 0;
    y.capacity = 0;
    y.is_negative = false;

    BigInt remainder;
    remainder.limbs = NULL;
    remainder.size = 0;
    remainder.capacity = 0;
    remainder.is_negative = false;

    BigIntStatus status = bigint_abs(&x, a);

    if (status == BIGINT_OK)
    {
        status = bigint_abs(&y, b);
    }

    while (status == BIGINT_OK && y.size != 0)
    {
        status = bigint_mod(&remainder, &x, &y);

        if (status == BIGINT_OK)
        {
            // Rotate ownership: x <- y, y <- remainder, remainder <- (old x, now scratch).
            BigInt temp = x;
            x = y;
            y = remainder;
            remainder = temp;
        }
    }

    if (status == BIGINT_OK)
    {
        status = bigint_copy(result, &x);
    }

    if (status == BIGINT_OK)
    {
        result->is_negative = false;
    }

    free(x.limbs);
    free(y.limbs);
    free(remainder.limbs);

    return status;
}

BigIntStatus bigint_lcm( /*Least common multiple for BigInts: lcm(a,b) = (|a| / gcd(a,b)) * |b|*/
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

    BigInt g;
    g.limbs = NULL;
    g.size = 0;
    g.capacity = 0;
    g.is_negative = false;

    BigInt abs_a;
    abs_a.limbs = NULL;
    abs_a.size = 0;
    abs_a.capacity = 0;
    abs_a.is_negative = false;

    BigInt abs_b;
    abs_b.limbs = NULL;
    abs_b.size = 0;
    abs_b.capacity = 0;
    abs_b.is_negative = false;

    BigInt quotient;
    quotient.limbs = NULL;
    quotient.size = 0;
    quotient.capacity = 0;
    quotient.is_negative = false;

    BigInt remainder;
    remainder.limbs = NULL;
    remainder.size = 0;
    remainder.capacity = 0;
    remainder.is_negative = false;

    // (|a| / gcd) * |b| instead of |a*b| / gcd: dividing first keeps the
    // intermediate value (and therefore the work bigint_mul has to do)
    // roughly gcd times smaller, and it's an exact division (no remainder)
    // since gcd(a,b) always divides a.
    BigIntStatus status = bigint_gcd(&g, a, b);

    if (status == BIGINT_OK)
    {
        status = bigint_abs(&abs_a, a);
    }

    if (status == BIGINT_OK)
    {
        status = bigint_abs(&abs_b, b);
    }

    if (status == BIGINT_OK)
    {
        status = bigint_div_mod(&quotient, &remainder, &abs_a, &g);
    }

    if (status == BIGINT_OK)
    {
        status = bigint_mul(result, &quotient, &abs_b);
    }

    if (status == BIGINT_OK)
    {
        result->is_negative = false;
    }

    free(g.limbs);
    free(abs_a.limbs);
    free(abs_b.limbs);
    free(quotient.limbs);
    free(remainder.limbs);

    return status;
}

BigIntStatus bigint_factorial( /*Calculate factorial of a BigInt (n!)*/
    BigInt *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (value->is_negative)
    {
        return BIGINT_NEGATIVE_ARGUMENT; // factorial is undefined for negative numbers
    }

    if (value->size == 0)
    {
        return bigint_set_uint64(result, 1); // 0! = 1
    }

    if (value->size > 1 || value->limbs[0] > BIGINT_FACTORIAL_MAX_N)
    {
        // n! for n this large is astronomically larger than could be
        // computed or stored; refuse rather than churn forever.
        return BIGINT_VALUE_TOO_LARGE;
    }

    uint64_t n = value->limbs[0]; // copied out before result is touched - safe even if result aliases value

    BigIntStatus status = bigint_set_uint64(result, 1);

    if (status == BIGINT_OK && n >= 2)
    {
        uint64_t i = 2;

        // Checking `i == n` *before* incrementing (instead of the more
        // obvious `for (i = 2; i <= n; i++)`) matters at the boundary:
        // if n == UINT64_MAX, "i <= n" is always true and "i++" wraps
        // i back to 0 once it reaches UINT64_MAX, which would loop
        // forever, repeatedly corrupting result by multiplying by 0.
        // This form can never increment i past n, so it can't wrap.
        for (;;)
        {
            status = bigint_multiply_by_uint64(result, i);

            if (status != BIGINT_OK)
            {
                break;
            }

            if (i == n)
            {
                break;
            }

            i++;
        }
    }

    if (status == BIGINT_OK)
    {
        result->is_negative = false;
    }

    return status;
}

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

        limbs = malloc(bytes);

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

        limbs = malloc(bytes);

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

        limbs = malloc(bytes);

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

    size_t limb_shift = n / 64;
    size_t bit_shift = n % 64;

    size_t new_size;
    size_t new_size_bytes;

    if (bigint_size_add(a->size, limb_shift, &new_size) != BIGINT_OK ||
        bigint_size_add(new_size, 1, &new_size) != BIGINT_OK ||
        bigint_size_mul(new_size, sizeof(uint64_t), &new_size_bytes) != BIGINT_OK)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    uint64_t *limbs = calloc(new_size, sizeof(uint64_t));

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
            limbs[i + limb_shift + 1] |= (part >> (64 - bit_shift));
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

    size_t limb_shift = n / 64;

    if (limb_shift >= a->size)
    {
        result->size = 0;
        result->is_negative = false;
        return BIGINT_OK;
    }

    size_t bit_shift = n % 64;
    size_t new_size = a->size - limb_shift; // limb_shift < a->size, just checked above, so this can't underflow

    size_t new_size_bytes;

    if (bigint_size_mul(new_size, sizeof(uint64_t), &new_size_bytes) != BIGINT_OK)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    uint64_t *limbs = malloc(new_size_bytes);

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
                merged |= (a->limbs[i + limb_shift + 1] << (64 - bit_shift));
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

/*
------------------------------------------------------------------------------------------------------------------------------
    Additional utility check functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

bool bigint_is_even( /*Check if a BigInt is even*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return false;
    }

    if (value->size == 0)
    {
        return true; // Zero is even
    }

    return (value->limbs[0] & 1) == 0;
}

bool bigint_is_odd( /*Check if a BigInt is odd*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return false;
    }

    if (value->size == 0)
    {
        return false; // Zero is not odd
    }

    return (value->limbs[0] & 1) != 0;
}

static BigIntStatus bigint_modular_multiply(
    BigInt *result,
    const BigInt *a,
    const BigInt *b,
    const BigInt *modulus
)
{
    BigInt product = { NULL, 0, 0, false };
    BigIntStatus status = bigint_mul(&product, a, b);

    if (status == BIGINT_OK)
    {
        status = bigint_mod(result, &product, modulus);
    }

    free(product.limbs);
    return status;
}

static BigIntStatus bigint_modular_pow(
    BigInt *result,
    const BigInt *base,
    const BigInt *exponent,
    const BigInt *modulus
)
{
    BigInt accumulator = { NULL, 0, 0, false };
    BigInt factor = { NULL, 0, 0, false };
    BigInt remaining = { NULL, 0, 0, false };

    BigIntStatus status = bigint_set_uint64(&accumulator, 1);

    if (status == BIGINT_OK)
    {
        status = bigint_mod(&accumulator, &accumulator, modulus);
    }
    if (status == BIGINT_OK)
    {
        status = bigint_mod(&factor, base, modulus);
    }
    if (status == BIGINT_OK)
    {
        status = bigint_copy(&remaining, exponent);
    }

    while (status == BIGINT_OK && remaining.size != 0)
    {
        if (bigint_is_odd(&remaining))
        {
            status = bigint_modular_multiply(
                &accumulator, &accumulator, &factor, modulus);
        }

        if (status == BIGINT_OK)
        {
            status = bigint_shift_right(&remaining, &remaining, 1);
        }

        if (status == BIGINT_OK && remaining.size != 0)
        {
            status = bigint_modular_multiply(&factor, &factor, &factor, modulus);
        }
    }

    if (status == BIGINT_OK)
    {
        status = bigint_copy(result, &accumulator);
    }

    free(accumulator.limbs);
    free(factor.limbs);
    free(remaining.limbs);
    return status;
}

static bool bigint_is_miller_rabin_witness(
    const BigInt *value,
    const BigInt *odd_part,
    size_t squarings,
    uint64_t witness
)
{
    uint64_t witness_storage;
    uint64_t one_storage;
    BigInt witness_value;
    BigInt one;
    bigint_wrap_uint64(&witness_value, &witness_storage, witness);
    bigint_wrap_uint64(&one, &one_storage, 1);

    BigInt value_minus_one = { NULL, 0, 0, false };
    BigInt x = { NULL, 0, 0, false };
    BigIntStatus status = bigint_sub(&value_minus_one, value, &one);

    if (status == BIGINT_OK)
    {
        status = bigint_mod(&x, &witness_value, value);
    }

    if (status == BIGINT_OK && x.size == 0)
    {
        free(value_minus_one.limbs);
        free(x.limbs);
        return true;
    }

    if (status == BIGINT_OK)
    {
        status = bigint_modular_pow(&x, &x, odd_part, value);
    }

    if (status == BIGINT_OK &&
        (bigint_compare(&x, &one) == 0 || bigint_compare(&x, &value_minus_one) == 0))
    {
        free(value_minus_one.limbs);
        free(x.limbs);
        return true;
    }

    for (size_t i = 1; status == BIGINT_OK && i < squarings; i++)
    {
        status = bigint_modular_multiply(&x, &x, &x, value);

        if (status == BIGINT_OK && bigint_compare(&x, &value_minus_one) == 0)
        {
            free(value_minus_one.limbs);
            free(x.limbs);
            return true;
        }
    }

    free(value_minus_one.limbs);
    free(x.limbs);
    return false;
}

bool bigint_is_probable_prime( /*Check primality with Miller-Rabin*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return false;
    }

    if (value->is_negative || value->size == 0 ||
        (value->size == 1 && value->limbs[0] < 2))
    {
        return false; // Numbers less than 2 are not prime
    }

    if (bigint_is_even(value))
    {
        return value->size == 1 && value->limbs[0] == 2;
    }

    uint64_t one_storage;
    BigInt one;
    bigint_wrap_uint64(&one, &one_storage, 1);

    BigInt odd_part = { NULL, 0, 0, false };
    BigIntStatus status = bigint_sub(&odd_part, value, &one);
    size_t squarings = 0;

    while (status == BIGINT_OK && bigint_is_even(&odd_part))
    {
        status = bigint_shift_right(&odd_part, &odd_part, 1);
        squarings++;
    }

    // These witnesses are deterministic for every unsigned 64-bit integer.
    // For larger values they provide a strong probable-prime test.
    static const uint64_t witnesses[] = {
        2ULL, 325ULL, 9375ULL, 28178ULL,
        450775ULL, 9780504ULL, 1795265022ULL
    };

    for (size_t i = 0;
         status == BIGINT_OK && i < sizeof(witnesses) / sizeof(witnesses[0]);
         i++)
    {
        if (!bigint_is_miller_rabin_witness(
                value, &odd_part, squarings, witnesses[i]))
        {
            status = BIGINT_INVALID_ARGUMENT;
        }
    }

    free(odd_part.limbs);
    return status == BIGINT_OK;
}

bool bigint_is_perfect_square( /*Check if a BigInt is a perfect square*/
    const BigInt *value
)
{
    if (value == NULL || value->is_negative)
    {
        return false;
    }

    if (value->size == 0)
    {
        return true; // Zero is a perfect square
    }

    // Build the integer square root one bit at a time, most significant
    // bit first: tentatively set each bit and keep it only if the square
    // doesn't overshoot.
    size_t bits = bigint_bit_length(value);
    size_t root_bits = (bits + 1) / 2;

    BigInt root;
    root.limbs = NULL;
    root.size = 0;
    root.capacity = 0;
    root.is_negative = false;

    BigInt candidate;
    candidate.limbs = NULL;
    candidate.size = 0;
    candidate.capacity = 0;
    candidate.is_negative = false;

    BigInt candidate_squared;
    candidate_squared.limbs = NULL;
    candidate_squared.size = 0;
    candidate_squared.capacity = 0;
    candidate_squared.is_negative = false;

    BigIntStatus status = BIGINT_OK;

    for (size_t i = root_bits; status == BIGINT_OK && i > 0; i--)
    {
        size_t bit_index = i - 1;

        status = bigint_copy(&candidate, &root);

        if (status == BIGINT_OK)
        {
            status = bigint_set_bit(&candidate, bit_index);
        }

        if (status == BIGINT_OK)
        {
            status = bigint_mul(&candidate_squared, &candidate, &candidate);
        }

        if (status == BIGINT_OK && bigint_compare_abs(&candidate_squared, value) <= 0)
        {
            BigInt temp = root;
            root = candidate;
            candidate = temp;
        }
    }

    bool result = false;

    if (status == BIGINT_OK)
    {
        BigInt root_squared;
        root_squared.limbs = NULL;
        root_squared.size = 0;
        root_squared.capacity = 0;
        root_squared.is_negative = false;

        if (bigint_mul(&root_squared, &root, &root) == BIGINT_OK)
        {
            result = (bigint_compare_abs(&root_squared, value) == 0);
        }

        free(root_squared.limbs);
    }

    free(root.limbs);
    free(candidate.limbs);
    free(candidate_squared.limbs);

    return status == BIGINT_OK && result;
}
