#ifndef MYSCIENCECALC_BIGDECIMAL_INTERNAL_H
#define MYSCIENCECALC_BIGDECIMAL_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <bigint.h>

struct BigDecimal
{
    BigInt *coefficient;
    int64_t scale;
};

#endif