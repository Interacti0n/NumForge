#include <numforge/engine.h>
#include <numforge/runtime.h>
#include "../calculator/expression_internal.h"
#include "../calculator/evaluator.h"

#include <limits.h>
#include <stdlib.h>

struct NumForgeContext {
    CalculatorContext calculator;
    size_t allocation_bytes;
    size_t single_allocation;
};
struct NumForgeExpression { CalculatorExpression *tree; };
struct NumForgeResult {
    CalculatorValue value;
    NumForgeContext context;
};

static NumForgeContext policy(const NumForgeContext *context)
{
    NumForgeContext value;
    if (context != NULL) return *context;
    calculator_context_init(&value.calculator);
    value.calculator.output_scale = CALCULATOR_UNLIMITED_OUTPUT_SCALE;
    value.allocation_bytes = CALCULATOR_ALLOCATION_BUDGET;
    value.single_allocation = CALCULATOR_SINGLE_ALLOCATION;
    return value;
}

static bool begin(const NumForgeContext *context)
{
    return numforge_budget_begin((uint64_t)context->calculator.time_limit_ms,
        context->allocation_bytes, context->single_allocation);
}

static NumForgeStatus mapped(CalculatorStatus status)
{
    switch (status) {
        case CALCULATOR_OK: return NUMFORGE_OK;
        case CALCULATOR_NULL_ARGUMENT: return NUMFORGE_NULL_ARGUMENT;
        case CALCULATOR_INVALID_ARGUMENT: return NUMFORGE_INVALID_ARGUMENT;
        case CALCULATOR_INVALID_TOKEN: return NUMFORGE_INVALID_TOKEN;
        case CALCULATOR_SYNTAX_ERROR: return NUMFORGE_SYNTAX_ERROR;
        case CALCULATOR_DIVISION_BY_ZERO: return NUMFORGE_DIVISION_BY_ZERO;
        case CALCULATOR_VALUE_TOO_LARGE: return NUMFORGE_VALUE_TOO_LARGE;
        case CALCULATOR_SCALE_OVERFLOW: return NUMFORGE_SCALE_OVERFLOW;
        case CALCULATOR_OUT_OF_MEMORY: return NUMFORGE_OUT_OF_MEMORY;
        case CALCULATOR_TIME_LIMIT: return NUMFORGE_TIME_LIMIT;
        case CALCULATOR_ARGUMENT_COUNT: return NUMFORGE_ARGUMENT_COUNT;
        case CALCULATOR_UNDEFINED_VARIABLE: return NUMFORGE_UNDEFINED_VARIABLE;
        default: return NUMFORGE_UNSUPPORTED;
    }
}

static NumForgeStatus diagnostic(NumForgeStatus status, size_t offset, NumForgeError *error)
{
    if (error != NULL) {
        error->status = status;
        error->offset = status == NUMFORGE_OK || status == NUMFORGE_OUT_OF_MEMORY ||
            status == NUMFORGE_TIME_LIMIT || status == NUMFORGE_MEMORY_LIMIT ? 0U : offset;
    }
    return status;
}

/* Check before the owning scope ends; preserve the caller's sticky failure. */
static NumForgeStatus budget_status(CalculatorStatus status)
{
    (void)numforge_budget_check();
    if (numforge_budget_failure() == NUMFORGE_BUDGET_TIME) return NUMFORGE_TIME_LIMIT;
    if (numforge_budget_failure() == NUMFORGE_BUDGET_MEMORY) return NUMFORGE_MEMORY_LIMIT;
    return mapped(status);
}

NumForgeContext *numforge_context_create(void)
{
    NumForgeContext *context = numforge_malloc(sizeof(*context));
    if (context != NULL) *context = policy(NULL);
    return context;
}
void numforge_context_destroy(NumForgeContext *context) { free(context); }

NumForgeStatus numforge_context_set_precision(NumForgeContext *context, int64_t digits)
{
    if (context == NULL) return NUMFORGE_NULL_ARGUMENT;
    if (digits < 1 || digits > CALCULATOR_MAX_OUTPUT_SCALE) return NUMFORGE_INVALID_ARGUMENT;
    context->calculator.division_scale = digits;
    return NUMFORGE_OK;
}
NumForgeStatus numforge_context_set_rounding(NumForgeContext *context, BigDecimalRoundingMode rounding)
{
    if (context == NULL) return NUMFORGE_NULL_ARGUMENT;
    if (rounding < BIGDECIMAL_ROUND_TOWARD_ZERO || rounding > BIGDECIMAL_ROUND_HALF_EVEN)
        return NUMFORGE_INVALID_ARGUMENT;
    context->calculator.rounding = rounding;
    return NUMFORGE_OK;
}
NumForgeStatus numforge_context_set_limits(NumForgeContext *context, uint64_t milliseconds,
    size_t allocation_bytes, size_t single_allocation)
{
    if (context == NULL) return NUMFORGE_NULL_ARGUMENT;
    if (milliseconds > INT64_MAX) return NUMFORGE_INVALID_ARGUMENT;
    context->calculator.time_limit_ms = (int64_t)milliseconds;
    context->allocation_bytes = allocation_bytes;
    context->single_allocation = single_allocation;
    return NUMFORGE_OK;
}

