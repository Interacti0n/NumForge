#include "sqrt_internal.h"

static BigRationalStatus integer_status(BigIntStatus status)
{
    switch (status) {
        case BIGINT_OK: return BIGRATIONAL_OK;
        case BIGINT_OUT_OF_MEMORY: return BIGRATIONAL_OUT_OF_MEMORY;
        case BIGINT_NULL_ARGUMENT: return BIGRATIONAL_NULL_ARGUMENT;
        case BIGINT_VALUE_TOO_LARGE: return BIGRATIONAL_VALUE_TOO_LARGE;
        default: return BIGRATIONAL_INVALID_ARGUMENT;
    }
}

BigRationalStatus numforge_rational_sqrt_exact(BigRational *result,
    const BigRational *operand, bool *exact)
{
    if (result == NULL || operand == NULL || exact == NULL) return BIGRATIONAL_NULL_ARGUMENT;
    BigInt *numerator = bigint_create(), *denominator = bigint_create();
    BigInt *top = bigint_create(), *bottom = bigint_create(), *square = bigint_create();
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    bool rational = true;
    if (numerator == NULL || denominator == NULL || top == NULL || bottom == NULL || square == NULL) goto done;
    status = bigrational_get_numerator(numerator, operand);
    if (status == BIGRATIONAL_OK) status = bigrational_get_denominator(denominator, operand);
    if (status == BIGRATIONAL_OK && bigint_is_negative(numerator)) status = BIGRATIONAL_INVALID_ARGUMENT;
    if (status == BIGRATIONAL_OK) status = integer_status(bigint_isqrt(top, numerator));
    if (status == BIGRATIONAL_OK) status = integer_status(bigint_mul(square, top, top));
    if (status == BIGRATIONAL_OK) rational = bigint_compare(square, numerator) == 0;
    if (status == BIGRATIONAL_OK && rational) status = integer_status(bigint_isqrt(bottom, denominator));
    if (status == BIGRATIONAL_OK && rational) status = integer_status(bigint_mul(square, bottom, bottom));
    if (status == BIGRATIONAL_OK && rational) rational = bigint_compare(square, denominator) == 0;
    if (status == BIGRATIONAL_OK && rational) status = bigrational_set_fraction(result, top, bottom);
    if (status == BIGRATIONAL_OK) *exact = rational;
done:
    bigint_destroy(numerator); bigint_destroy(denominator);
    bigint_destroy(top); bigint_destroy(bottom); bigint_destroy(square);
    return status;
}
