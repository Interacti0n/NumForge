#include "bigint_internal.h"
#include <numforge/bigint.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Number-theory predicates and utility checks for BigInt.
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

static BigIntStatus bigint_modular_multiply( /*Compute (a * b) mod modulus, storing the result in result*/
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

static BigIntStatus bigint_modular_pow( /*Compute (base ^ exponent) mod modulus, storing the result in result*/
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

static BigIntStatus bigint_miller_rabin_passes( /*Check if a witness passes the Miller-Rabin test for a given value*/
    bool *result,
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
    bool passes = false;

    if (status == BIGINT_OK)
    {
        status = bigint_mod(&x, &witness_value, value);
    }

    if (status == BIGINT_OK && x.size == 0)
    {
        passes = true;
    }

    if (status == BIGINT_OK && !passes)
    {
        status = bigint_modular_pow(&x, &x, odd_part, value);
    }

    if (status == BIGINT_OK && !passes &&
        (bigint_compare(&x, &one) == 0 || bigint_compare(&x, &value_minus_one) == 0))
    {
        passes = true;
    }

    for (size_t i = 1; status == BIGINT_OK && !passes && i < squarings; i++)
    {
        status = bigint_modular_multiply(&x, &x, &x, value);

        if (status == BIGINT_OK && bigint_compare(&x, &value_minus_one) == 0)
        {
            passes = true;
        }
    }

    free(value_minus_one.limbs);
    free(x.limbs);

    if (status == BIGINT_OK)
    {
        *result = passes;
    }

    return status;
}

BigIntStatus bigint_is_probable_prime( /*Check primality with Miller-Rabin*/
    bool *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (value->is_negative || value->size == 0 ||
        (value->size == 1 && value->limbs[0] < 2))
    {
        *result = false; // Numbers less than 2 are not prime
        return BIGINT_OK;
    }

    if (bigint_is_even(value))
    {
        *result = value->size == 1 && value->limbs[0] == 2;
        return BIGINT_OK;
    }

    uint64_t one_storage;
    BigInt one;
    bigint_wrap_uint64(&one, &one_storage, 1);

    BigInt odd_part = { NULL, 0, 0, false };
    BigIntStatus status = bigint_sub(&odd_part, value, &one);
    size_t squarings = 0;
    bool is_probable_prime = true;

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
         status == BIGINT_OK && is_probable_prime &&
             i < sizeof(witnesses) / sizeof(witnesses[0]);
         i++)
    {
        bool passes;

        status = bigint_miller_rabin_passes(
            &passes, value, &odd_part, squarings, witnesses[i]
        );

        if (status == BIGINT_OK && !passes)
        {
            is_probable_prime = false;
        }
    }

    free(odd_part.limbs);

    if (status == BIGINT_OK)
    {
        *result = is_probable_prime;
    }

    return status;
}

BigIntStatus bigint_is_perfect_square( /*Check if a BigInt is a perfect square*/
    bool *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (value->is_negative)
    {
        *result = false;
        return BIGINT_OK;
    }

    if (value->size == 0)
    {
        *result = true; // Zero is a perfect square
        return BIGINT_OK;
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

    bool is_perfect_square = false;

    if (status == BIGINT_OK)
    {
        BigInt root_squared;
        root_squared.limbs = NULL;
        root_squared.size = 0;
        root_squared.capacity = 0;
        root_squared.is_negative = false;

        status = bigint_mul(&root_squared, &root, &root);

        if (status == BIGINT_OK)
        {
            is_perfect_square = (bigint_compare_abs(&root_squared, value) == 0);
        }

        free(root_squared.limbs);
    }

    free(root.limbs);
    free(candidate.limbs);
    free(candidate_squared.limbs);

    if (status == BIGINT_OK)
    {
        *result = is_perfect_square;
    }

    return status;
}