/* Reject application-only capabilities even if nested in a pure expression. */
static CalculatorStatus validate_tree(const CalculatorExpression *tree, CalculatorError *error)
{
    CalculatorStatus status = CALCULATOR_OK;
    switch (tree->type) {
        case CALCULATOR_EXPRESSION_VARIABLE:
            status = CALCULATOR_UNDEFINED_VARIABLE;
            break;
        case CALCULATOR_EXPRESSION_ANSWER:
            status = CALCULATOR_NOT_IMPLEMENTED;
            break;
        case CALCULATOR_EXPRESSION_UNARY:
            return validate_tree(tree->data.unary.operand, error);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return validate_tree(tree->data.postfix.operand, error);
        case CALCULATOR_EXPRESSION_BINARY:
            status = validate_tree(tree->data.binary.left, error);
            return status == CALCULATOR_OK ? validate_tree(tree->data.binary.right, error) : status;
        case CALCULATOR_EXPRESSION_CALL:
            switch (tree->data.call.function->implementation) {
                case CALCULATOR_FUNCTION_RANDOM:
                case CALCULATOR_FUNCTION_QUANTITY:
                case CALCULATOR_FUNCTION_CONVERT:
                case CALCULATOR_FUNCTION_PENDING:
                    status = CALCULATOR_NOT_IMPLEMENTED;
                    break;
                default:
                    for (size_t i = 0U; i < tree->data.call.count; ++i) {
                        status = validate_tree(tree->data.call.arguments[i], error);
                        if (status != CALCULATOR_OK) return status;
                    }
                    break;
            }
            break;
        default: break;
    }
    if (status != CALCULATOR_OK) calculator_error_set(error, status, tree->offset);
    return status;
}

NumForgeStatus numforge_expression_prepare(NumForgeExpression **result, const char *input,
    const NumForgeContext *context, NumForgeError *error)
{
    if (result == NULL || input == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    NumForgeContext settings = policy(context);
    bool owner = begin(&settings);
    CalculatorError detail = {CALCULATOR_OK, 0U};
    CalculatorExpression *tree = NULL;
    NumForgeExpression *prepared = NULL;
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length] != '\0') ++length;
    CalculatorStatus status = length > CALCULATOR_MAX_INPUT_BYTES ? CALCULATOR_VALUE_TOO_LARGE :
        calculator_parse_variables(input, &tree, &detail);
    if (status == CALCULATOR_OK) status = validate_tree(tree, &detail);
    if (status == CALCULATOR_OK) {
        prepared = numforge_malloc(sizeof(*prepared));
        if (prepared == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    }
    NumForgeStatus final = budget_status(status);
    if (final == NUMFORGE_OK) {
        prepared->tree = tree;
        *result = prepared;
    } else {
        calculator_expression_destroy(tree);
        free(prepared);
    }
    if (owner) numforge_budget_end();
    return diagnostic(final, final == mapped(status) ? detail.offset : 0U, error);
}

void numforge_expression_destroy(NumForgeExpression *expression)
{
    if (expression == NULL) return;
    calculator_expression_destroy(expression->tree);
    free(expression);
}

