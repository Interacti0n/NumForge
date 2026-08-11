#include "bigint_internal.h"
#include <mysciencecalc/bigint.h>

#include <stdlib.h>
#include <string.h>

static uint64_t bigint_divide_128_by_u64( /*Divide a 128-bit value by a uint64_t*/
    uint64_t high, 
    uint64_t low, 
    uint64_t divisor, 
    uint64_t *remainder
)
{
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

static uint64_t bigint_divide_by_uint64( /*Divide a BigInt by a uint64_t*/
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
        if (value->capacity < 1)
        {
            uint64_t *new_limbs = realloc(
                value->limbs,
                sizeof(uint64_t)
            );

            if (new_limbs == NULL)
            {
                return 0;
            }

            value->limbs = new_limbs;
            value->capacity = 1;
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

    if (value->size == value->capacity)
    {
        size_t new_capacity = value->capacity * 2;

        if (new_capacity == 0)
        {
            new_capacity = 1;
        }

        uint64_t *new_limbs = realloc(
            value->limbs,
            new_capacity * sizeof(uint64_t)
        );

        if (new_limbs == NULL)
        {
            return 0;
        }

        value->limbs = new_limbs;
        value->capacity = new_capacity;
    }

    value->limbs[value->size] = 1;
    value->size++;

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
        if (value->size == value->capacity)
        {
            size_t new_capacity = value->capacity * 2;

            if (new_capacity == 0)
            {
                new_capacity = 1;
            }

            uint64_t *new_limbs = realloc(
                value->limbs,
                new_capacity * sizeof(uint64_t)
            );

            if (new_limbs == NULL)
            {
                return 0;
            }

            value->limbs = new_limbs;
            value->capacity = new_capacity;
        }

        value->limbs[value->size] = carry;
        value->size++;
    }

    return 1;
}

static int bigint_subtract_abs( /*Subtract the absolute values of two BigInts*/
    BigInt *result,
    const BigInt *a,
    const BigInt *b
)
{
    if (bigint_compare_abs(a, b) < 0)
    {
        return bigint_subtract_abs(result,b,a);
    }

    if (result->capacity < a->size)
    {
        uint64_t *new_limbs = realloc(
            result->limbs,
            a->size * sizeof(uint64_t)
        );

        if (new_limbs == NULL)
        {
            return 0;
        }

        result->limbs = new_limbs;
        result->capacity = a->size;
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

    while (result->size > 0 &&
           result->limbs[result->size - 1] == 0)
    {
        result->size--;
    }

    return 1;
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
        return 1;
    }

    if (destination->capacity < source->size)
    {
        uint64_t *new_limbs = realloc(
            destination->limbs,
            source->size * sizeof(uint64_t)
        );

        if (new_limbs == NULL)
        {
            return 0;
        }

        destination->limbs = new_limbs;
        destination->capacity = source->size;
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

    // Maximum 19 decimal digits per limb.
    size_t capacity =
        value->size * 19 + 1;

    char *string = malloc(capacity);

    if (string == NULL)
    {
        return NULL;
    }

    size_t position = 0;

    // Highest limb first.
    uint64_t limb =
        value->limbs[value->size - 1];

    char buffer[19];
    size_t digits = 0;

    // Convert highest limb without leading zeros.
    do
    {
        buffer[digits++] =
            (char)('0' + limb % 10);

        limb /= 10;

    } while (limb > 0);

    while (digits > 0)
    {
        string[position++] =
            buffer[--digits];
    }

    // Remaining limbs must always use exactly 19 digits.
    for (size_t i = value->size - 1; i > 0; )
    {
        --i;

        limb = value->limbs[i];

        for (size_t j = 19; j > 0; )
        {
            --j;

            string[position + j] =
                (char)('0' + limb % 10);

            limb /= 10;
        }

        position += 19;
    }

    string[position] = '\0';

    return string;
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

    value->size = 0;

    size_t len = strlen(string);

    if (len == 0)
    {
        return 1;
    }

    // Each limb stores up to 19 decimal digits.
    size_t required = (len + 18) / 19;

    if (value->capacity < required)
    {
        uint64_t *new_limbs =
            realloc(
                value->limbs,
                required * sizeof(uint64_t)
            );

        if (new_limbs == NULL)
        {
            return 0;
        }

        value->limbs = new_limbs;
        value->capacity = required;
    }

    // Parse from right to left because limbs are little-endian.
    for (size_t i = len; i > 0; )
    {
        size_t start = (i > 19) ? i - 19 : 0;

        uint64_t limb = 0;

        for (size_t j = start; j < i; j++)
        {
            char character = string[j];

            if (character < '0' || character > '9')
            {
                return 0;
            }

            limb =
                limb * 10 +
                (uint64_t)(character - '0');
        }

        value->limbs[value->size++] = limb;

        i = start;
    }

    // Remove leading zero limbs.
    while (value->size > 1 &&
           value->limbs[value->size - 1] == 0)
    {
        value->size--;
    }

    return 1;
}

int bigint_compare_abs( /*Compare two absolute values of BigInts*/
    const BigInt *a, 
    const BigInt *b
)
{
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

int bigint_add( /*Add two BigInts (a+b)*/
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
)
{
    size_t max_size = (a->size > b->size) ? a->size : b->size;

    if (result->capacity < max_size + 1)
    {
        uint64_t *new_limbs = realloc(
            result->limbs,
            (max_size + 1) * sizeof(uint64_t)
        );

        if (new_limbs == NULL)
        {
            return 0;
        }

        result->limbs = new_limbs;
        result->capacity = max_size + 1;
    }

    uint64_t carry = 0;

    for (size_t i = 0; i < max_size; i++)
    {
        uint64_t limb_a = (i < a->size) ? a->limbs[i] : 0;
        uint64_t limb_b = (i < b->size) ? b->limbs[i] : 0;

        uint64_t sum = limb_a + limb_b + carry;

        if (sum < limb_a || sum < limb_b)
        {
            carry = 1;
        }
        else
        {
            carry = 0;
        }

        result->limbs[i] = sum;
    }

    if (carry > 0)
    {
        result->limbs[max_size] = carry;
        result->size = max_size + 1;
    }
    else
    {
        result->size = max_size;
    }

    return 1;
}

int bigint_sub( /*Subtract two BigInts (a-b)*/
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
)
{
    int cmp = bigint_compare_abs(a, b);

    if (cmp == 0)
    {
        result->size = 0;
        result->is_negative = false;
        return 1;
    }

    if (a->is_negative != b->is_negative)
    {
        result->is_negative = a->is_negative;
        bigint_add(result, a, b);
        return 1;
        
    }
    else if (!a->is_negative)
    {
        // Both positive: a - b
        result->is_negative = (cmp < 0);
        bigint_subtract_abs(result, a, b);
        return 1;
    }
    else
    {
        // Both negative: (-a) - (-b) = b - a
        result->is_negative = (cmp > 0);
        bigint_subtract_abs(result, a, b);
        return 1;
    }

    return 1;
}

int bigint_mul( /*Multiply two BigInts (a*b)*/
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
)
{
    // Implementation of multiplication would go here
    return 1;
}












