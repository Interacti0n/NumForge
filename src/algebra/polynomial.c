#include "polynomial_internal.h"
#include "../bigrational/sqrt_internal.h"
#include <numforge/runtime.h>
#include <stdlib.h>

static BigRationalStatus small_rational(BigRational *result, const char *text)
{
    BigInt *number = bigint_create();
    if (number == NULL) return BIGRATIONAL_OUT_OF_MEMORY;
    BigIntStatus parsed = bigint_set_string(number, text);
    BigRationalStatus status = parsed == BIGINT_OK ? bigrational_from_bigint(result, number) :
        parsed == BIGINT_OUT_OF_MEMORY ? BIGRATIONAL_OUT_OF_MEMORY : BIGRATIONAL_INVALID_ARGUMENT;
    bigint_destroy(number);
    return status;
}

void numforge_polynomial_destroy(NumForgePolynomialSolution *solution)
{
    if (!solution) return;
    bigrational_destroy(solution->center);
    bigrational_destroy(solution->radicand);
    bigrational_destroy(solution->offset);
    bigrational_destroy(solution->discriminant);
    free(solution);
}

static BigRationalStatus root_fields(NumForgePolynomialSolution *solution)
{
    solution->center = bigrational_create();
    solution->radicand = bigrational_create();
    solution->offset = bigrational_create();
    return solution->center && solution->radicand && solution->offset
        ? BIGRATIONAL_OK : BIGRATIONAL_OUT_OF_MEMORY;
}

BigRationalStatus numforge_polynomial_solve(NumForgePolynomialSolution **result,
    const BigRational *a, const BigRational *b, const BigRational *c)
{
    if (result == NULL || a == NULL || b == NULL || c == NULL) return BIGRATIONAL_NULL_ARGUMENT;
    BigRationalStatus status = BIGRATIONAL_OK;
    NumForgePolynomialSolution *solution = numforge_calloc(1U, sizeof(*solution));
    enum { ZERO, NEGATIVE, DENOMINATOR, SQUARE, PRODUCT, FOUR, WORK_COUNT };
    BigRational *work[WORK_COUNT] = {0};
    int a_sign = 0, b_sign = 0, c_sign = 0, discriminant_sign = 0;
    if (!solution) status = BIGRATIONAL_OUT_OF_MEMORY;
    if (status == BIGRATIONAL_OK) {
        work[ZERO] = bigrational_create();
        if (!work[ZERO]) status = BIGRATIONAL_OUT_OF_MEMORY;
    }
    if (status == BIGRATIONAL_OK)
        status = bigrational_compare(&a_sign, a, work[ZERO]);
    if (status == BIGRATIONAL_OK)
        status = bigrational_compare(&b_sign, b, work[ZERO]);
    if (status == BIGRATIONAL_OK && !a_sign && !b_sign) {
        status = bigrational_compare(&c_sign, c, work[ZERO]);
        solution->kind = c_sign ? NUMFORGE_POLYNOMIAL_NONE : NUMFORGE_POLYNOMIAL_ALL;
    } else if (status == BIGRATIONAL_OK) {
        solution->kind = NUMFORGE_POLYNOMIAL_FINITE;
        solution->degree = a_sign ? 2U : 1U;
        status = root_fields(solution);
        if (status == BIGRATIONAL_OK && !a_sign) {
            status = bigrational_negate(solution->center, c);
            if (status == BIGRATIONAL_OK)
                status = bigrational_div(solution->center, solution->center, b);
            solution->count = 1U;
            solution->multiplicity[0] = 1U;
        } else if (status == BIGRATIONAL_OK) {
            for (size_t i = 1U; i < WORK_COUNT && status == BIGRATIONAL_OK; ++i) {
                work[i] = bigrational_create();
                if (!work[i]) status = BIGRATIONAL_OUT_OF_MEMORY;
            }
            solution->discriminant = bigrational_create();
            if (!solution->discriminant) status = BIGRATIONAL_OUT_OF_MEMORY;
            if (status == BIGRATIONAL_OK) status = small_rational(work[FOUR], "4");
            if (status == BIGRATIONAL_OK) status = small_rational(work[DENOMINATOR], "2");
            if (status == BIGRATIONAL_OK)
                status = bigrational_mul(work[SQUARE], b, b);
            if (status == BIGRATIONAL_OK)
                status = bigrational_mul(work[PRODUCT], a, c);
            if (status == BIGRATIONAL_OK)
                status = bigrational_mul(work[PRODUCT], work[PRODUCT], work[FOUR]);
            if (status == BIGRATIONAL_OK)
                status = bigrational_sub(solution->discriminant, work[SQUARE], work[PRODUCT]);
            if (status == BIGRATIONAL_OK)
                status = bigrational_compare(&discriminant_sign, solution->discriminant, work[ZERO]);
            if (status == BIGRATIONAL_OK)
                status = bigrational_negate(work[NEGATIVE], b);
            if (status == BIGRATIONAL_OK)
                status = bigrational_mul(work[DENOMINATOR], work[DENOMINATOR], a);
            if (status == BIGRATIONAL_OK)
                status = bigrational_div(solution->center, work[NEGATIVE], work[DENOMINATOR]);
            if (status == BIGRATIONAL_OK)
                status = bigrational_mul(work[SQUARE], work[DENOMINATOR], work[DENOMINATOR]);
            if (status == BIGRATIONAL_OK)
                status = bigrational_abs(solution->radicand, solution->discriminant);
            if (status == BIGRATIONAL_OK)
                status = bigrational_div(solution->radicand, solution->radicand, work[SQUARE]);
            if (status == BIGRATIONAL_OK) {
                bool exact = false;
                status = numforge_rational_sqrt_exact(solution->offset, solution->radicand, &exact);
                if (status == BIGRATIONAL_OK && !exact) {
                    bigrational_destroy(solution->offset);
                    solution->offset = NULL;
                    status = BIGRATIONAL_OK;
                }
            }
            solution->imaginary = discriminant_sign < 0;
            solution->count = discriminant_sign ? 2U : 1U;
            solution->multiplicity[0] = discriminant_sign ? 1U : 2U;
            solution->multiplicity[1] = discriminant_sign ? 1U : 0U;
        }
    }
    if (!numforge_budget_check()) status = BIGRATIONAL_OUT_OF_MEMORY;
    if (status == BIGRATIONAL_OK) {
        *result = solution;
        solution = NULL;
    }
    for (size_t i = 0; i < WORK_COUNT; ++i) bigrational_destroy(work[i]);
    numforge_polynomial_destroy(solution);
    return status;
}