NumForgeStatus numforge_expression_evaluate(NumForgeResult **result,
    const NumForgeExpression *expression, const NumForgeContext *context, NumForgeError *error)
{
    if (result == NULL || expression == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    NumForgeContext settings = policy(context);
    bool owner = begin(&settings);
    CalculatorError detail = {CALCULATOR_OK, 0U};
    NumForgeResult *value = numforge_calloc(1U, sizeof(*value));
    CalculatorStatus status = value == NULL ? CALCULATOR_OUT_OF_MEMORY :
        calculator_evaluate_parsed_value(expression->tree, &settings.calculator,
            NULL, NULL, &value->value, &detail);
    NumForgeStatus final = budget_status(status);
    if (final == NUMFORGE_OK) {
        value->context = settings;
        *result = value;
    } else numforge_result_destroy(value);
    if (owner) numforge_budget_end();
    return diagnostic(final, final == mapped(status) ? detail.offset : 0U, error);
}

NumForgeStatus numforge_evaluate(NumForgeResult **result, const char *input,
    const NumForgeContext *context, NumForgeError *error)
{
    if (result == NULL || input == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    NumForgeContext settings = policy(context);
    bool owner = begin(&settings);
    NumForgeExpression *expression = NULL;
    NumForgeStatus status = numforge_expression_prepare(&expression, input, &settings, error);
    if (status == NUMFORGE_OK) status = numforge_expression_evaluate(result, expression, &settings, error);
    numforge_expression_destroy(expression);
    if (owner) numforge_budget_end();
    return status;
}

void numforge_result_destroy(NumForgeResult *result)
{
    if (result == NULL) return;
    calculator_value_destroy(&result->value);
    free(result);
}
NumForgeResultKind numforge_result_kind(const NumForgeResult *result)
{
    if (result == NULL) return NUMFORGE_RESULT_INVALID;
    switch (result->value.kind) {
        case CALCULATOR_VALUE_INTEGER: return NUMFORGE_RESULT_INTEGER;
        case CALCULATOR_VALUE_RATIONAL: return NUMFORGE_RESULT_RATIONAL;
        case CALCULATOR_VALUE_DECIMAL: return NUMFORGE_RESULT_DECIMAL;
        case CALCULATOR_VALUE_COMPLEX_RATIONAL: return NUMFORGE_RESULT_COMPLEX_RATIONAL;
        case CALCULATOR_VALUE_COMPLEX_DECIMAL: return NUMFORGE_RESULT_COMPLEX_DECIMAL;
        default: return NUMFORGE_RESULT_INVALID;
    }
}
bool numforge_result_is_exact(const NumForgeResult *result)
{
    NumForgeResultKind kind = numforge_result_kind(result);
    return kind == NUMFORGE_RESULT_INTEGER || kind == NUMFORGE_RESULT_RATIONAL ||
        kind == NUMFORGE_RESULT_COMPLEX_RATIONAL;
}
const BigInt *numforge_result_integer(const NumForgeResult *result)
{
    return numforge_result_kind(result) == NUMFORGE_RESULT_INTEGER ? result->value.integer : NULL;
}
const BigRational *numforge_result_rational(const NumForgeResult *result)
{
    return numforge_result_kind(result) == NUMFORGE_RESULT_RATIONAL ? result->value.rational : NULL;
}
const BigDecimal *numforge_result_decimal(const NumForgeResult *result)
{
    return numforge_result_kind(result) == NUMFORGE_RESULT_DECIMAL ? result->value.number : NULL;
}
const BigRationalComplex *numforge_result_complex_rational(const NumForgeResult *result)
{
    return numforge_result_kind(result) == NUMFORGE_RESULT_COMPLEX_RATIONAL ? result->value.complex_rational : NULL;
}
const BigComplex *numforge_result_complex_decimal(const NumForgeResult *result)
{
    return numforge_result_kind(result) == NUMFORGE_RESULT_COMPLEX_DECIMAL ? result->value.complex_decimal : NULL;
}

NumForgeStatus numforge_result_to_string(const NumForgeResult *result, char **text)
{
    if (result == NULL || text == NULL) return NUMFORGE_NULL_ARGUMENT;
    bool owner = begin(&result->context);
    char *temporary = NULL;
    CalculatorStatus status = calculator_value_snapshot_text(&result->value, &temporary);
    NumForgeStatus final = budget_status(status);
    if (final == NUMFORGE_OK) *text = temporary;
    else free(temporary);
    if (owner) numforge_budget_end();
    return final;
}

const char *numforge_status_to_string(NumForgeStatus status)
{
    switch (status) {
        case NUMFORGE_OK: return "success";
        case NUMFORGE_NULL_ARGUMENT: return "null argument";
        case NUMFORGE_INVALID_ARGUMENT: return "invalid argument";
        case NUMFORGE_INVALID_TOKEN: return "invalid token";
        case NUMFORGE_SYNTAX_ERROR: return "syntax error";
        case NUMFORGE_DIVISION_BY_ZERO: return "division by zero";
        case NUMFORGE_VALUE_TOO_LARGE: return "value too large";
        case NUMFORGE_SCALE_OVERFLOW: return "scale overflow";
        case NUMFORGE_OUT_OF_MEMORY: return "out of memory";
        case NUMFORGE_TIME_LIMIT: return "time limit exceeded";
        case NUMFORGE_MEMORY_LIMIT: return "allocation budget exceeded";
        case NUMFORGE_UNSUPPORTED: return "unsupported capability";
        case NUMFORGE_ARGUMENT_COUNT: return "wrong number of arguments";
        case NUMFORGE_UNDEFINED_VARIABLE: return "undefined variable";
        default: return "unknown status";
    }
}
