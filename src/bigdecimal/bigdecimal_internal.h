#ifndef NUMFORGE_BIGDECIMAL_INTERNAL_H
#define NUMFORGE_BIGDECIMAL_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <numforge/bigint.h>

/*
 * A BigDecimal represents coefficient * 10^(-scale).
 *
 * Invariants enforced by the future normalization routine:
 * - coefficient is owned by the BigDecimal and is never NULL;
 * - zero is always stored with scale == 0;
 * - a non-zero coefficient is not divisible by 10;
 * - scale may be negative, allowing compact values such as 1200 as 12e2.
 */
struct BigDecimal
{
    BigInt *coefficient;
    int64_t scale;
};

#endif
