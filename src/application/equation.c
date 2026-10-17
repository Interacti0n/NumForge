#include "equation.h"
#include "exact_functions.h"
#include "value_internal.h"
#include "../internal/numforge_alloc.h"
#include <stdlib.h>

static CalculatorStatus small_rational(BigRational *result, const char *text)
{
    BigDecimal *number = bigdecimal_create();
    if (!number) return CALCULATOR_OUT_OF_MEMORY;
    CalculatorStatus status = calculator_from_decimal_status(bigdecimal_set_string(number, text));
    if (status == CALCULATOR_OK)
        status = calculator_from_rational_status(bigrational_from_bigdecimal(result, number));
    bigdecimal_destroy(number);
    return status;
}

void application_equation_destroy(ApplicationEquationSolution *solution)
{
    if (!solution) return;
    bigrational_destroy(solution->center);
    bigrational_destroy(solution->radicand);
    bigrational_destroy(solution->offset);
    bigrational_destroy(solution->discriminant);
    free(solution);
}

static CalculatorStatus root_fields(ApplicationEquationSolution *solution)
{
    solution->center = bigrational_create();
    solution->radicand = bigrational_create();
    solution->offset = bigrational_create();
    return solution->center && solution->radicand && solution->offset
        ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
}

CalculatorStatus application_equation_solve(ApplicationEquationSolution **result,
    const BigRational *a, const BigRational *b, const BigRational *c,
    const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (!result || !a || !b || !c || !context) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationEquationSolution *solution = numforge_calloc(1U, sizeof(*solution));
    enum { ZERO, NEGATIVE, DENOMINATOR, SQUARE, PRODUCT, FOUR, WORK_COUNT };
    BigRational *work[WORK_COUNT] = {0};
    int a_sign = 0, b_sign = 0, c_sign = 0, discriminant_sign = 0;
    if (!solution) status = CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK) {
        work[ZERO] = bigrational_create();
        if (!work[ZERO]) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
        status = calculator_from_rational_status(bigrational_compare(&a_sign, a, work[ZERO]));
    if (status == CALCULATOR_OK)
        status = calculator_from_rational_status(bigrational_compare(&b_sign, b, work[ZERO]));
    if (status == CALCULATOR_OK && !a_sign && !b_sign) {
        status = calculator_from_rational_status(bigrational_compare(&c_sign, c, work[ZERO]));
        solution->kind = c_sign ? APPLICATION_EQUATION_NONE : APPLICATION_EQUATION_ALL;
    } else if (status == CALCULATOR_OK) {
        solution->kind = APPLICATION_EQUATION_FINITE;
        solution->degree = a_sign ? 2U : 1U;
        status = root_fields(solution);
        if (status == CALCULATOR_OK && !a_sign) {
            status = calculator_from_rational_status(bigrational_negate(solution->center, c));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_div(solution->center, solution->center, b));
            solution->count = 1U;
            solution->multiplicity[0] = 1U;
        } else if (status == CALCULATOR_OK) {
            for (size_t i = 1U; i < WORK_COUNT && status == CALCULATOR_OK; ++i) {
                work[i] = bigrational_create();
                if (!work[i]) status = CALCULATOR_OUT_OF_MEMORY;
            }
            solution->discriminant = bigrational_create();
            if (!solution->discriminant) status = CALCULATOR_OUT_OF_MEMORY;
            if (status == CALCULATOR_OK) status = small_rational(work[FOUR], "4");
            if (status == CALCULATOR_OK) status = small_rational(work[DENOMINATOR], "2");
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_mul(work[SQUARE], b, b));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_mul(work[PRODUCT], a, c));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_mul(work[PRODUCT], work[PRODUCT], work[FOUR]));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_sub(solution->discriminant, work[SQUARE], work[PRODUCT]));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_compare(&discriminant_sign, solution->discriminant, work[ZERO]));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_negate(work[NEGATIVE], b));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_mul(work[DENOMINATOR], work[DENOMINATOR], a));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_div(solution->center, work[NEGATIVE], work[DENOMINATOR]));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_mul(work[SQUARE], work[DENOMINATOR], work[DENOMINATOR]));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_abs(solution->radicand, solution->discriminant));
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_div(solution->radicand, solution->radicand, work[SQUARE]));
            if (status == CALCULATOR_OK) {
                status = calculator_exact_sqrt(solution->offset, solution->radicand);
                if (status == CALCULATOR_NOT_IMPLEMENTED) {
                    bigrational_destroy(solution->offset);
                    solution->offset = NULL;
                    status = CALCULATOR_OK;
                }
            }
            solution->imaginary = discriminant_sign < 0;
            solution->count = discriminant_sign ? 2U : 1U;
            solution->multiplicity[0] = discriminant_sign ? 1U : 2U;
            solution->multiplicity[1] = discriminant_sign ? 1U : 0U;
        }
    }
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) {
        *result = solution;
        solution = NULL;
    } else calculator_error_set(error, status, 0U);
    for (size_t i = 0; i < WORK_COUNT; ++i) bigrational_destroy(work[i]);
    application_equation_destroy(solution);
    if (owner) numforge_budget_end();
    return status;
}

CalculatorStatus application_equation_root_exact(BigRationalComplex **result,
    const ApplicationEquationSolution *solution, size_t index,
    const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (!result || !solution || !context) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    if (solution->kind != APPLICATION_EQUATION_FINITE || index >= solution->count) {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (!solution->offset) {
        calculator_error_set(error, CALCULATOR_NOT_IMPLEMENTED, 0U);
        return CALCULATOR_NOT_IMPLEMENTED;
    }
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigRational *real = bigrational_create(), *imaginary = bigrational_create();
    BigRationalComplex *value = bigrationalcomplex_create();
    if (!real || !imaginary || !value) status = CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK && solution->imaginary) {
        status = calculator_from_rational_status(bigrational_copy(real, solution->center));
        if (status == CALCULATOR_OK)
            status = calculator_from_rational_status(index
                ? bigrational_copy(imaginary, solution->offset)
                : bigrational_negate(imaginary, solution->offset));
    } else if (status == CALCULATOR_OK) {
        status = calculator_from_rational_status(index
            ? bigrational_add(real, solution->center, solution->offset)
            : bigrational_sub(real, solution->center, solution->offset));
    }
    if (status == CALCULATOR_OK)
        status = calculator_from_complex_status(bigrationalcomplex_set_parts(value, real, imaginary));
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { *result = value; value = NULL; }
    else calculator_error_set(error, status, 0U);
    bigrationalcomplex_destroy(value);
    bigrational_destroy(real);
    bigrational_destroy(imaginary);
    if (owner) numforge_budget_end();
    return status;
}
