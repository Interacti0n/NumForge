#include <numforge/engine.h>
#include <numforge/runtime.h>
#include "../calculator/expression_internal.h"
#include "../calculator/evaluator.h"
#include "../calculator/exact_functions.h"
#include "../calculator/value_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct NumForgeContext {
    CalculatorContext calculator;
    size_t allocation_bytes;
    size_t single_allocation;
};
struct NumForgeExpression {
    CalculatorExpression *tree;
    size_t parameter_count;
    char parameters[][CALCULATOR_VARIABLE_NAME_BYTES + 1U];
};
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
static CalculatorStatus validate_tree(CalculatorExpression *tree,
    const NumForgeExpression *prepared, const NumForgeResult *const *arguments, CalculatorError *error)
{
    CalculatorStatus status = CALCULATOR_OK;
    switch (tree->type) {
        case CALCULATOR_EXPRESSION_VARIABLE:
            status = CALCULATOR_UNDEFINED_VARIABLE;
            tree->data.variable.value = NULL;
            for (size_t i = 0U; i < prepared->parameter_count; ++i) {
                if (strcmp(tree->data.variable.name, prepared->parameters[i]) == 0) {
                    if (arguments != NULL) tree->data.variable.value = &arguments[i]->value;
                    return CALCULATOR_OK;
                }
            }
            break;
        case CALCULATOR_EXPRESSION_ANSWER:
            status = CALCULATOR_NOT_IMPLEMENTED;
            break;
        case CALCULATOR_EXPRESSION_UNARY:
            return validate_tree(tree->data.unary.operand, prepared, arguments, error);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return validate_tree(tree->data.postfix.operand, prepared, arguments, error);
        case CALCULATOR_EXPRESSION_BINARY:
            status = validate_tree(tree->data.binary.left, prepared, arguments, error);
            return status == CALCULATOR_OK ? validate_tree(tree->data.binary.right, prepared, arguments, error) : status;
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
                        status = validate_tree(tree->data.call.arguments[i], prepared, arguments, error);
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
    return numforge_expression_prepare_parameters(result, input, NULL, 0U, context, error);
}

static bool valid_parameter(const char *name)
{
    size_t length = 0U;
    while (length <= CALCULATOR_VARIABLE_NAME_BYTES && name[length] != '\0') ++length;
    if (length == 0U || length > CALCULATOR_VARIABLE_NAME_BYTES) return false;
    for (size_t i = 0U; i < length; ++i)
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z'))) return false;
    CalculatorConstant constant;
    return strcmp(name, "i") != 0 && strcmp(name, "ans") != 0 &&
        calculator_function_find(name, length) == NULL &&
        !calculator_constant_from_text(name, length, &constant);
}

NumForgeStatus numforge_expression_prepare_parameters(NumForgeExpression **result,
    const char *input, const char *const *names, size_t count,
    const NumForgeContext *context, NumForgeError *error)
{
    if (result == NULL || input == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    if (count > CALCULATOR_VARIABLE_CAPACITY) return diagnostic(NUMFORGE_INVALID_ARGUMENT, 0U, error);
    if (count != 0U && names == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    for (size_t i = 0U; i < count; ++i) {
        if (names[i] == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
        if (!valid_parameter(names[i])) return diagnostic(NUMFORGE_INVALID_ARGUMENT, 0U, error);
        for (size_t j = 0U; j < i; ++j)
            if (strcmp(names[i], names[j]) == 0) return diagnostic(NUMFORGE_INVALID_ARGUMENT, 0U, error);
    }
    NumForgeContext settings = policy(context);
    bool owner = begin(&settings);
    CalculatorError detail = {CALCULATOR_OK, 0U};
    CalculatorExpression *tree = NULL;
    NumForgeExpression *prepared = NULL;
    size_t length = 0U;
    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length] != '\0') ++length;
    CalculatorStatus status = length > CALCULATOR_MAX_INPUT_BYTES ? CALCULATOR_VALUE_TOO_LARGE :
        calculator_parse_variables(input, &tree, &detail);
    if (status == CALCULATOR_OK) {
        prepared = numforge_malloc(sizeof(*prepared) + count * sizeof(prepared->parameters[0]));
        if (prepared == NULL) status = CALCULATOR_OUT_OF_MEMORY;
        else {
            prepared->parameter_count = count;
            for (size_t i = 0U; i < count; ++i)
                memcpy(prepared->parameters[i], names[i], strlen(names[i]) + 1U);
            status = validate_tree(tree, prepared, NULL, &detail);
        }
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

size_t numforge_expression_parameter_count(const NumForgeExpression *expression)
{
    return expression == NULL ? 0U : expression->parameter_count;
}
const char *numforge_expression_parameter_name(const NumForgeExpression *expression, size_t index)
{
    return expression != NULL && index < expression->parameter_count ? expression->parameters[index] : NULL;
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
    return numforge_expression_evaluate_arguments(result, expression, NULL, 0U, context, error);
}

NumForgeStatus numforge_expression_evaluate_arguments(NumForgeResult **result,
    const NumForgeExpression *expression, const NumForgeResult *const *arguments,
    size_t count, const NumForgeContext *context, NumForgeError *error)
{
    if (result == NULL || expression == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    if (count != expression->parameter_count) return diagnostic(NUMFORGE_ARGUMENT_COUNT, 0U, error);
    if (count != 0U && arguments == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    for (size_t i = 0U; i < count; ++i)
        if (arguments[i] == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    NumForgeContext settings = policy(context);
    bool owner = begin(&settings);
    CalculatorError detail = {CALCULATOR_OK, 0U};
    NumForgeResult *value = numforge_calloc(1U, sizeof(*value));
    CalculatorStatus status = value == NULL ? CALCULATOR_OUT_OF_MEMORY :
        validate_tree(expression->tree, expression, arguments, &detail);
    if (status == CALCULATOR_OK)
        status = calculator_evaluate_parsed_value(expression->tree, &settings.calculator,
            NULL, NULL, &value->value, &detail);
    /* No caller-owned values survive this operation, including failed evaluations. */
    if (count != 0U) (void)validate_tree(expression->tree, expression, NULL, NULL);
    NumForgeStatus final = budget_status(status);
    if (final == NUMFORGE_OK) {
        value->context = settings;
        *result = value;
    } else numforge_result_destroy(value);
    if (owner) numforge_budget_end();
    return diagnostic(final, final == mapped(status) ? detail.offset : 0U, error);
}

static NumForgeStatus copy_argument(NumForgeResult **result, const void *input,
    CalculatorValueKind kind, const NumForgeContext *context, NumForgeError *error)
{
    if (result == NULL || input == NULL) return diagnostic(NUMFORGE_NULL_ARGUMENT, 0U, error);
    NumForgeContext settings = policy(context);
    bool owner = begin(&settings);
    NumForgeResult *copy = numforge_calloc(1U, sizeof(*copy));
    CalculatorStatus status = CALCULATOR_OUT_OF_MEMORY;
    if (copy != NULL) {
        copy->value.kind = kind;
        copy->value.number = bigdecimal_create();
        if (copy->value.number != NULL) switch (kind) {
            case CALCULATOR_VALUE_INTEGER:
                copy->value.integer = bigint_create();
                if (copy->value.integer != NULL) status = calculator_from_integer_status(bigint_copy(copy->value.integer, input));
                break;
            case CALCULATOR_VALUE_RATIONAL:
                copy->value.rational = bigrational_create();
                if (copy->value.rational != NULL) status = calculator_from_rational_status(bigrational_copy(copy->value.rational, input));
                break;
            case CALCULATOR_VALUE_DECIMAL:
                status = calculator_from_decimal_status(bigdecimal_copy(copy->value.number, input));
                break;
            case CALCULATOR_VALUE_COMPLEX_RATIONAL:
                copy->value.complex_rational = bigrationalcomplex_create();
                if (copy->value.complex_rational != NULL) status = calculator_from_complex_status(bigrationalcomplex_copy(copy->value.complex_rational, input));
                break;
            case CALCULATOR_VALUE_COMPLEX_DECIMAL:
                copy->value.complex_decimal = bigcomplex_create();
                if (copy->value.complex_decimal != NULL) status = calculator_from_complex_status(bigcomplex_copy(copy->value.complex_decimal, input));
                break;
        }
        if (status == CALCULATOR_OK && (kind == CALCULATOR_VALUE_INTEGER || kind == CALCULATOR_VALUE_RATIONAL)) {
            status = calculator_materialize_exact(copy->value.number, &copy->value, &settings.calculator);
            copy->value.exact_integer_projection = kind == CALCULATOR_VALUE_INTEGER && status == CALCULATOR_OK;
        }
    }
    NumForgeStatus final = budget_status(status);
    if (final == NUMFORGE_OK) {
        copy->context = settings;
        copy->value.context = settings.calculator;
        copy->value.independent = numforge_result_is_exact(copy);
        *result = copy;
    } else numforge_result_destroy(copy);
    if (owner) numforge_budget_end();
    return diagnostic(final, 0U, error);
}

NumForgeStatus numforge_result_from_integer(NumForgeResult **result, const BigInt *value,
    const NumForgeContext *context, NumForgeError *error)
{ return copy_argument(result, value, CALCULATOR_VALUE_INTEGER, context, error); }
NumForgeStatus numforge_result_from_rational(NumForgeResult **result, const BigRational *value,
    const NumForgeContext *context, NumForgeError *error)
{ return copy_argument(result, value, CALCULATOR_VALUE_RATIONAL, context, error); }
NumForgeStatus numforge_result_from_decimal(NumForgeResult **result, const BigDecimal *value,
    const NumForgeContext *context, NumForgeError *error)
{ return copy_argument(result, value, CALCULATOR_VALUE_DECIMAL, context, error); }
NumForgeStatus numforge_result_from_complex_rational(NumForgeResult **result, const BigRationalComplex *value,
    const NumForgeContext *context, NumForgeError *error)
{ return copy_argument(result, value, CALCULATOR_VALUE_COMPLEX_RATIONAL, context, error); }
NumForgeStatus numforge_result_from_complex_decimal(NumForgeResult **result, const BigComplex *value,
    const NumForgeContext *context, NumForgeError *error)
{ return copy_argument(result, value, CALCULATOR_VALUE_COMPLEX_DECIMAL, context, error); }

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
