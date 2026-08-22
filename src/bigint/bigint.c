#include "bigint_internal.h"
#include <mysciencecalc/bigint.h>

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Storage note: limbs are base-2^64, little-endian (limbs[0] is the least
    significant 64 bits). Every helper below assumes this consistently -
    this is what lets add/sub run as plain ripple-carry, mul run without any
    per-limb division, and the bitwise/shift operations work directly on the
    limb array.

    Canonical form: every BigInt this file hands back to a caller has no
    trailing (most significant) zero limbs, and is never "negative zero"
    (size == 0 implies is_negative == false). bigint_normalize() below is
    the single place that enforces this; every function that could produce
    a non-canonical result calls it before returning.
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
    // Both callers in this file (bigint_mod_uint64, bigint_divide_by_uint64)
    // satisfy this by construction: they seed `high` with 0 and thereafter
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

static int bigint_size_add( /*Overflow-checked size_t addition: out = a+b, or fail*/
    size_t a,
    size_t b,
    size_t *out
)
{
    if (a > SIZE_MAX - b)
    {
        return 0;
    }

    *out = a + b;

    return 1;
}

static int bigint_size_mul( /*Overflow-checked size_t multiplication: out = a*b, or fail*/
    size_t a,
    size_t b,
    size_t *out
)
{
    if (a != 0 && b > SIZE_MAX / a)
    {
        return 0;
    }

    *out = a * b;

    return 1;
}

static uint64_t bigint_mod_uint64( /*Compute value % divisor without mutating value*/
    const BigInt *value,
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
        bigint_divide_128_by_u64(
            remainder,
            value->limbs[i - 1],
            divisor,
            &remainder);
    }

    return remainder;
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

static int bigint_ensure_capacity( /*Grow a BigInt's limb buffer to hold at least `needed` limbs, with overflow checks at every step*/
    BigInt *value,
    size_t needed
)
{
    if (value == NULL)
    {
        return 0;
    }

    if (value->capacity >= needed)
    {
        return 1;
    }

    size_t new_capacity = (value->capacity == 0) ? 1 : value->capacity;

    while (new_capacity < needed)
    {
        size_t doubled;

        if (!bigint_size_mul(new_capacity, 2, &doubled))
        {
            // Doubling would overflow size_t; jump straight to exactly
            // what's needed instead of looping forever.
            new_capacity = needed;
            break;
        }

        new_capacity = doubled;
    }

    size_t new_capacity_bytes;

    if (!bigint_size_mul(new_capacity, sizeof(uint64_t), &new_capacity_bytes))
    {
        return 0; // the requested limb count can't be expressed as a byte size on this platform
    }

    uint64_t *new_limbs = realloc(value->limbs, new_capacity_bytes);

    if (new_limbs == NULL)
    {
        return 0;
    }

    value->limbs = new_limbs;
    value->capacity = new_capacity;

    return 1;
}

static int bigint_add_uint64( /*Add a uint64_t to a BigInt*/
    BigInt *value, 
    uint64_t amount
)
{
    if (value == NULL)
    {
        return 0;
    }

    if (amount == 0)
    {
        return 1;
    }

    if (value->size == 0)
    {
        if (!bigint_ensure_capacity(value, 1))
        {
            return 0;
        }

        value->limbs[0] = amount;
        value->size = 1;

        return 1;
    }

    uint64_t old = value->limbs[0];

    value->limbs[0] += amount;

    if (value->limbs[0] >= old)
    {
        return 1;
    }

    size_t index = 1;

    while (index < value->size)
    {
        old = value->limbs[index];

        value->limbs[index] += 1;

        if (value->limbs[index] >= old)
        {
            return 1;
        }

        index++;
    }

    size_t needed;

    if (!bigint_size_add(value->size, 1, &needed) || !bigint_ensure_capacity(value, needed))
    {
        return 0;
    }

    value->limbs[value->size] = 1;
    value->size++;

    return 1;
}

