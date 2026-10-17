#include "equation.h"
#include "exact_functions.h"
#include "value_internal.h"
#include "../internal/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void application_equation_root_text_destroy(ApplicationEquationRootText *text)
{
    if (!text) return;
    free(text->exact); free(text->approximation); free(text);
}

static CalculatorStatus radical_text(char **result, const ApplicationEquationSolution *solution, size_t index)
{
    char *center = NULL, *radicand = NULL;
    CalculatorStatus status = calculator_from_rational_status(bigrational_to_string(solution->center, &center));
    if (status == CALCULATOR_OK)
        status = calculator_from_rational_status(bigrational_to_string(solution->radicand, &radicand));
    if (status == CALCULATOR_OK) {
        bool zero = strcmp(center, "0") == 0;
        const char *prefix = zero ? (index ? "" : "-") : (index ? " + " : " - ");
        size_t size = strlen(center) + strlen(radicand) + 24U;
        if (size > CALCULATOR_MAX_OUTPUT_BYTES) status = CALCULATOR_VALUE_TOO_LARGE;
        else {
            *result = numforge_malloc(size);
            if (!*result) status = CALCULATOR_OUT_OF_MEMORY;
            else (void)snprintf(*result, size, "%s%s%ssqrt(%s)", zero ? "" : center,
                prefix, solution->imaginary ? "i*" : "", radicand);
        }
    }
    free(center); free(radicand);
    return status;
}

CalculatorStatus application_equation_format_root(ApplicationEquationRootText **result,
    const ApplicationEquationSolution *solution, size_t index,
    const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (!result || !solution || !context) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U); return CALCULATOR_NULL_ARGUMENT;
    }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    if (solution->kind != APPLICATION_EQUATION_FINITE || index >= solution->count ||
        context->division_scale < 1 || context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE) {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U); return CALCULATOR_INVALID_ARGUMENT;
    }
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    ApplicationEquationRootText *text = numforge_calloc(1U, sizeof(*text));
    BigRationalComplex *exact = NULL;
    BigComplex *preview = NULL;
    enum { REAL, IMAGINARY, CENTER, RADICAL, STABLE, PRODUCT, ONE, DECIMALS };
    BigDecimal *decimal[DECIMALS] = {0};
    BigRational *product = NULL, *zero = NULL;
    int sign = 0;
    int64_t digits = context->division_scale, working = digits + 24;
    if (!text) status = CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK && solution->offset) {
        status = application_equation_root_exact(&exact, solution, index, context, error);
        if (status == CALCULATOR_OK)
            status = calculator_from_complex_status(bigrationalcomplex_to_string(exact, CALCULATOR_MAX_OUTPUT_BYTES, &text->exact));
    } else if (status == CALCULATOR_OK) status = radical_text(&text->exact, solution, index);
    for (size_t i = 0U; i < DECIMALS && status == CALCULATOR_OK; ++i) {
        decimal[i] = bigdecimal_create();
        if (!decimal[i]) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK) {
        preview = bigcomplex_create();
        if (!preview) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK && exact) {
        status = calculator_from_complex_status(bigrationalcomplex_to_bigcomplex(preview, exact, working, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status == CALCULATOR_OK) status = calculator_from_complex_status(bigcomplex_get_real(decimal[REAL], preview));
        if (status == CALCULATOR_OK) status = calculator_from_complex_status(bigcomplex_get_imaginary(decimal[IMAGINARY], preview));
    } else if (status == CALCULATOR_OK) {
        status = calculator_from_rational_status(bigrational_to_bigdecimal(decimal[CENTER], solution->center, working, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status == CALCULATOR_OK)
            status = calculator_from_rational_status(bigrational_to_bigdecimal(decimal[RADICAL], solution->radicand, working, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status == CALCULATOR_OK)
            status = calculator_from_decimal_status(bigdecimal_sqrt(decimal[RADICAL], decimal[RADICAL], working, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status == CALCULATOR_OK && solution->imaginary) {
            status = calculator_from_decimal_status(bigdecimal_copy(decimal[REAL], decimal[CENTER]));
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(index
                ? bigdecimal_copy(decimal[IMAGINARY], decimal[RADICAL])
                : bigdecimal_negate(decimal[IMAGINARY], decimal[RADICAL]));
        } else if (status == CALCULATOR_OK) {
            product = bigrational_create(); zero = bigrational_create();
            if (!product || !zero) status = CALCULATOR_OUT_OF_MEMORY;
            if (status == CALCULATOR_OK)
                status = calculator_from_rational_status(bigrational_compare(&sign, solution->center, zero));
            /* Choose the root with no cancellation, then recover its partner
             * from the exact rational product instead of center - radical. */
            if (status == CALCULATOR_OK) status = calculator_from_decimal_status(sign < 0
                ? bigdecimal_sub(decimal[STABLE], decimal[CENTER], decimal[RADICAL])
                : bigdecimal_add(decimal[STABLE], decimal[CENTER], decimal[RADICAL]));
            if (status == CALCULATOR_OK && ((sign < 0 && index == 0U) || (sign >= 0 && index == 1U)))
                status = calculator_from_decimal_status(bigdecimal_copy(decimal[REAL], decimal[STABLE]));
            else if (status == CALCULATOR_OK) {
                status = calculator_from_rational_status(bigrational_mul(product, solution->center, solution->center));
                if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_sub(product, product, solution->radicand));
                if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_to_bigdecimal(decimal[PRODUCT], product, working, BIGDECIMAL_ROUND_HALF_EVEN));
                if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_div_significant(decimal[REAL], decimal[PRODUCT], decimal[STABLE], working, BIGDECIMAL_ROUND_HALF_EVEN));
            }
        }
    }
    if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_set_string(decimal[ONE], "1"));
    for (size_t i = REAL; i <= IMAGINARY && status == CALCULATOR_OK; ++i)
        status = calculator_from_decimal_status(bigdecimal_div_significant(decimal[i], decimal[i], decimal[ONE], digits, BIGDECIMAL_ROUND_HALF_EVEN));
    if (status == CALCULATOR_OK) status = calculator_from_complex_status(bigcomplex_set_parts(preview, decimal[REAL], decimal[IMAGINARY]));
    if (status == CALCULATOR_OK) status = calculator_from_complex_status(bigcomplex_format(preview, -1,
        BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, CALCULATOR_MAX_OUTPUT_BYTES, &text->approximation));
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { *result = text; text = NULL; }
    else calculator_error_set(error, status, 0U);
    application_equation_root_text_destroy(text);
    bigrationalcomplex_destroy(exact); bigcomplex_destroy(preview);
    bigrational_destroy(product); bigrational_destroy(zero);
    for (size_t i = 0U; i < DECIMALS; ++i) bigdecimal_destroy(decimal[i]);
    if (owner) numforge_budget_end();
    return status;
}
