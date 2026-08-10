#ifndef MYSCIENCECALC_BIGINT_INTERNAL_H
#define MYSCIENCECALC_BIGINT_INTERNAL_H

#include <stdint.h>
#include <stddef.h>

struct BigInt
{
    uint64_t *limbs;
    size_t size;
    size_t capacity;
};

#endif