static int bigint_sub_uint64( /*Subtract a uint64_t from a BigInt*/
    BigInt *value,
    uint64_t amount
)
{
    if (value == NULL)
    {
        return 0;
    }

    if (amount == 0)
    {
        return 1;
    }

    if (value->size == 0)
    {
        return 1;
    }

    uint64_t old = value->limbs[0];

    value->limbs[0] -= amount;

    if (value->limbs[0] <= old)
    {
        return 1;
    }

    size_t index = 1;

    while (index < value->size)
    {
        old = value->limbs[index];

        value->limbs[index] -= 1;

        if (value->limbs[index] <= old)
        {
            return 1;
        }

        index++;
    }

    bigint_normalize(value);

    return 1;
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

static int bigint_multiply_by_uint64( /*Multiply a BigInt by a uint64_t*/
    BigInt *value,
    uint64_t multiplier
)
{
    if (value == NULL)
    {
        return 0;
    }

    if (value->size == 0)
    {
        return 1;
    }

    if (multiplier == 0)
    {
        value->size = 0;
        bigint_normalize(value);
        return 1;
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

        if (!bigint_size_add(value->size, 1, &needed) || !bigint_ensure_capacity(value, needed))
        {
            return 0;
        }

        value->limbs[value->size] = carry;
        value->size++;
    }

    return 1;
}

static int bigint_add_abs( /*Add the absolute values of two BigInts*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    size_t max_size =
        (a->size > b->size) ? a->size : b->size;

    size_t needed;

    if (!bigint_size_add(max_size, 1, &needed) || !bigint_ensure_capacity(result, needed))
    {
        return 0;
    }

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

    return 1;
}

static int bigint_subtract_abs( /*Subtract the absolute values of two BigInts*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if (bigint_compare_abs(a, b) < 0)
    {
        return bigint_subtract_abs(result,b,a);
    }

    if (!bigint_ensure_capacity(result, a->size))
    {
        return 0;
    }

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

    return 1;
}

static int bigint_set_uint64( /*Set a BigInt to a small non-negative value, growing capacity as needed*/
    BigInt *value,
    uint64_t amount
)
{
    if (value == NULL)
    {
        return 0;
    }

    if (amount == 0)
    {
        value->size = 0;
        value->is_negative = false;
        return 1;
    }

    if (!bigint_ensure_capacity(value, 1))
    {
        return 0;
    }

    value->limbs[0] = amount;
    value->size = 1;
    value->is_negative = false;

    return 1;
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

static int bigint_set_bit( /*Set a single bit of |value|, growing the BigInt as needed*/
    BigInt *value,
    size_t bit_index
)
{
    size_t limb_index = bit_index / 64;
    size_t bit_offset = bit_index % 64;

    size_t needed;

    if (!bigint_size_add(limb_index, 1, &needed) || !bigint_ensure_capacity(value, needed))
    {
        return 0;
    }

    while (value->size <= limb_index)
    {
        value->limbs[value->size] = 0;
        value->size++;
    }

    value->limbs[limb_index] |= (1ULL << bit_offset);

    return 1;
}

static int bigint_shift_left_one_bit( /*Multiply |value| by 2 in place*/
    BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    if (value->size == 0)
    {
        return 1;
    }

    uint64_t overflow = value->limbs[value->size - 1] >> 63;

    if (overflow)
    {
        size_t needed;

        if (!bigint_size_add(value->size, 1, &needed) || !bigint_ensure_capacity(value, needed))
        {
            return 0;
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

    return 1;
}

static int bigint_divmod_abs( /*Long division on magnitudes only: quotient = |a|/|b|, remainder = |a|%|b|*/
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
        return 1;
    }

    if (!bigint_ensure_capacity(quotient, a->size))
    {
        return 0;
    }

    for (size_t i = bits; i > 0; i--)
    {
        size_t bit_index = i - 1;

        if (!bigint_shift_left_one_bit(remainder))
        {
            return 0;
        }

        if (!bigint_add_uint64(remainder, (uint64_t)bigint_get_bit(a, bit_index)))
        {
            return 0;
        }

        if (bigint_compare_abs(remainder, b) >= 0)
        {
            if (!bigint_subtract_abs(remainder, remainder, b))
            {
                return 0;
            }

            if (!bigint_set_bit(quotient, bit_index))
            {
                return 0;
            }
        }
    }

    bigint_normalize(quotient);

    return 1;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Operation functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

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

int bigint_copy( /*Create a copy of a BigInt*/
    BigInt *destination, 
    const BigInt *source
)
{
    if (destination == NULL || source == NULL)
    {
        return 0;
    }

    if (destination == source)
    {
        return 1;
    }

    if (source->size == 0)
    {
        destination->size = 0;
        destination->is_negative = false;
        return 1;
    }

    if (!bigint_ensure_capacity(destination, source->size))
    {
        return 0;
    }

    memcpy(
        destination->limbs,
        source->limbs,
        source->size * sizeof(uint64_t)
    );

    destination->size = source->size;
    destination->is_negative = source->is_negative;

    return 1;
}

int bigint_set_string( /*Transform string to BigInt*/
    BigInt *value,
    const char *string
)
{
    if (value == NULL || string == NULL)
    {
        return 0;
    }

    size_t len = strlen(string);

    if (len == 0)
    {
        value->size = 0;
        value->is_negative = false;
        return 1;
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
        return 0; // sign with no digits
    }

    for (size_t i = start; i < len; i++)
    {
        if (string[i] < '0' || string[i] > '9')
        {
            return 0;
        }
    }

    value->size = 0;
    value->is_negative = false;

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

        if (!bigint_multiply_by_uint64(value, chunk_multiplier) ||
            !bigint_add_uint64(value, chunk_value))
        {
            return 0;
        }

        i += chunk_len;
    }

    value->is_negative = negative;
    bigint_normalize(value);

    return 1;
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

    if (!bigint_copy(&scratch, value))
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

    if (!bigint_size_mul(value->size, 64, &bits_estimate) ||
        !bigint_size_add(bits_estimate, 62, &rounded) ||
        !bigint_size_add(rounded / 63, 1, &chunks_capacity) ||
        !bigint_size_mul(chunks_capacity, sizeof(uint64_t), &chunks_bytes))
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

    if (!bigint_size_mul(chunk_count, 19, &chunk_digits) ||
        !bigint_size_add(chunk_digits, 2, &capacity) ||
        (value->is_negative && !bigint_size_add(capacity, 1, &capacity)))
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

int bigint_compare_abs( /*Compare two absolute values of BigInts*/
    const BigInt *a, 
    const BigInt *b
)
{
    if (a == NULL || b == NULL)
    {
        return 0;
    }

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

int bigint_is_zero( /*Check if a BigInt is zero*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    return value->size == 0;
}

int bigint_is_one( /*Check if a BigInt is one*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    return value->size == 1 && value->limbs[0] == 1 && !value->is_negative;
}

int bigint_is_negative( /*Check if a BigInt is negative*/
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

int bigint_add( /*Add two BigInts (a+b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

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

int bigint_sub( /*Subtract two BigInts (a-b)*/
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    // a - b == a + (-b). Delegating to bigint_add (which already handles
    // every sign combination correctly) instead of duplicating that logic
    // here is what fixes the differing-signs case.
    BigInt negated_b;
    negated_b.limbs = NULL;
    negated_b.size = 0;
    negated_b.capacity = 0;
    negated_b.is_negative = false;

    if (!bigint_negate(&negated_b, b))
    {
        free(negated_b.limbs);
        return 0;
    }

    int success = bigint_add(result, a, &negated_b);

    free(negated_b.limbs);

    return success;
}

int bigint_mul( /*Multiply two BigInts (a*b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if (a->size == 0 || b->size == 0)
    {
        result->size = 0;
        result->is_negative = false;

        return 1;
    }

    size_t result_size;

    if (!bigint_size_add(a->size, b->size, &result_size))
    {
        return 0;
    }

    // Accumulate into a fresh buffer rather than result->limbs directly, so
    // this is safe even when result aliases a and/or b (e.g. squaring via
    // bigint_mul(x, x, x)). calloc itself checks result_size*sizeof(uint64_t)
    // for overflow on top of the result_size computation above.
    uint64_t *product = calloc(result_size, sizeof(uint64_t));

    if (product == NULL)
    {
        return 0;
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

    return 1;
}

int bigint_div_mod( /*Divide and modulo operation for BigInts (a/b and a%b), truncating toward zero*/
    BigInt *quotient,
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
)
{
    if (quotient == NULL || remainder == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if (b->size == 0)
    {
        return 0; // division by zero
    }

    // Work from local copies of the magnitudes so this is correct even if
    // quotient/remainder alias a or b.
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

    int ok = bigint_copy(&a_copy, a) && bigint_copy(&b_copy, b);

    if (ok)
    {
        bool a_negative = a->is_negative;
        bool b_negative = b->is_negative;

        ok = bigint_divmod_abs(quotient, remainder, &a_copy, &b_copy);

        if (ok)
        {
            quotient->is_negative = (a_negative != b_negative);
            bigint_normalize(quotient);

            remainder->is_negative = a_negative;
            bigint_normalize(remainder);
        }
    }

    free(a_copy.limbs);
    free(b_copy.limbs);

    return ok;
}

int bigint_div( /*Divide two BigInts (a/b), truncating toward zero*/
    BigInt *quotient,
    BigInt *remainder,
    const BigInt *a,
    const BigInt *b
)
{
    return bigint_div_mod(quotient, remainder, a, b);
}

int bigint_mod( /*Modulo operation for BigInts (a%b)*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    BigInt quotient;
    quotient.limbs = NULL;
    quotient.size = 0;
    quotient.capacity = 0;
    quotient.is_negative = false;

    int ok = bigint_div_mod(&quotient, result, a, b);

    free(quotient.limbs);

    return ok;
}

int bigint_pow( /*Exponentiation for BigInts (base^exponent) via binary exponentiation*/
    BigInt *result,
    const BigInt *base,
    const BigInt *exponent
)
{
    if (result == NULL || base == NULL || exponent == NULL)
    {
        return 0;
    }

    if (exponent->is_negative)
    {
        return 0; // negative exponents have no BigInt result
    }

    if (exponent->size == 0)
    {
        return bigint_set_uint64(result, 1);
    }

    if (base->size == 0)
    {
        result->size = 0;
        result->is_negative = false;
        return 1;
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

    int ok = bigint_set_uint64(&accumulator, 1) && bigint_abs(&base_power, base);

    size_t bits = bigint_bit_length(exponent);

    for (size_t i = 0; ok && i < bits; i++)
    {
        if (bigint_get_bit(exponent, i))
        {
            ok = bigint_mul(&accumulator, &accumulator, &base_power);
        }

        if (ok && i + 1 < bits)
        {
            ok = bigint_mul(&base_power, &base_power, &base_power);
        }
    }

    if (ok)
    {
        ok = bigint_copy(result, &accumulator);
    }

    if (ok)
    {
        bool exponent_is_odd = bigint_get_bit(exponent, 0) != 0;
        result->is_negative = base->is_negative && exponent_is_odd;
        bigint_normalize(result);
    }

    free(accumulator.limbs);
    free(base_power.limbs);

    return ok;
}

int bigint_gcd( /*Greatest common divisor for BigInts (gcd(a,b)) via the Euclidean algorithm*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
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

    int ok = bigint_abs(&x, a) && bigint_abs(&y, b);

    while (ok && y.size != 0)
    {
        ok = bigint_mod(&remainder, &x, &y);

        if (ok)
        {
            // Rotate ownership: x <- y, y <- remainder, remainder <- (old x, now scratch).
            BigInt temp = x;
            x = y;
            y = remainder;
            remainder = temp;
        }
    }

    if (ok)
    {
        ok = bigint_copy(result, &x);
    }

    if (ok)
    {
        result->is_negative = false;
    }

    free(x.limbs);
    free(y.limbs);
    free(remainder.limbs);

    return ok;
}

int bigint_lcm( /*Least common multiple for BigInts: lcm(a,b) = (|a| / gcd(a,b)) * |b|*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if (a->size == 0 || b->size == 0)
    {
        result->size = 0;
        result->is_negative = false;
        return 1;
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
    int ok = bigint_gcd(&g, a, b) &&
             bigint_abs(&abs_a, a) &&
             bigint_abs(&abs_b, b) &&
             bigint_div_mod(&quotient, &remainder, &abs_a, &g) &&
             bigint_mul(result, &quotient, &abs_b);

    if (ok)
    {
        result->is_negative = false;
    }

    free(g.limbs);
    free(abs_a.limbs);
    free(abs_b.limbs);
    free(quotient.limbs);
    free(remainder.limbs);

    return ok;
}

int bigint_factorial( /*Calculate factorial of a BigInt (n!)*/
    BigInt *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return 0;
    }

    if (value->is_negative)
    {
        return 0; // factorial is undefined for negative numbers
    }

    if (value->size == 0)
    {
        return bigint_set_uint64(result, 1); // 0! = 1
    }

    if (value->size > 1)
    {
        // n! for n this large is astronomically larger than could ever be
        // computed or stored; refuse rather than churn forever.
        return 0;
    }

    uint64_t n = value->limbs[0];

    if (!bigint_set_uint64(result, 1))
    {
        return 0;
    }

    if (n >= 2)
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
            if (!bigint_multiply_by_uint64(result, i))
            {
                return 0;
            }

            if (i == n)
            {
                break;
            }

            i++;
        }
    }

    result->is_negative = false;

    return 1;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Bitwise operation functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

int bigint_and( /*Bitwise AND for BigInts (a&b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if (a->is_negative || b->is_negative)
    {
        return 0;
    }

    size_t min_size = (a->size < b->size) ? a->size : b->size;

    uint64_t *limbs = NULL;

    if (min_size > 0)
    {
        size_t bytes;

        if (!bigint_size_mul(min_size, sizeof(uint64_t), &bytes))
        {
            return 0;
        }

        limbs = malloc(bytes);

        if (limbs == NULL)
        {
            return 0;
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

    return 1;
}

int bigint_or( /*Bitwise OR for BigInts (a|b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if (a->is_negative || b->is_negative)
    {
        return 0;
    }

    size_t max_size = (a->size > b->size) ? a->size : b->size;

    uint64_t *limbs = NULL;

    if (max_size > 0)
    {
        size_t bytes;

        if (!bigint_size_mul(max_size, sizeof(uint64_t), &bytes))
        {
            return 0;
        }

        limbs = malloc(bytes);

        if (limbs == NULL)
        {
            return 0;
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

    return 1;
}

int bigint_xor( /*Bitwise XOR for BigInts (a^b). Operands must be non-negative.*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if (a->is_negative || b->is_negative)
    {
        return 0;
    }

    size_t max_size = (a->size > b->size) ? a->size : b->size;

    uint64_t *limbs = NULL;

    if (max_size > 0)
    {
        size_t bytes;

        if (!bigint_size_mul(max_size, sizeof(uint64_t), &bytes))
        {
            return 0;
        }

        limbs = malloc(bytes);

        if (limbs == NULL)
        {
            return 0;
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

    return 1;
}

int bigint_not( /*Bitwise NOT for BigInts (~a), defined arbitrary-precision as -(a+1)*/
    BigInt *result,
    const BigInt *a
)
{
    if (result == NULL || a == NULL)
    {
        return 0;
    }

    BigInt one;
    one.limbs = NULL;
    one.size = 0;
    one.capacity = 0;
    one.is_negative = false;

    BigInt sum;
    sum.limbs = NULL;
    sum.size = 0;
    sum.capacity = 0;
    sum.is_negative = false;

    int ok = bigint_set_uint64(&one, 1);
    ok = ok && bigint_add(&sum, a, &one);
    ok = ok && bigint_negate(result, &sum);

    free(one.limbs);
    free(sum.limbs);

    return ok;
}

int bigint_shift_left( /*Left shift for BigInts (a<<n)*/
    BigInt *result,
    const BigInt *a,
    size_t n
)
{
    if (result == NULL || a == NULL)
    {
        return 0;
    }

    if (a->size == 0 || n == 0)
    {
        return bigint_copy(result, a);
    }

    size_t limb_shift = n / 64;
    size_t bit_shift = n % 64;

    size_t new_size;
    size_t new_size_bytes;

    if (!bigint_size_add(a->size, limb_shift, &new_size) ||
        !bigint_size_add(new_size, 1, &new_size) ||
        !bigint_size_mul(new_size, sizeof(uint64_t), &new_size_bytes))
    {
        return 0;
    }

    uint64_t *limbs = calloc(new_size, sizeof(uint64_t));

    if (limbs == NULL)
    {
        return 0;
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

    return 1;
}

int bigint_shift_right( /*Right shift for BigInts (a>>n), truncating toward zero*/
    BigInt *result,
    const BigInt *a,
    size_t n
)
{
    if (result == NULL || a == NULL)
    {
        return 0;
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
        return 1;
    }

    size_t bit_shift = n % 64;
    size_t new_size = a->size - limb_shift; // limb_shift < a->size, just checked above, so this can't underflow

    size_t new_size_bytes;

    if (!bigint_size_mul(new_size, sizeof(uint64_t), &new_size_bytes))
    {
        return 0;
    }

    uint64_t *limbs = malloc(new_size_bytes);

    if (limbs == NULL)
    {
        return 0;
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

    return 1;
}

int bigint_increment( /*Increment a BigInt by 1 (a++)*/
    BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    return bigint_add_uint64(value, 1);
}

int bigint_decrement( /*Decrement a BigInt by 1 (a--)*/
    BigInt *value
)
{
    if (value == NULL)
    {
        return 0;
    }

    return bigint_sub_uint64(value, 1);
}

int bigint_abs( /*Absolute value of a BigInt (|a|)*/
    BigInt *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return 0;
    }

    if (!bigint_copy(result, value))
    {
        return 0;
    }

    result->is_negative = false;

    return 1;
}

int bigint_negate( /*Negate a BigInt (-a)*/
    BigInt *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return 0;
    }

    if (!bigint_copy(result, value))
    {
        return 0;
    }

    result->is_negative = !value->is_negative;

    // Zero has no sign - flipping it must not produce a "negative zero"
    // that would compare unequal to a plain zero. bigint_normalize()
    // enforces exactly that (it's a no-op here beyond the sign check,
    // since a copy of an already-canonical value has no trailing zeros
    // to trim).
    bigint_normalize(result);

    return 1;
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

bool bigint_is_most_likely_prime( /*Check if a BigInt is most likely prime via bounded trial division*/
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

    if (value->size == 1 && value->limbs[0] == 2)
    {
        return true;
    }

    if (bigint_is_even(value))
    {
        return false;
    }

    // Bounded trial division: this is a deterministic proof of primality for
    // any value whose true square root is <= TRIAL_DIVISION_BOUND (i.e.
    // value <= ~9 * 10^12). Beyond that it is a fast composite-detecting
    // filter only - a composite value with no factor under the bound would
    // be (incorrectly) reported prime. Making this exact for arbitrarily
    // large values needs a probabilistic test (e.g. Miller-Rabin), which is
    // a reasonable next step if you need it.
    const uint64_t trial_division_bound = 3000000ULL;

    for (uint64_t divisor = 3; divisor <= trial_division_bound; divisor += 2)
    {
        if (bigint_mod_uint64(value, divisor) == 0)
        {
            return value->size == 1 && value->limbs[0] == divisor;
        }
    }

    return true;
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

    int ok = 1;

    for (size_t i = root_bits; ok && i > 0; i--)
    {
        size_t bit_index = i - 1;

        ok = bigint_copy(&candidate, &root) && bigint_set_bit(&candidate, bit_index);
        ok = ok && bigint_mul(&candidate_squared, &candidate, &candidate);

        if (ok && bigint_compare_abs(&candidate_squared, value) <= 0)
        {
            BigInt temp = root;
            root = candidate;
            candidate = temp;
        }
    }

    bool result = false;

    if (ok)
    {
        BigInt root_squared;
        root_squared.limbs = NULL;
        root_squared.size = 0;
        root_squared.capacity = 0;
        root_squared.is_negative = false;

        if (bigint_mul(&root_squared, &root, &root))
        {
            result = (bigint_compare_abs(&root_squared, value) == 0);
        }

        free(root_squared.limbs);
    }

    free(root.limbs);
    free(candidate.limbs);
    free(candidate_squared.limbs);

    return ok && result;
}