#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/bigint.h>

#include <stddef.h>
#include <stdint.h>

#define BIGINT_LIMB_BITS 64U

/*
------------------------------------------------------------------------------------------------------------------------------
    Integer square-root operations for BigInt.

    The implementation uses Newton iteration and returns the floor of the
    exact square root. Public results are committed only after the temporary
    computation has completed successfully.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal Newton iteration helper.
------------------------------------------------------------------------------------------------------------------------------
*/
static BigIntStatus integer_sqrt_impl( /*Internal Newton iteration helper for integer square root*/
    BigInt *root,
    const BigInt *number
)
{
    if (bigint_is_zero(number))
    {
        return bigint_set_string(root, "0");
    }

    size_t bits = (number->size - 1U) * BIGINT_LIMB_BITS;

    for (uint64_t top = number->limbs[number->size - 1U]; top != 0; top >>= 1U)
    {
        bits++;
    }

    BigInt *next = bigint_create();

    if (next == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    BigIntStatus status = bigint_set_string(root, "1");

    if (status == BIGINT_OK)
    {
        status = bigint_shift_left(root, root, bits / 2U + bits % 2U);
    }

    while (status == BIGINT_OK)
    {
        if (!numforge_budget_check())
        {
            status = BIGINT_OUT_OF_MEMORY;
            break;
        }

        status = bigint_div(next, number, root);

        if (status == BIGINT_OK)
        {
            status = bigint_add(next, next, root);
        }

        if (status == BIGINT_OK)
        {
            status = bigint_shift_right(next, next, 1U);
        }

        if (status != BIGINT_OK || bigint_compare(next, root) >= 0)
        {
            break;
        }

        status = bigint_copy(root, next);
    }

    bigint_destroy(next);
    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Public square-root function.
------------------------------------------------------------------------------------------------------------------------------
*/
BigIntStatus bigint_isqrt( /*Compute the integer square root of a non-negative BigInt, storing the result in result*/
    BigInt *result,
    const BigInt *value
)
{
    if (result == NULL || value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (bigint_is_negative(value))
    {
        return BIGINT_NEGATIVE_ARGUMENT;
    }

    BigInt *temporary = bigint_create();

    if (temporary == NULL)
    {
        return BIGINT_OUT_OF_MEMORY;
    }

    BigIntStatus status = integer_sqrt_impl(temporary, value);

    if (status == BIGINT_OK)
    {
        status = bigint_copy(result, temporary);
    }

    bigint_destroy(temporary);
    return status;
}
