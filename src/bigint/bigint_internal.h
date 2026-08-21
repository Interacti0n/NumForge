#ifndef MYSCIENCECALC_BIGINT_INTERNAL_H
#define MYSCIENCECALC_BIGINT_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * BigInt representation invariants:
 *
 * - limbs store the absolute value only.
 * - limbs are base 2^64, little-endian.
 * - size is the number of significant limbs.
 * - if size > 0, limbs[size - 1] != 0.
 * - size == 0 represents zero.
 * - zero is never negative.
 * - is_negative applies only to non-zero values.
 *
 * Internal magnitude helpers do not modify is_negative.
 * Public arithmetic operations are responsible for signed semantics.
 */

struct BigInt
{
    uint64_t *limbs;
    size_t size;
    size_t capacity;
    bool is_negative;
};

#endif