#include "src/application/equations/equation.h"
#include "src/engine/parser/expression_internal.h"
#include "src/engine/evaluator/evaluator.h"
#include "src/engine/evaluator/exact_functions.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>
#include <string.h>

typedef struct Polynomial { BigRational *c[3]; } Polynomial;

static void destroy(Polynomial *value)
{
    for (size_t i = 0U; i < 3U; ++i) { bigrational_destroy(value->c[i]); value->c[i] = NULL; }
}
static CalculatorStatus create(Polynomial *value)
{
    for (size_t i = 0U; i < 3U; ++i) {
        value->c[i] = bigrational_create();
        if (!value->c[i]) return CALCULATOR_OUT_OF_MEMORY;
    }
    return CALCULATOR_OK;
}
static CalculatorStatus small(BigRational *value, const char *text)
{
    BigInt *integer = bigint_create();
    CalculatorStatus status = integer ? calculator_from_integer_status(bigint_set_string(integer, text)) : CALCULATOR_OUT_OF_MEMORY;
    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_from_bigint(value, integer));
    bigint_destroy(integer); return status;
}
/* Inspect before evaluating any constant subtree: no hidden RNG or session
 * dependency may execute, even when multiplied by zero or raised to zero. */
static CalculatorStatus inspect(const CalculatorExpression *node, bool *variable, CalculatorError *error)
{
    CalculatorStatus status = CALCULATOR_OK;
    if (!numforge_budget_check()) return calculator_budget_status(CALCULATOR_OUT_OF_MEMORY);
    switch (node->type) {
        case CALCULATOR_EXPRESSION_VARIABLE:
            if (strcmp(node->data.variable.name, "x") != 0) status = CALCULATOR_UNDEFINED_VARIABLE;
            else *variable = true;
            break;
        case CALCULATOR_EXPRESSION_ANSWER: status = CALCULATOR_UNDEFINED_ANSWER; break;
        case CALCULATOR_EXPRESSION_UNARY: return inspect(node->data.unary.operand, variable, error);
        case CALCULATOR_EXPRESSION_POSTFIX: return inspect(node->data.postfix.operand, variable, error);
        case CALCULATOR_EXPRESSION_BINARY:
            status = inspect(node->data.binary.left, variable, error);
            if (status == CALCULATOR_OK) status = inspect(node->data.binary.right, variable, error);
            return status;
        case CALCULATOR_EXPRESSION_CALL:
            if (node->data.call.function->implementation == CALCULATOR_FUNCTION_RANDOM) status = CALCULATOR_INVALID_ARGUMENT;
            else for (size_t i = 0U; i < node->data.call.count && status == CALCULATOR_OK; ++i)
                status = inspect(node->data.call.arguments[i], variable, error);
            if (status != CALCULATOR_OK && error && error->status != CALCULATOR_OK) return status;
            break;
        default: break;
    }
    if (status != CALCULATOR_OK) calculator_error_set(error, status, node->offset);
    return status;
}
static CalculatorStatus constant(BigRational *result, const CalculatorExpression *node,
    const CalculatorContext *context, CalculatorError *error)
{
    CalculatorValue value = {0};
    CalculatorStatus status = calculator_evaluate_parsed_value(node, context, NULL, NULL, &value, error);
    if (status == CALCULATOR_OK && value.quantity) status = CALCULATOR_DIMENSION_ERROR;
    if (status == CALCULATOR_OK && calculator_value_is_complex(&value)) status = CALCULATOR_INVALID_ARGUMENT;
    if (status == CALCULATOR_OK) {
        if (value.kind == CALCULATOR_VALUE_INTEGER)
            status = calculator_from_rational_status(bigrational_from_bigint(result, value.integer));
        else if (value.kind == CALCULATOR_VALUE_RATIONAL)
            status = calculator_from_rational_status(bigrational_copy(result, value.rational));
        else status = CALCULATOR_NOT_IMPLEMENTED;
    }
    calculator_value_destroy(&value); return status;
}
static CalculatorStatus multiply(Polynomial *result, const Polynomial *a, const Polynomial *b)
{
    BigRational *zero = bigrational_create(), *term = bigrational_create();
    CalculatorStatus status = zero && term ? CALCULATOR_OK : CALCULATOR_OUT_OF_MEMORY;
    size_t degrees[2] = {0U, 0U}; const Polynomial *values[] = {a, b};
    for (size_t k = 0U; k < 2U && status == CALCULATOR_OK; ++k)
        for (size_t i = 1U; i < 3U && status == CALCULATOR_OK; ++i) {
            int comparison = 0;
            status = calculator_from_rational_status(bigrational_compare(&comparison, values[k]->c[i], zero));
            if (comparison) degrees[k] = i;
        }
    if (status == CALCULATOR_OK && degrees[0] + degrees[1] > 2U) status = CALCULATOR_NOT_IMPLEMENTED;
    for (size_t i = 0U; i <= degrees[0] && status == CALCULATOR_OK; ++i)
        for (size_t j = 0U; j <= degrees[1] && status == CALCULATOR_OK; ++j) {
            status = calculator_from_rational_status(bigrational_mul(term, a->c[i], b->c[j]));
            if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_add(result->c[i+j], result->c[i+j], term));
        }
    bigrational_destroy(zero); bigrational_destroy(term); return status;
}
static CalculatorStatus polynomial(Polynomial *result, const CalculatorExpression *node,
    const CalculatorContext *context, CalculatorError *error)
{
    bool variable = false;
    CalculatorStatus status = inspect(node, &variable, error);
    if (status != CALCULATOR_OK) return status;
    if (!variable) status = constant(result->c[0], node, context, error);
    else if (node->type == CALCULATOR_EXPRESSION_VARIABLE) status = small(result->c[1], "1");
    else if (node->type == CALCULATOR_EXPRESSION_UNARY) {
        status = polynomial(result, node->data.unary.operand, context, error);
        if (node->data.unary.operation == CALCULATOR_UNARY_MINUS)
            for (size_t i = 0U; i < 3U && status == CALCULATOR_OK; ++i)
                status = calculator_from_rational_status(bigrational_negate(result->c[i], result->c[i]));
    } else if (node->type == CALCULATOR_EXPRESSION_BINARY || node->type == CALCULATOR_EXPRESSION_POSTFIX) {
        Polynomial left = {{0}}, right = {{0}};
        status = create(&left);
        if (status == CALCULATOR_OK) status = create(&right);
        CalculatorBinaryOperator operation = CALCULATOR_BINARY_POWER;
        const CalculatorExpression *first = node->type == CALCULATOR_EXPRESSION_BINARY ? node->data.binary.left : node->data.postfix.operand;
        if (status == CALCULATOR_OK) status = polynomial(&left, first, context, error);
        if (status == CALCULATOR_OK && node->type == CALCULATOR_EXPRESSION_BINARY) {
            operation = node->data.binary.operation;
            status = polynomial(&right, node->data.binary.right, context, error);
        } else if (status == CALCULATOR_OK) {
            if (node->data.postfix.operation != CALCULATOR_POSTFIX_SQUARE) status = CALCULATOR_NOT_IMPLEMENTED;
            else status = small(right.c[0], "2");
        }
        if (status == CALCULATOR_OK && (operation == CALCULATOR_BINARY_ADD || operation == CALCULATOR_BINARY_SUBTRACT))
            for (size_t i = 0U; i < 3U && status == CALCULATOR_OK; ++i)
                status = calculator_from_rational_status(operation == CALCULATOR_BINARY_ADD
                    ? bigrational_add(result->c[i], left.c[i], right.c[i])
                    : bigrational_sub(result->c[i], left.c[i], right.c[i]));
        else if (status == CALCULATOR_OK && operation == CALCULATOR_BINARY_MULTIPLY) status = multiply(result, &left, &right);
        else if (status == CALCULATOR_OK) {
            bool right_variable = false;
            if (node->type == CALCULATOR_EXPRESSION_BINARY) status = inspect(node->data.binary.right, &right_variable, error);
            if (status == CALCULATOR_OK && right_variable) status = CALCULATOR_NOT_IMPLEMENTED;
            if (status == CALCULATOR_OK && operation == CALCULATOR_BINARY_DIVIDE) {
                for (size_t i = 0U; i < 3U && status == CALCULATOR_OK; ++i)
                    status = calculator_from_rational_status(bigrational_div(result->c[i], left.c[i], right.c[0]));
            } else if (status == CALCULATOR_OK) {
                BigRational *exponent = bigrational_create(); int comparison = 1; unsigned power = 3U;
                if (!exponent) status = CALCULATOR_OUT_OF_MEMORY;
                const char *texts[] = {"0", "1", "2"};
                for (unsigned i = 0U; i < 3U && status == CALCULATOR_OK; ++i) {
                    status = small(exponent, texts[i]);
                    if (status == CALCULATOR_OK) status = calculator_from_rational_status(bigrational_compare(&comparison, right.c[0], exponent));
                    if (!comparison) { power = i; break; }
                }
                if (status == CALCULATOR_OK && power == 0U) status = small(result->c[0], "1");
                else if (status == CALCULATOR_OK && power == 1U)
                    for (size_t i = 0U; i < 3U && status == CALCULATOR_OK; ++i) status = calculator_from_rational_status(bigrational_copy(result->c[i], left.c[i]));
                else if (status == CALCULATOR_OK && power == 2U) status = multiply(result, &left, &left);
                else if (status == CALCULATOR_OK) status = CALCULATOR_NOT_IMPLEMENTED;
                bigrational_destroy(exponent);
            }
        }
        destroy(&left); destroy(&right);
    } else status = CALCULATOR_NOT_IMPLEMENTED;
    if (status != CALCULATOR_OK && (!error || error->status == CALCULATOR_OK)) calculator_error_set(error, status, node->offset);
    return status;
}

