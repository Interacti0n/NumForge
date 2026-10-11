#ifndef NUMFORGE_BIGDECIMAL_INTERNAL_H
#define NUMFORGE_BIGDECIMAL_INTERNAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include "../bigint/bigint_internal.h"

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

/* Per-call owned workspace: zero-initialize, clear once after the series.
 * Inputs/outputs must not refer to workspace fields. Output may alias input;
 * only completed, normalized coefficients are published. No shared cache. */
typedef struct BigDecimalWorkspace
{
    BigInt quotient, remainder, aligned, scaled, power, twice, arithmetic;
    BigIntDivisionWorkspace division;
    uint64_t power_exponent;
    bool power_valid;
} BigDecimalWorkspace;
void bigdecimal_workspace_clear(BigDecimalWorkspace *workspace);
/* Private logarithm paths, also exercised directly by the opt-in harness. */
BigDecimalStatus bigdecimal_ln_series_internal(BigDecimal *result,
    const BigDecimal *value, int64_t digits, BigDecimalRoundingMode rounding);
BigDecimalStatus bigdecimal_ln_agm_internal(BigDecimal *result,
    const BigDecimal *value, int64_t digits, int64_t extra, BigDecimalRoundingMode rounding,
    unsigned *expansions, unsigned *iterations, unsigned *attempts);
BigDecimalStatus bigdecimal_mul_significant_workspace(BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits,
    BigDecimalRoundingMode rounding, BigDecimalWorkspace *workspace);
BigDecimalStatus bigdecimal_add_significant_workspace(BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits,
    BigDecimalRoundingMode rounding, BigDecimalWorkspace *workspace);
BigDecimalStatus bigdecimal_round_significant_workspace(BigDecimal *result,
    const BigDecimal *value, int64_t digits, BigDecimalRoundingMode rounding,
    BigDecimalWorkspace *workspace);
BigDecimalStatus bigdecimal_div_significant_workspace(BigDecimal *result,
    const BigDecimal *a, const BigDecimal *b, int64_t digits,
    BigDecimalRoundingMode rounding, BigDecimalWorkspace *workspace);
BigDecimalStatus bigdecimal_align_coefficient_digits(BigInt *aligned, const BigInt *value, uint64_t places);
BigDecimalStatus bigdecimal_significant_target(const BigDecimal *a, const BigDecimal *b,
    int64_t digits, BigInt *aligned, int64_t *target);
BigDecimalStatus bigdecimal_round_quotient_scratch(BigInt *quotient, const BigInt *remainder,
    const BigInt *divisor, bool negative, BigDecimalRoundingMode rounding, BigInt *scratch);

/* Private paired evaluations matching their separate public calls. Outputs
 * must be distinct; either may alias value. Both remain unchanged on error. */
BigDecimalStatus bigdecimal_sinh_cosh_pair(
    BigDecimal *sine, BigDecimal *cosine, const BigDecimal *value,
    int64_t digits, BigDecimalRoundingMode rounding);

BigDecimalStatus bigdecimal_sin_cos_pair(
    BigDecimal *sine, BigDecimal *cosine, const BigDecimal *value,
    int64_t digits, BigDecimalRoundingMode rounding
);

#include "../internal/decimal_profile.h"

#endif
