#include <numforge/bigrational.h>

#include "src/core/bigdecimal/bigdecimal_internal.h"
#include "src/runtime/numforge_alloc.h"
#include "src/core/storage.h"
#include "src/runtime/instrumentation/decimal_profile.h"

#include <stdlib.h>
#include <string.h>

/*
 * An exact, reduced fraction. Every public mutation builds a complete
 * temporary and exchanges ownership only after all operations succeed.
 */
struct BigRational
{
    BigInt *numerator;
    BigInt *denominator;
};

size_t bigrational_storage_bytes(const BigRational *value)
{
    return value ? numforge_storage_add(sizeof(*value), numforge_storage_add(
        bigint_storage_bytes(value->numerator), bigint_storage_bytes(value->denominator))) : 0U;
}

static BigRationalStatus rational_from_integer_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK:
            return BIGRATIONAL_OK;
        case BIGINT_NULL_ARGUMENT:
            return BIGRATIONAL_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return BIGRATIONAL_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO:
            return BIGRATIONAL_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE:
            return BIGRATIONAL_VALUE_TOO_LARGE;
        default:
            return BIGRATIONAL_INVALID_ARGUMENT;
    }
}

static BigRationalStatus rational_from_decimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return BIGRATIONAL_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return BIGRATIONAL_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return BIGRATIONAL_OUT_OF_MEMORY;
        case BIGDECIMAL_DIVISION_BY_ZERO:
            return BIGRATIONAL_DIVISION_BY_ZERO;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return BIGRATIONAL_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return BIGRATIONAL_SCALE_OVERFLOW;
        default:
            return BIGRATIONAL_INVALID_ARGUMENT;
    }
}

const char *bigrational_status_to_string(BigRationalStatus status)
{
    switch (status)
    {
        case BIGRATIONAL_OK:
            return "success";
        case BIGRATIONAL_NULL_ARGUMENT:
            return "null argument";
        case BIGRATIONAL_OUT_OF_MEMORY:
            return "out of memory";
        case BIGRATIONAL_INVALID_ARGUMENT:
            return "invalid argument";
        case BIGRATIONAL_DIVISION_BY_ZERO:
            return "division by zero";
        case BIGRATIONAL_VALUE_TOO_LARGE:
            return "value too large";
        case BIGRATIONAL_SCALE_OVERFLOW:
            return "scale overflow";
        default:
            return "unknown status";
    }
}

BigRational *bigrational_create(void)
{
    BigRational *value = numforge_malloc(sizeof(*value));
    if (value == NULL)
    {
        return NULL;
    }
    value->numerator = bigint_create();
    value->denominator = bigint_create();
    if (value->numerator == NULL || value->denominator == NULL ||
        bigint_set_string(value->denominator, "1") != BIGINT_OK)
    {
        bigrational_destroy(value);
        return NULL;
    }
    return value;
}

void bigrational_destroy(BigRational *value)
{
    if (value == NULL)
    {
        return;
    }
    bigint_destroy(value->numerator);
    bigint_destroy(value->denominator);
    free(value);
}

static void rational_commit(BigRational *destination, BigRational *temporary)
{
    BigInt *old_numerator = destination->numerator;
    BigInt *old_denominator = destination->denominator;
    destination->numerator = temporary->numerator;
    destination->denominator = temporary->denominator;
    temporary->numerator = old_numerator;
    temporary->denominator = old_denominator;
    bigrational_destroy(temporary);
}

/* Private, already reduced parts stay detached until both are ready. Moving
 * them cannot fail, including when destination aliases either input. */
static BigRationalStatus commit_parts(BigRational *result, BigInt *top, BigInt *bottom)
{
    if (bigint_is_zero(top)) {
        BigIntStatus status = bigint_set_uint64(bottom, 1U);
        if (status != BIGINT_OK) return rational_from_integer_status(status);
    } else if (bottom->is_negative) {
        top->is_negative = !top->is_negative;
        bottom->is_negative = false;
    }
    if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    bigint_commit(result->numerator, top);
    bigint_commit(result->denominator, bottom);
    return BIGRATIONAL_OK;
}

