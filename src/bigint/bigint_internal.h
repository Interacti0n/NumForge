#ifndef NUMFORGE_BIGINT_INTERNAL_H
#define NUMFORGE_BIGINT_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <numforge/bigint.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    BigInt representation invariants:

    - limbs store the absolute value only.
    - limbs use base 2^64 in little-endian order.
    - size is the number of significant limbs.
    - if size > 0, limbs[size - 1] != 0.
    - size == 0 represents zero.
    - zero is never negative.
    - is_negative applies only to non-zero values.

    Internal magnitude helpers do not modify is_negative. Public arithmetic
    operations are responsible for signed semantics.
------------------------------------------------------------------------------------------------------------------------------
*/

struct BigInt
{
    uint64_t *limbs;
    size_t size;
    size_t capacity;
    bool is_negative;
};

/* Private normalization of a temporary magnitude, in blocks of up to 19
 * decimal zeros. On cancellation the temporary may be partially reduced. */
BigIntStatus bigint_strip_decimal_zeros(BigInt *value, uint64_t *removed);

#endif
