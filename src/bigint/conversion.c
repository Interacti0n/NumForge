#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/bigint.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BIGINT_LIMB_BITS 64U

/*
------------------------------------------------------------------------------------------------------------------------------
    Parsing, formatting, and comparison functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

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
        if (!numforge_budget_check())
        {
            free(parsed.limbs);
            return BIGINT_OUT_OF_MEMORY;
        }
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
        char *string = numforge_malloc(2);

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

    if (bigint_size_mul(value->size, BIGINT_LIMB_BITS, &bits_estimate) != BIGINT_OK ||
        bigint_size_add(bits_estimate, 62, &rounded) != BIGINT_OK ||
        bigint_size_add(rounded / 63, 1, &chunks_capacity) != BIGINT_OK ||
        bigint_size_mul(chunks_capacity, sizeof(uint64_t), &chunks_bytes) != BIGINT_OK)
    {
        free(scratch.limbs);
        return NULL;
    }

    uint64_t *chunks = numforge_malloc(chunks_bytes);

    if (chunks == NULL)
    {
        free(scratch.limbs);
        return NULL;
    }

    size_t chunk_count = 0;

    while (scratch.size > 0)
    {
        if (!numforge_budget_check())
        {
            free(scratch.limbs);
            free(chunks);
            return NULL;
        }
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

    char *string = numforge_malloc(capacity);

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