static BigIntStatus divide_factor(BigInt *out, const BigInt *value, const BigInt *factor)
{
    return bigint_is_one(factor) ? bigint_copy(out, value) : bigint_div(out, value, factor);
}

BigRationalStatus bigrational_copy(BigRational *result, const BigRational *value)
{
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    if (result == value) return BIGRATIONAL_OK;
    BigInt top = {0}, bottom = {0};
    BigRationalStatus status = rational_from_integer_status(bigint_copy(&top, value->numerator));
    if (status == BIGRATIONAL_OK) status = rational_from_integer_status(bigint_copy(&bottom, value->denominator));
    if (status == BIGRATIONAL_OK) status = commit_parts(result, &top, &bottom);
    free(top.limbs); free(bottom.limbs);
    return status;
}

BigRationalStatus bigrational_set_fraction(BigRational *result, const BigInt *numerator, const BigInt *denominator)
{
    if (result == NULL || numerator == NULL || denominator == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    if (bigint_is_zero(denominator))
    {
        return BIGRATIONAL_DIVISION_BY_ZERO;
    }
    if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    BigInt top = {0}, bottom = {0}, factor = {0};
    BigRationalStatus status = BIGRATIONAL_OK;
    if (bigint_is_one(denominator)) {
        status = rational_from_integer_status(bigint_copy(&top, numerator));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(bigint_copy(&bottom, denominator));
    } else if (!bigint_is_zero(numerator)) {
        status = rational_from_integer_status(bigint_gcd(&factor, numerator, denominator));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(divide_factor(&top, numerator, &factor));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(divide_factor(&bottom, denominator, &factor));
    }
    if (status == BIGRATIONAL_OK) status = commit_parts(result, &top, &bottom);
    free(top.limbs); free(bottom.limbs); free(factor.limbs);
    return status;
}

BigRationalStatus bigrational_from_bigint(BigRational *result, const BigInt *value)
{
    BigRational *temporary;
    BigRationalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    temporary = bigrational_create();
    if (temporary == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = rational_from_integer_status(bigint_copy(temporary->numerator, value));
    if (status == BIGRATIONAL_OK)
    {
        rational_commit(result, temporary);
    }
    else
    {
        bigrational_destroy(temporary);
    }
    return status;
}

BigRationalStatus bigrational_from_bigdecimal(BigRational *result, const BigDecimal *value)
{
    BigRational *temporary;
    BigRationalStatus status;
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    temporary = bigrational_create();
    if (temporary == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    status = rational_from_integer_status(bigint_copy(temporary->numerator, value->coefficient));
    if (status == BIGRATIONAL_OK && value->scale > 0)
    {
        status =
            rational_from_decimal_status(bigdecimal_set_power_of_ten(temporary->denominator, (uint64_t)value->scale));
    }
    else if (status == BIGRATIONAL_OK && value->scale < 0)
    {
        status = rational_from_decimal_status(bigdecimal_multiply_power_of_ten(
            temporary->numerator, temporary->numerator, bigdecimal_abs_i64(value->scale)));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = bigrational_set_fraction(result, temporary->numerator, temporary->denominator);
    }
    bigrational_destroy(temporary);
    return status;
}

BigRationalStatus bigrational_get_numerator(BigInt *result, const BigRational *value)
{
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_from_integer_status(bigint_copy(result, value->numerator));
}

BigRationalStatus bigrational_get_denominator(BigInt *result, const BigRational *value)
{
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_from_integer_status(bigint_copy(result, value->denominator));
}

BigRationalStatus bigrational_to_string(const BigRational *value, char **result)
{
    char *numerator;
    char *denominator = NULL;
    char *text;
    size_t first, second = 0U;
    if (value == NULL || result == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    numerator = bigint_to_string(value->numerator);
    if (numerator == NULL)
    {
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    if (!bigint_is_one(value->denominator))
    {
        denominator = bigint_to_string(value->denominator);
        if (denominator == NULL)
        {
            free(numerator);
            return BIGRATIONAL_OUT_OF_MEMORY;
        }
        second = strlen(denominator);
    }
    first = strlen(numerator);
    if (second > SIZE_MAX - 2U || first > SIZE_MAX - second - 2U)
    {
        free(numerator);
        free(denominator);
        return BIGRATIONAL_VALUE_TOO_LARGE;
    }
    text = numforge_malloc(first + second + (denominator == NULL ? 1U : 2U));
    if (text == NULL)
    {
        free(numerator);
        free(denominator);
        return BIGRATIONAL_OUT_OF_MEMORY;
    }
    memcpy(text, numerator, first);
    if (denominator != NULL)
    {
        text[first] = '/';
        memcpy(text + first + 1U, denominator, second);
        text[first + second + 1U] = '\0';
    }
    else
    {
        text[first] = '\0';
    }
    free(numerator);
    free(denominator);
    *result = text;
    return BIGRATIONAL_OK;
}

BigRationalStatus bigrational_to_bigdecimal(BigDecimal *result, const BigRational *value, int64_t digits,
                                            BigDecimalRoundingMode rounding)
{
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    if (digits < 1 || !bigdecimal_valid_rounding(rounding))
    {
        return BIGRATIONAL_INVALID_ARGUMENT;
    }
    return rational_from_decimal_status(bigdecimal_from_reduced_fraction(
        result, value->numerator, value->denominator, digits, rounding));
}

BigRationalStatus bigrational_compare(int *comparison, const BigRational *a, const BigRational *b)
{
    BigInt g = {0}, left = {0}, right = {0}, factor = {0};
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    if (comparison == NULL || a == NULL || b == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    if (a == b || bigint_compare(a->denominator, b->denominator) == 0) {
        *comparison = bigint_compare(a->numerator, b->numerator);
        return BIGRATIONAL_OK;
    }
    if (bigint_is_zero(a->numerator) || bigint_is_zero(b->numerator) ||
        bigint_is_negative(a->numerator) != bigint_is_negative(b->numerator)) {
        *comparison = bigint_compare(a->numerator, b->numerator);
        return BIGRATIONAL_OK;
    }
    status = rational_from_integer_status(bigint_gcd(&g, a->denominator, b->denominator));
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(divide_factor(&factor, b->denominator, &g));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(&left, a->numerator, &factor));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(divide_factor(&factor, a->denominator, &g));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(&right, b->numerator, &factor));
    }
    if (status == BIGRATIONAL_OK)
    {
        *comparison = bigint_compare(&left, &right);
    }
    free(g.limbs); free(left.limbs); free(right.limbs); free(factor.limbs);
    return status;
}

static BigRationalStatus rational_unary(BigRational *result, const BigRational *value, bool negate)
{
    if (result == NULL || value == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    BigInt top = {0}, bottom = {0};
    BigRationalStatus status = rational_from_integer_status(bigint_copy(&top, value->numerator));
    if (status == BIGRATIONAL_OK) status = rational_from_integer_status(bigint_copy(&bottom, value->denominator));
    if (status == BIGRATIONAL_OK) {
        top.is_negative = top.size && (negate ? !top.is_negative : false);
        status = commit_parts(result, &top, &bottom);
    }
    free(top.limbs); free(bottom.limbs);
    return status;
}

BigRationalStatus bigrational_negate(BigRational *result, const BigRational *value)
{
    return rational_unary(result, value, true);
}

BigRationalStatus bigrational_abs(BigRational *result, const BigRational *value)
{
    return rational_unary(result, value, false);
}

static BigRationalStatus rational_sum(BigRational *result, const BigRational *a, const BigRational *b, bool subtract)
{
    if (!result || !a || !b) return BIGRATIONAL_NULL_ARGUMENT;
    if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    if (bigint_is_zero(b->numerator)) return bigrational_copy(result, a);
    if (bigint_is_zero(a->numerator)) return subtract ? bigrational_negate(result, b) : bigrational_copy(result, b);
    BigInt g = {0}, left = {0}, right = {0}, u = {0}, v = {0}, top = {0}, bottom = {0};
    BigRationalStatus status;
    if (bigint_compare(a->denominator, b->denominator) == 0) {
        status = rational_from_integer_status(subtract ? bigint_sub(&top, a->numerator, b->numerator) :
            bigint_add(&top, a->numerator, b->numerator));
        if (status == BIGRATIONAL_OK) status = bigrational_set_fraction(result, &top, a->denominator);
    } else {
        status = rational_from_integer_status(bigint_gcd(&g, a->denominator, b->denominator));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(divide_factor(&u, b->denominator, &g));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(divide_factor(&v, a->denominator, &g));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(bigint_mul(&left, a->numerator, &u));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(bigint_mul(&right, b->numerator, &v));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(subtract ? bigint_sub(&top, &left, &right) : bigint_add(&top, &left, &right));
        /* For reduced a/b and c/d, the only possible remaining common factor
         * of a*(d/g)+c*(b/g) and b*(d/g) divides g=gcd(b,d).
         * Reduce by gcd(top,g), rather than GCD with the full product. */
        if (status == BIGRATIONAL_OK && !bigint_is_one(&g)) status = rational_from_integer_status(bigint_gcd(&g, &top, &g));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(divide_factor(&top, &top, &g));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(divide_factor(&right, b->denominator, &g));
        if (status == BIGRATIONAL_OK) status = rational_from_integer_status(bigint_mul(&bottom, &v, &right));
        if (status == BIGRATIONAL_OK) status = commit_parts(result, &top, &bottom);
    }
    free(g.limbs); free(left.limbs); free(right.limbs); free(u.limbs); free(v.limbs);
    free(top.limbs); free(bottom.limbs);
    return status;
}

BigRationalStatus bigrational_add(BigRational *result, const BigRational *a, const BigRational *b)
{
    return rational_sum(result, a, b, false);
}

BigRationalStatus bigrational_sub(BigRational *result, const BigRational *a, const BigRational *b)
{
    return rational_sum(result, a, b, true);
}

static BigRationalStatus rational_product(BigRational *result, const BigRational *a, const BigRational *b, bool divide)
{
    BigInt g1 = {0}, g2 = {0}, p = {0}, q = {0}, u = {0}, v = {0}, numerator = {0}, denominator = {0};
    const BigInt *top = divide ? b->denominator : b->numerator;
    const BigInt *bottom = divide ? b->numerator : b->denominator;
    BigRationalStatus status = BIGRATIONAL_OUT_OF_MEMORY;
    if (divide && bigint_is_zero(b->numerator))
    {
        return BIGRATIONAL_DIVISION_BY_ZERO;
    }
    if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    if (bigint_is_zero(a->numerator) || (!divide && bigint_is_zero(b->numerator))) {
        status = commit_parts(result, &numerator, &denominator);
        goto done;
    }
    if (bigint_is_one(b->numerator) && bigint_is_one(b->denominator)) return bigrational_copy(result, a);
    if (!divide && bigint_is_one(a->numerator) && bigint_is_one(a->denominator)) return bigrational_copy(result, b);
    status = rational_from_integer_status(bigint_gcd(&g1, a->numerator, bottom));
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_gcd(&g2, top, a->denominator));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(divide_factor(&p, a->numerator, &g1));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(divide_factor(&q, top, &g2));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(divide_factor(&u, a->denominator, &g2));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(divide_factor(&v, bottom, &g1));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(&numerator, &p, &q));
    }
    if (status == BIGRATIONAL_OK)
    {
        status = rational_from_integer_status(bigint_mul(&denominator, &u, &v));
    }
    if (status == BIGRATIONAL_OK)
    {
        /* Cross-cancellation of canonical inputs already proves coprimality. */
        status = commit_parts(result, &numerator, &denominator);
    }
done:
    free(g1.limbs); free(g2.limbs); free(p.limbs); free(q.limbs); free(u.limbs); free(v.limbs);
    free(numerator.limbs); free(denominator.limbs);
    return status;
}

BigRationalStatus bigrational_mul(BigRational *result, const BigRational *a, const BigRational *b)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_product(result, a, b, false);
}

BigRationalStatus bigrational_div(BigRational *result, const BigRational *a, const BigRational *b)
{
    if (result == NULL || a == NULL || b == NULL)
    {
        return BIGRATIONAL_NULL_ARGUMENT;
    }
    return rational_product(result, a, b, true);
}
