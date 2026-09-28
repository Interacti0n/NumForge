#ifndef NUMFORGE_BIGRATIONAL_H
#define NUMFORGE_BIGRATIONAL_H

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Exact fraction. Storage is private; use create/destroy for its lifetime.
 * Every value is reduced with a positive denominator. Zero is stored as 0/1.
 * A denominator of one remains representable by this standalone type; the
 * string conversion writes the integer without a /1 suffix. */
typedef struct BigRational BigRational;

typedef enum BigRationalStatus
{
    BIGRATIONAL_OK = 0,
    BIGRATIONAL_NULL_ARGUMENT,
    BIGRATIONAL_OUT_OF_MEMORY,
    BIGRATIONAL_INVALID_ARGUMENT,
    BIGRATIONAL_DIVISION_BY_ZERO,
    BIGRATIONAL_VALUE_TOO_LARGE,
    BIGRATIONAL_SCALE_OVERFLOW
} BigRationalStatus;

const char *bigrational_status_to_string(BigRationalStatus status);

/* create returns an owned zero, destroy accepts NULL. Mutating operations
 * preserve their outputs on failure and support output/input aliasing. */
BigRational *bigrational_create(void);
void bigrational_destroy(BigRational *value);
BigRationalStatus bigrational_copy(BigRational *result, const BigRational *value);
BigRationalStatus bigrational_set_fraction(
    BigRational *result,
    const BigInt *numerator,
    const BigInt *denominator
);
BigRationalStatus bigrational_from_bigint(BigRational *result, const BigInt *value);
/* Converts the stored finite decimal exactly. It does not infer whether the
 * decimal was originally an approximation of another mathematical value. */
BigRationalStatus bigrational_from_bigdecimal(BigRational *result, const BigDecimal *value);

/* Writes owned copies into initialized BigInt destinations. */
BigRationalStatus bigrational_get_numerator(BigInt *result, const BigRational *value);
BigRationalStatus bigrational_get_denominator(BigInt *result, const BigRational *value);
/* The caller frees *result with free(). On failure *result is unchanged. */
BigRationalStatus bigrational_to_string(const BigRational *value, char **result);
/* digits is a positive significant-digit count. A terminating quotient stays
 * exact; only a recurring quotient is rounded. The client supplies its current
 * working precision, then may apply final display rounding separately. */
BigRationalStatus bigrational_to_bigdecimal(
    BigDecimal *result,
    const BigRational *value,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

BigRationalStatus bigrational_compare(int *comparison, const BigRational *a, const BigRational *b);
BigRationalStatus bigrational_negate(BigRational *result, const BigRational *value);
BigRationalStatus bigrational_abs(BigRational *result, const BigRational *value);
BigRationalStatus bigrational_add(
    BigRational *result, const BigRational *a, const BigRational *b);
BigRationalStatus bigrational_sub(
    BigRational *result, const BigRational *a, const BigRational *b);
BigRationalStatus bigrational_mul(
    BigRational *result, const BigRational *a, const BigRational *b);
BigRationalStatus bigrational_div(
    BigRational *result, const BigRational *a, const BigRational *b);

#ifdef __cplusplus
}
#endif

#endif