static BigRationalStatus complex_status(BigComplexStatus status)
{
    switch (status) {
        case BIGCOMPLEX_OK: return BIGRATIONAL_OK;
        case BIGCOMPLEX_OUT_OF_MEMORY: return BIGRATIONAL_OUT_OF_MEMORY;
        case BIGCOMPLEX_VALUE_TOO_LARGE: return BIGRATIONAL_VALUE_TOO_LARGE;
        case BIGCOMPLEX_SCALE_OVERFLOW: return BIGRATIONAL_SCALE_OVERFLOW;
        case BIGCOMPLEX_DIVISION_BY_ZERO: return BIGRATIONAL_DIVISION_BY_ZERO;
        case BIGCOMPLEX_NULL_ARGUMENT: return BIGRATIONAL_NULL_ARGUMENT;
        default: return BIGRATIONAL_INVALID_ARGUMENT;
    }
}

BigRationalStatus numforge_polynomial_root_exact(BigRationalComplex **result,
    const NumForgePolynomialSolution *solution, size_t index)
{
    if (result == NULL || solution == NULL) return BIGRATIONAL_NULL_ARGUMENT;
    if (solution->kind != NUMFORGE_POLYNOMIAL_FINITE || index >= solution->count || solution->offset == NULL)
        return BIGRATIONAL_INVALID_ARGUMENT;
    BigRationalStatus status = BIGRATIONAL_OK;
    BigRational *real = bigrational_create(), *imaginary = bigrational_create();
    BigRationalComplex *value = bigrationalcomplex_create();
    if (!real || !imaginary || !value) status = BIGRATIONAL_OUT_OF_MEMORY;
    if (status == BIGRATIONAL_OK && solution->imaginary) {
        status = bigrational_copy(real, solution->center);
        if (status == BIGRATIONAL_OK)
            status = index
                ? bigrational_copy(imaginary, solution->offset)
                : bigrational_negate(imaginary, solution->offset);
    } else if (status == BIGRATIONAL_OK) {
        status = index
            ? bigrational_add(real, solution->center, solution->offset)
            : bigrational_sub(real, solution->center, solution->offset);
    }
    if (status == BIGRATIONAL_OK)
        status = complex_status(bigrationalcomplex_set_parts(value, real, imaginary));
    if (!numforge_budget_check()) status = BIGRATIONAL_OUT_OF_MEMORY;
    if (status == BIGRATIONAL_OK) { *result = value; value = NULL; }
    bigrationalcomplex_destroy(value);
    bigrational_destroy(real);
    bigrational_destroy(imaginary);
    return status;
}

NumForgePolynomialKind numforge_polynomial_kind(const NumForgePolynomialSolution *s)
{ return s == NULL ? NUMFORGE_POLYNOMIAL_INVALID : s->kind; }
unsigned numforge_polynomial_degree(const NumForgePolynomialSolution *s)
{ return s == NULL ? 0U : s->degree; }
size_t numforge_polynomial_root_count(const NumForgePolynomialSolution *s)
{ return s == NULL ? 0U : s->count; }
unsigned numforge_polynomial_root_multiplicity(const NumForgePolynomialSolution *s, size_t index)
{ return s == NULL || index >= s->count ? 0U : s->multiplicity[index]; }
bool numforge_polynomial_is_imaginary(const NumForgePolynomialSolution *s)
{ return s != NULL && s->imaginary; }
const BigRational *numforge_polynomial_center(const NumForgePolynomialSolution *s)
{ return s == NULL ? NULL : s->center; }
const BigRational *numforge_polynomial_radicand(const NumForgePolynomialSolution *s)
{ return s == NULL ? NULL : s->radicand; }
const BigRational *numforge_polynomial_rational_offset(const NumForgePolynomialSolution *s)
{ return s == NULL ? NULL : s->offset; }
const BigRational *numforge_polynomial_discriminant(const NumForgePolynomialSolution *s)
{ return s == NULL ? NULL : s->discriminant; }
