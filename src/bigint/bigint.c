#include "bigint_internal.h"
#include <mysciencecalc/bigint.h>

#include <stdlib.h>
#include <string.h>

static uint64_t bigint_divide_128_by_u64(
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

static uint64_t bigint_divide_by_uint64(
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

static int bigint_add_uint64(
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

static void bigint_multiply_u64_u64(
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

static int bigint_multiply_by_uint64(
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



BigInt *bigint_create(
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

void bigint_destroy(
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

int bigint_set_uint64(
    BigInt *value, 
    uint64_t number
)
{
    if (value == NULL)
    {
        return 0;
    }

    if (number == 0)
    {
        value->size = 0;
        return 1;
    }

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

    value->limbs[0] = number;
    value->size = 1;

    return 1;
}

int bigint_copy(
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

    return 1;
}

char *bigint_to_string(
    const BigInt *value
)
{
    if (value == NULL)
    {
        return NULL;
    }

    BigInt *temp = bigint_create();

    if (temp == NULL)
    {
        return NULL;
    }

    if (!bigint_copy(temp, value))
    {
        bigint_destroy(temp);
        return NULL;
    }

    size_t capacity = 32;
    size_t length = 0;

    char *string = malloc(capacity);

    if (string == NULL)
    {
        bigint_destroy(temp);
        return NULL;
    }

    if (temp->size == 0)
    {
        string[0] = '0';
        string[1] = '\0';

        bigint_destroy(temp);

        return string;
    }

    while (temp->size > 0)
    {
        uint64_t remainder =
            bigint_divide_by_uint64(temp, 10);

        if (length + 1 >= capacity)
        {
            capacity *= 2;

            char *new_string =
                realloc(string, capacity);

            if (new_string == NULL)
            {
                free(string);
                bigint_destroy(temp);

                return NULL;
            }

            string = new_string;
        }

        string[length++] =
            (char)('0' + remainder);
    }

    string[length] = '\0';

    for (size_t i = 0; i < length / 2; i++)
    {
        char temp_char = string[i];

        string[i] =
            string[length - 1 - i];

        string[length - 1 - i] =
            temp_char;
    }

    bigint_destroy(temp);

    return string;
}

int bigint_set_string(
    BigInt *value,
    const char *string
)
{
    if (value == NULL || string == NULL)
    {
        return 0;
    }

    value->size = 0;

    for (size_t i = 0; string[i] != '\0'; i++)
    {
        char character = string[i];

        if (character < '0' || character > '9')
        {
            return 0;
        }

        uint64_t digit =
            (uint64_t)(character - '0');

        if (!bigint_multiply_by_uint64(value, 10))
        {
            return 0;
        }

        if (!bigint_add_uint64(value, digit))
        {
            return 0;
        }
    }

    return 1;
}

void bigint_string_free(
    char *string
)
{
    free(string);
}

int bigint_compare(
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

int bigint_add(
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return 0;
    }

    if(result == a || result == b)
    {
        BigInt *temp = bigint_create();

        if (temp == NULL)
        {
            return 0;
        }

        if (!bigint_add(temp, a, b))
        {
            bigint_destroy(temp);
            return 0;
        }

        if (!bigint_copy(result, temp))
        {
            bigint_destroy(temp);
            return 0;
        }

        bigint_destroy(temp);

        return 1;
    }

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

int bigint_sub(
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
)
{
    // Implementation of subtraction would go here
    return 1;
}

int bigint_mul(
    BigInt *result, 
    const BigInt *a, 
    const BigInt *b
)
{
    // Implementation of multiplication would go here
    return 1;
}












