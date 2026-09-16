#include "bigint_internal.h"

#include <numforge/bigint.h>

#include <stdbool.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Comparison and inspection functions for BigInt.
------------------------------------------------------------------------------------------------------------------------------
*/

int bigint_compare( /*Compare two BigInts*/
    const BigInt *a,
    const BigInt *b
)
{
    int comparison;

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

    comparison = bigint_compare_abs(a, b);

    if (a->is_negative)
    {
        return -comparison;
    }

    return comparison;
}

bool bigint_is_zero( /*Check if a BigInt is zero*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return false;
    }

    return value->size == 0;
}

bool bigint_is_one( /*Check if a BigInt is one*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return false;
    }

    return value->size == 1 &&
           value->limbs[0] == 1 &&
           !value->is_negative;
}

bool bigint_is_negative( /*Check if a BigInt is negative*/
    const BigInt *value
)
{
    if (value == NULL)
    {
        return false;
    }

    return value->is_negative;
}