CalculatorStatus application_equation_solve_text(ApplicationEquationSolution **result,
    const char *input, const CalculatorContext *context, CalculatorError *error)
{
    calculator_error_clear(error);
    if (!result || !input || !context) { calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U); return CALCULATOR_NULL_ARGUMENT; }
    CalculatorStatus status = calculator_context_validate(context, error);
    if (status != CALCULATOR_OK) return status;
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length]) ++length;
    if (length > CALCULATOR_MAX_INPUT_BYTES) { calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, 0U); return CALCULATOR_VALUE_TOO_LARGE; }
    const char *equal = strchr(input, '=');
    if (!equal || strchr(equal+1, '=') || strchr(input, '\n') || strchr(input, '\r')) {
        calculator_error_set(error, CALCULATOR_SYNTAX_ERROR, equal ? (size_t)(equal-input) : length); return CALCULATOR_SYNTAX_ERROR;
    }
    if (context->division_scale < 1 || context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->angle_unit < CALCULATOR_ANGLE_RADIANS || context->angle_unit > CALCULATOR_ANGLE_DEGREES) {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U); return CALCULATOR_INVALID_ARGUMENT;
    }
    CalculatorContext exact = *context; exact.significant_division = true;
    bool owner = numforge_budget_begin((uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    char *copy = numforge_malloc(length+1U);
    CalculatorExpression *tree[2] = {0}; Polynomial sides[2] = {{{0}}, {{0}}};
    ApplicationEquationSolution *solution = NULL;
    if (!copy) status = CALCULATOR_OUT_OF_MEMORY;
    size_t separator = (size_t)(equal-input);
    if (copy) { memcpy(copy, input, length+1U); copy[separator] = '\0'; }
    for (size_t i = 0U; i < 2U && status == CALCULATOR_OK; ++i) {
        status = create(&sides[i]);
        if (status == CALCULATOR_OK) status = calculator_parse_variables(copy + (i ? separator+1U : 0U), &tree[i], error);
        if (status == CALCULATOR_OK) status = polynomial(&sides[i], tree[i], &exact, error);
        if (status != CALCULATOR_OK && i) calculator_error_set(error, status, separator+1U+(error ? error->offset : 0U));
    }
    for (size_t i = 0U; i < 3U && status == CALCULATOR_OK; ++i)
        status = calculator_from_rational_status(bigrational_sub(sides[0].c[i], sides[0].c[i], sides[1].c[i]));
    if (status == CALCULATOR_OK) status = application_equation_solve(&solution, sides[0].c[2], sides[0].c[1], sides[0].c[0], context, error);
    status = calculator_budget_status(status);
    if (status == CALCULATOR_OK) { *result = solution; solution = NULL; }
    else calculator_error_set(error, status, error ? error->offset : 0U);
    for (size_t i = 0U; i < 2U; ++i) { calculator_expression_destroy(tree[i]); destroy(&sides[i]); }
    application_equation_destroy(solution); free(copy);
    if (owner) numforge_budget_end();
    return status;
}
