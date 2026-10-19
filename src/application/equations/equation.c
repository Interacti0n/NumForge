#include "src/application/equations/equation.h"
#include "src/engine/evaluator/exact_functions.h"
#include "src/engine/values/value_internal.h"
#include "src/engine/parser/tokenizer.h"
#include "src/engine/evaluator/functions.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>
#include <string.h>

/* Reject nondeterminism before evaluation, including every registered alias. */
static CalculatorStatus coefficient_input(const char *input, CalculatorError *error)
{
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length]) ++length;
    if (length > CALCULATOR_MAX_INPUT_BYTES) return CALCULATOR_VALUE_TOO_LARGE;
    const char *assignment = memchr(input, '=', length);
    if (assignment) {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, (size_t)(assignment - input));
        return CALCULATOR_INVALID_ARGUMENT;
    }
    CalculatorTokenizer tokenizer;
    CalculatorToken token;
    CalculatorStatus status = calculator_tokenizer_init(&tokenizer, input);
    while (status == CALCULATOR_OK) {
        if (!numforge_budget_check()) return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
        status = calculator_tokenizer_next(&tokenizer, &token, error);
        if (status != CALCULATOR_OK || token.type == CALCULATOR_TOKEN_END) break;
        if (token.type == CALCULATOR_TOKEN_IDENTIFIER) {
            const CalculatorFunction *function = calculator_function_find(token.text, token.length);
            if (function && function->implementation == CALCULATOR_FUNCTION_RANDOM) {
                calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, token.offset);
                return CALCULATOR_INVALID_ARGUMENT;
            }
        }
    }
    return status;
}

CalculatorStatus application_equation_solve_expressions(ApplicationEquationSolution **result,
    const char *const inputs[3], const CalculatorContext *context,
    CalculatorError *error, size_t *failed_coefficient)
{
    calculator_error_clear(error);
    if (failed_coefficient) *failed_coefficient = SIZE_MAX;
    if (!result || !inputs || !inputs[0] || !inputs[1] || !inputs[2] || !context) {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    if (context->division_scale < 1 || context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->angle_unit < CALCULATOR_ANGLE_RADIANS || context->angle_unit > CALCULATOR_ANGLE_DEGREES) {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    CalculatorContext exact_context = *context;
    exact_context.significant_division = true;
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigRational *coefficients[3] = {0};
    ApplicationEquationSolution *solution = NULL;
    for (size_t i = 0; i < 3U && status == CALCULATOR_OK; ++i) {
        CalculatorValue value = {0};
        CalculatorVariable empty[1] = {0};
        status = coefficient_input(inputs[i], error);
        if (status == CALCULATOR_OK)
            status = calculator_compute_value_with_variables(inputs[i], &exact_context,
                NULL, NULL, empty, 0U, &value, error);
        if (status == CALCULATOR_OK && value.quantity) status = CALCULATOR_DIMENSION_ERROR;
        if (status == CALCULATOR_OK && calculator_value_is_complex(&value)) status = CALCULATOR_INVALID_ARGUMENT;
        if (status == CALCULATOR_OK && value.kind != CALCULATOR_VALUE_INTEGER &&
            value.kind != CALCULATOR_VALUE_RATIONAL) status = CALCULATOR_NOT_IMPLEMENTED;
        if (status == CALCULATOR_OK) {
            coefficients[i] = bigrational_create();
            if (!coefficients[i]) status = CALCULATOR_OUT_OF_MEMORY;
            else status = calculator_from_rational_status(value.kind == CALCULATOR_VALUE_INTEGER
                ? bigrational_from_bigint(coefficients[i], value.integer)
                : bigrational_copy(coefficients[i], value.rational));
        }
        calculator_value_destroy(&value);
        if (status != CALCULATOR_OK && failed_coefficient) *failed_coefficient = i;
    }
    if (status == CALCULATOR_OK)
        status = application_equation_solve(&solution, coefficients[0], coefficients[1], coefficients[2], context, error);
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { *result = solution; solution = NULL; }
    else calculator_error_set(error, status, error ? error->offset : 0U);
    application_equation_destroy(solution);
    for (size_t i = 0; i < 3U; ++i) bigrational_destroy(coefficients[i]);
    if (owner) numforge_budget_end();
    return status;
}

void application_equation_destroy(ApplicationEquationSolution *solution)
{ numforge_polynomial_destroy(solution); }

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
    ApplicationEquationSolution *solution = NULL;
    status = calculator_from_rational_status(numforge_polynomial_solve(&solution, a, b, c));
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { *result = solution; solution = NULL; }
    else calculator_error_set(error, status, 0U);
    numforge_polynomial_destroy(solution);
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
    if (numforge_polynomial_kind(solution) != APPLICATION_EQUATION_FINITE || index >= numforge_polynomial_root_count(solution))
        status = CALCULATOR_INVALID_ARGUMENT;
    else if (!numforge_polynomial_rational_offset(solution)) status = CALCULATOR_NOT_IMPLEMENTED;
    if (status != CALCULATOR_OK) { calculator_error_set(error, status, 0U); return status; }
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    BigRationalComplex *value = NULL;
    status = calculator_from_rational_status(numforge_polynomial_root_exact(&value, solution, index));
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { *result = value; value = NULL; }
    else calculator_error_set(error, status, 0U);
    bigrationalcomplex_destroy(value);
    if (owner) numforge_budget_end();
    return status;
}
