#ifndef NUMFORGE_BIGDECIMAL_INTERNAL_H
#define NUMFORGE_BIGDECIMAL_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    A BigDecimal represents coefficient * 10^(-scale).

    Canonical representation invariants, enforced after every successful
    mutating public operation:
    - coefficient is owned by the BigDecimal and is never NULL;
    - zero is always stored with scale == 0;
    - a non-zero coefficient is not divisible by 10;
    - scale may be negative, allowing compact values such as 1200 as 12e2.
------------------------------------------------------------------------------------------------------------------------------
*/
struct BigDecimal
{
    BigInt *coefficient;
    int64_t scale;
};

typedef struct BigDecimalExponent
{
    bool negative;
    bool high;
    uint64_t magnitude;
} BigDecimalExponent;

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared helpers used by the private BigDecimal implementation modules.

    These declarations are intentionally kept outside the installed public
    headers. They are implementation details and are not part of NumForge's
    stable public API.

    Implementation: src/bigdecimal/bigdecimal.c
------------------------------------------------------------------------------------------------------------------------------
*/
BigDecimalStatus bigdecimal_from_bigint_status(BigIntStatus status);
uint64_t bigdecimal_abs_i64(int64_t value);
bool bigdecimal_size_add(size_t a, size_t b, size_t *result);
bool bigdecimal_i64_add(int64_t a, int64_t b, int64_t *result);
bool bigdecimal_i64_sub(int64_t a, int64_t b, int64_t *result);
BigDecimalStatus bigdecimal_scale_difference(
    int64_t larger,
    int64_t smaller,
    uint64_t *difference
);
bool bigdecimal_valid_rounding(BigDecimalRoundingMode rounding);
void bigdecimal_commit(BigDecimal *destination, BigDecimal *temporary);
BigDecimalStatus bigdecimal_set_power_of_ten(BigInt *value, uint64_t exponent);
BigDecimalStatus bigdecimal_multiply_power_of_ten(
    BigInt *result,
    const BigInt *value,
    uint64_t exponent
);
BigDecimalStatus bigdecimal_normalize(BigDecimal *value);
BigDecimalStatus bigdecimal_finish(
    BigDecimal *destination,
    BigDecimal *temporary
);
BigDecimalStatus bigdecimal_parse_exponent(const char *text, int64_t *result);
BigDecimalExponent bigdecimal_decimal_exponent(size_t digits, int64_t scale);
int bigdecimal_compare_exponents(BigDecimalExponent a, BigDecimalExponent b);
BigDecimalStatus bigdecimal_round_quotient(
    BigInt *quotient,
    const BigInt *remainder,
    const BigInt *divisor,
    bool negative,
    BigDecimalRoundingMode rounding
);
BigDecimalStatus bigdecimal_round_significant(
    BigDecimal *result,
    const BigDecimal *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

#endif
