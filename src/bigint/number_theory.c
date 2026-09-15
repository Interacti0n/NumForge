#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
#include <numforge/bigint.h>

#include <stdint.h>
#include <stdlib.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Number-theory functions for BigInt.

    GCD, LCM, and factorial are kept together because they compose the public
    arithmetic API while sharing the same ownership and budget conventions.
------------------------------------------------------------------------------------------------------------------------------
*/

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
    BigInt temporary;
    BigIntStatus status;
    uint64_t n;

    if (result == NULL || value == NULL)
    {
        return BIGINT_NULL_ARGUMENT;
    }

    if (value->is_negative)
    {
        return BIGINT_NEGATIVE_ARGUMENT; // factorial is undefined for negative numbers
    }

    if (value->size > 1 ||
        (value->size == 1 && value->limbs[0] > BIGINT_FACTORIAL_MAX_N))
    {
        return BIGINT_VALUE_TOO_LARGE;
    }

    n = value->size == 0 ? 0U : value->limbs[0];
    temporary.limbs = NULL;
    temporary.size = 0U;
    temporary.capacity = 0U;
    temporary.is_negative = false;
    status = bigint_set_uint64(&temporary, 1U);

    if (status == BIGINT_OK && n >= 2U)
    {
        uint64_t i = 2U;

        for (;;)
        {
            if (!numforge_budget_check())
            {
                status = BIGINT_OUT_OF_MEMORY;
                break;
            }
            status = bigint_multiply_by_uint64(&temporary, i);

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
        temporary.is_negative = false;
        bigint_commit(result, &temporary);
    }

    free(temporary.limbs);
    return status;
}
