#include "evaluator.h"
#include "expression_internal.h"
#include <numforge/runtime.h>

#include <stdlib.h>
#include <string.h>

#include <numforge/bigint.h>

#define CALCULATOR_STRINGIFY_VALUE(value) #value
#define CALCULATOR_STRINGIFY(value) CALCULATOR_STRINGIFY_VALUE(value)

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator implementation. It recursively evaluates AST nodes into temporary
    BigDecimal values, then copies a final successful value to the caller's
    destination. The caller's result therefore remains unchanged on failure.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for evaluator operations.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef struct CalculatorEvaluation
{
    const CalculatorContext *context;
} CalculatorEvaluation;

static bool calculator_time_limit_reached(const CalculatorEvaluation *evaluation)
{
    (void)evaluation;
    return !numforge_budget_check() && numforge_budget_failure() == NUMFORGE_BUDGET_TIME;
}

static CalculatorStatus calculator_evaluate_expression(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
);

static CalculatorStatus calculator_from_bigdecimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK: return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT: return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_DIVISION_BY_ZERO: return CALCULATOR_DIVISION_BY_ZERO;
        case BIGDECIMAL_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}

static CalculatorStatus calculator_from_bigint_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK: return CALCULATOR_OK;
        case BIGINT_NULL_ARGUMENT: return CALCULATOR_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case BIGINT_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case BIGINT_DIVISION_BY_ZERO: return CALCULATOR_DIVISION_BY_ZERO;
        case BIGINT_NEGATIVE_ARGUMENT:
        case BIGINT_INVALID_ARGUMENT:
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}

static bool calculator_valid_rounding(BigDecimalRoundingMode rounding)
{
    return rounding >= BIGDECIMAL_ROUND_TOWARD_ZERO &&
           rounding <= BIGDECIMAL_ROUND_HALF_EVEN;
}

static CalculatorStatus calculator_set_number(BigDecimal *value, const char *text)
{
    BigDecimalStatus decimal_status;
    const char *separator = strchr(text, ',');

    if (separator == NULL)
    {
        return calculator_from_bigdecimal_status(bigdecimal_set_string(value, text));
    }

    {
        size_t length = strlen(text);
        char *normalized = numforge_malloc(length + 1U);

        if (normalized == NULL)
        {
            return CALCULATOR_OUT_OF_MEMORY;
        }

        memcpy(normalized, text, length + 1U);
        normalized[separator - text] = '.';
        decimal_status = bigdecimal_set_string(value, normalized);
        free(normalized);
    }

    return calculator_from_bigdecimal_status(decimal_status);
}

/* Domain checks use public predicates, never numeric representation fields. */
static bool calculator_nonnegative_integer(const BigDecimal *value)
{
    bool integer = false;
    int sign = 0;
    return bigdecimal_is_integer(&integer, value) == BIGDECIMAL_OK &&
        bigdecimal_sign(&sign, value) == BIGDECIMAL_OK && integer && sign >= 0;
}

static CalculatorStatus calculator_bigdecimal_to_bigint(BigInt **result, const BigDecimal *value, bool signed_input)
{
    if (!signed_input && !calculator_nonnegative_integer(value)) return CALCULATOR_INVALID_ARGUMENT;
    BigInt *integer = bigint_create();
    if (integer == NULL) return CALCULATOR_OUT_OF_MEMORY;
    CalculatorStatus status = calculator_from_bigdecimal_status(bigdecimal_to_bigint(integer, value));
    if (status != CALCULATOR_OK) bigint_destroy(integer);
    else *result = integer;
    return status;
}

static CalculatorStatus calculator_set_bigdecimal_from_bigint(BigDecimal *value, const BigInt *integer)
{
    return calculator_from_bigdecimal_status(bigdecimal_from_bigint(value, integer));
}

static CalculatorStatus calculator_check_factorial_limit(const BigDecimal *value)
{
    BigDecimal *limit;
    CalculatorStatus status;
    int comparison = 0;

    if (!calculator_nonnegative_integer(value))
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    limit = bigdecimal_create();
    if (limit == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigdecimal_status(
        bigdecimal_set_string(limit, CALCULATOR_STRINGIFY(CALCULATOR_FACTORIAL_MAX_N)));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, value, limit));
    }
    if (status == CALCULATOR_OK && comparison > 0)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    bigdecimal_destroy(limit);
    return status;
}

static CalculatorStatus calculator_bigdecimal_pow(BigDecimal *result,
    const BigDecimal *base, const BigDecimal *exponent, const CalculatorEvaluation *evaluation)
{
    (void)evaluation;
    BigInt *integer = NULL;
    CalculatorStatus status = calculator_bigdecimal_to_bigint(&integer, exponent, false);
    if (status == CALCULATOR_OK)
        status = calculator_from_bigdecimal_status(bigdecimal_pow(result, base, integer));
    bigint_destroy(integer);
    return status;
}

static CalculatorStatus calculator_evaluate_postfix(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *operand;
    BigDecimal *value;
    BigInt *integer = NULL;
    BigInt *integer_result = NULL;
    CalculatorStatus status = calculator_evaluate_expression(
        &operand, expression->data.postfix.operand, evaluation, error);

    if (status != CALCULATOR_OK)
    {
        return status;
    }

    if (expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ||
        expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE)
    {
        value = bigdecimal_create();
        if (value == NULL)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(bigdecimal_mul(value, operand, operand));
        if (status == CALCULATOR_OK && expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE)
        {
            status = calculator_from_bigdecimal_status(bigdecimal_mul(value, value, operand));
        }
        bigdecimal_destroy(operand);
        if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
        }
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }

        *result = value;
        return CALCULATOR_OK;
    }

    if (expression->data.postfix.operation != CALCULATOR_POSTFIX_FACTORIAL)
    {
        bigdecimal_destroy(operand);
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);
        return CALCULATOR_INVALID_ARGUMENT;
    }

    status = calculator_check_factorial_limit(operand);
    if (status == CALCULATOR_OK)
    {
        status = calculator_bigdecimal_to_bigint(&integer, operand, false);
    }
    bigdecimal_destroy(operand);
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, expression->offset);
        return status;
    }

    integer_result = bigint_create();
    if (integer_result == NULL)
    {
        bigint_destroy(integer);
        calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigint_status(bigint_factorial(integer_result, integer));
    bigint_destroy(integer);
    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
    {
        status = CALCULATOR_TIME_LIMIT;
    }
    if (status != CALCULATOR_OK)
    {
        bigint_destroy(integer_result);
        calculator_error_set(error, status, expression->offset);
        return status;
    }

    value = bigdecimal_create();
    if (value == NULL)
    {
        bigint_destroy(integer_result);
        calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_set_bigdecimal_from_bigint(value, integer_result);
    bigint_destroy(integer_result);
    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);
        calculator_error_set(error, status, expression->offset);
        return status;
    }

    *result = value;
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_evaluate_integer_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigInt *arguments[2] = {NULL, NULL};
    BigInt *integer_result = NULL;
    BigDecimal *value = NULL;
    CalculatorStatus status = CALCULATOR_OK;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    bool child_error = false;
    for (size_t i = 0; i < expression->data.call.count; i++)
    {
        status = calculator_evaluate_expression(&value, expression->data.call.arguments[i], evaluation, error);
        if (status != CALCULATOR_OK) { child_error = true; break; }
        status = calculator_bigdecimal_to_bigint(&arguments[i], value, operation != CALCULATOR_FUNCTION_ISQRT);
        bigdecimal_destroy(value);
        value = NULL;
        if (status != CALCULATOR_OK) break;
    }
    if (status == CALCULATOR_OK)
    {
        integer_result = bigint_create();
        if (integer_result == NULL) status = CALCULATOR_OUT_OF_MEMORY;
    }
    if (status == CALCULATOR_OK)
    {
        BigIntStatus integer_status;
        switch (operation)
        {
            case CALCULATOR_FUNCTION_GCD: integer_status = bigint_gcd(integer_result, arguments[0], arguments[1]); break;
            case CALCULATOR_FUNCTION_LCM: integer_status = bigint_lcm(integer_result, arguments[0], arguments[1]); break;
            case CALCULATOR_FUNCTION_MOD: integer_status = bigint_mod(integer_result, arguments[0], arguments[1]); break;
            default: integer_status = bigint_isqrt(integer_result, arguments[0]); break;
        }
        status = calculator_from_bigint_status(integer_status);
    }
    if (status == CALCULATOR_OK)
    {
        value = bigdecimal_create();
        status = value == NULL ? CALCULATOR_OUT_OF_MEMORY : calculator_set_bigdecimal_from_bigint(value, integer_result);
    }
    bigint_destroy(arguments[0]);
    bigint_destroy(arguments[1]);
    bigint_destroy(integer_result);
    if (calculator_time_limit_reached(evaluation)) status = CALCULATOR_TIME_LIMIT;
    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);
        if (!child_error) calculator_error_set(error, status, expression->offset);
        return status;
    }
    *result = value;
    return CALCULATOR_OK;
}

static bool calculator_decimal_zero(const BigDecimal *value)
{
    bool zero = false;
    (void)bigdecimal_is_zero(&zero, value);
    return zero;
}

/* Root degree is an application parameter, not a rounded numeric argument. */
static CalculatorStatus calculator_evaluate_root_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *value = NULL, *degree_value = NULL, *limit = NULL;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    uint32_t degree = operation == CALCULATOR_FUNCTION_CBRT ? 3U : 2U;
    CalculatorStatus status = calculator_evaluate_expression(
        &value, expression->data.call.arguments[0], evaluation, error);
    bool child_error = status != CALCULATOR_OK;
    if (status == CALCULATOR_OK && operation == CALCULATOR_FUNCTION_ROOT)
    {
        status = calculator_evaluate_expression(&degree_value, expression->data.call.arguments[1], evaluation, error);
        child_error = status != CALCULATOR_OK;
        if (status == CALCULATOR_OK && (!calculator_nonnegative_integer(degree_value) || calculator_decimal_zero(degree_value)))
            status = CALCULATOR_INVALID_ARGUMENT;
        if (status == CALCULATOR_OK)
        {
            limit = bigdecimal_create();
            status = limit == NULL ? CALCULATOR_OUT_OF_MEMORY : calculator_from_bigdecimal_status(
                bigdecimal_set_string(limit, CALCULATOR_STRINGIFY(CALCULATOR_MAX_ROOT_DEGREE)));
        }
        if (status == CALCULATOR_OK)
        {
            int comparison = 0;
            status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, degree_value, limit));
            if (status == CALCULATOR_OK && comparison > 0) status = CALCULATOR_VALUE_TOO_LARGE;
        }
        if (status == CALCULATOR_OK)
        {
            char *text = NULL;
            status = calculator_from_bigdecimal_status(bigdecimal_to_string(degree_value, &text));
            if (status == CALCULATOR_OK) degree = (uint32_t)strtoul(text, NULL, 10);
            free(text);
        }
    }
    if (status == CALCULATOR_OK)
    {
        int64_t digits = evaluation->context->division_scale;
        if (digits < CALCULATOR_DEFAULT_DIVISION_SCALE) digits = CALCULATOR_DEFAULT_DIVISION_SCALE;
        status = calculator_from_bigdecimal_status(bigdecimal_root(
            value, value, degree, digits, evaluation->context->rounding));
    }
    bigdecimal_destroy(degree_value);
    bigdecimal_destroy(limit);
    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);
        if (!child_error) calculator_error_set(error, status, expression->offset);
        return status;
    }
    *result = value;
    return CALCULATOR_OK;
}

/* Evaluate basic calls left to right, retaining at most the selected value
 * and the current argument. Even an unselected argument must be evaluated
 * so that its errors are not silently discarded. */
static CalculatorStatus calculator_evaluate_basic_call(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *selected = NULL;
    CalculatorFunctionImplementation operation = expression->data.call.function->implementation;
    CalculatorStatus status = calculator_evaluate_expression(
        &selected, expression->data.call.arguments[0], evaluation, error);
    if (status != CALCULATOR_OK) return status;

    if (operation == CALCULATOR_FUNCTION_ABS)
        status = calculator_from_bigdecimal_status(bigdecimal_abs(selected, selected));
    else if (operation == CALCULATOR_FUNCTION_SIGN)
    {
        int numeric_sign = 0;
        (void)bigdecimal_sign(&numeric_sign, selected);
        const char *sign = numeric_sign == 0 ? "0" : numeric_sign < 0 ? "-1" : "1";
        status = calculator_from_bigdecimal_status(bigdecimal_set_string(selected, sign));
    }
    else
    {
        for (size_t index = 1; index < expression->data.call.count; index++)
        {
            BigDecimal *argument = NULL;
            int comparison = 0;
            status = calculator_evaluate_expression(
                &argument, expression->data.call.arguments[index], evaluation, error);
            if (status != CALCULATOR_OK)
            {
                bigdecimal_destroy(selected);
                return status;
            }
            status = calculator_from_bigdecimal_status(bigdecimal_compare(&comparison, argument, selected));
            if (status == CALCULATOR_OK &&
                ((operation == CALCULATOR_FUNCTION_MIN && comparison < 0) ||
                 (operation == CALCULATOR_FUNCTION_MAX && comparison > 0)))
            {
                BigDecimal *previous = selected;
                selected = argument;
                argument = previous;
            }
            bigdecimal_destroy(argument);
            if (status != CALCULATOR_OK) break;
        }
    }
    if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
        status = CALCULATOR_TIME_LIMIT;
    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(selected);
        calculator_error_set(error, status, expression->offset);
        return status;
    }
    *result = selected;
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_evaluate_expression(
    BigDecimal **result,
    const CalculatorExpression *expression,
    const CalculatorEvaluation *evaluation,
    CalculatorError *error
)
{
    BigDecimal *value;
    CalculatorStatus status;

    if (calculator_time_limit_reached(evaluation))
    {
        calculator_error_set(error, CALCULATOR_TIME_LIMIT, expression->offset);
        return CALCULATOR_TIME_LIMIT;
    }

    if (expression->type == CALCULATOR_EXPRESSION_CALL)
    {
        /* Borrow children for the existing operator path; this temporary node
         * owns nothing and must never be passed to expression_destroy. */
        CalculatorExpression operation = {0};
        operation.offset = expression->offset;
        operation.depth = expression->depth;
        switch (expression->data.call.function->implementation)
        {
            case CALCULATOR_FUNCTION_SQRT:
            case CALCULATOR_FUNCTION_CBRT:
            case CALCULATOR_FUNCTION_ROOT:
                return calculator_evaluate_root_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_GCD:
            case CALCULATOR_FUNCTION_LCM:
            case CALCULATOR_FUNCTION_MOD:
            case CALCULATOR_FUNCTION_ISQRT:
                return calculator_evaluate_integer_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_ABS:
            case CALCULATOR_FUNCTION_SIGN:
            case CALCULATOR_FUNCTION_MIN:
            case CALCULATOR_FUNCTION_MAX:
                return calculator_evaluate_basic_call(result, expression, evaluation, error);
            case CALCULATOR_FUNCTION_POWER:
                operation.type = CALCULATOR_EXPRESSION_BINARY;
                operation.data.binary.operation = CALCULATOR_BINARY_POWER;
                operation.data.binary.left = expression->data.call.arguments[0];
                operation.data.binary.right = expression->data.call.arguments[1];
                break;
            case CALCULATOR_FUNCTION_FACTORIAL:
                operation.type = CALCULATOR_EXPRESSION_POSTFIX;
                operation.data.postfix.operation = CALCULATOR_POSTFIX_FACTORIAL;
                operation.data.postfix.operand = expression->data.call.arguments[0];
                break;
            default:
                calculator_error_set(error, CALCULATOR_NOT_IMPLEMENTED, expression->offset);
                return CALCULATOR_NOT_IMPLEMENTED;
        }
        return calculator_evaluate_expression(result, &operation, evaluation, error);
    }

    if (expression->type == CALCULATOR_EXPRESSION_NUMBER)
    {
        value = bigdecimal_create();
        if (value == NULL)
        {
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_set_number(value, expression->data.number.text);
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }

        *result = value;
        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_CONSTANT)
    {
        value = bigdecimal_create();
        if (value == NULL)
        {
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_constant_set_value(value, expression->data.constant.constant);
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }

        *result = value;
        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_UNARY)
    {
        BigDecimal *operand;

        status = calculator_evaluate_expression(&operand, expression->data.unary.operand, evaluation, error);
        if (status != CALCULATOR_OK)
        {
            return status;
        }

        if (expression->data.unary.operation == CALCULATOR_UNARY_PLUS)
        {
            *result = operand;
            return CALCULATOR_OK;
        }

        if (expression->data.unary.operation != CALCULATOR_UNARY_MINUS)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);
            return CALCULATOR_INVALID_ARGUMENT;
        }

        value = bigdecimal_create();
        if (value == NULL)
        {
            bigdecimal_destroy(operand);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        status = calculator_from_bigdecimal_status(bigdecimal_negate(value, operand));
        bigdecimal_destroy(operand);
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }

        *result = value;
        return CALCULATOR_OK;
    }

    if (expression->type == CALCULATOR_EXPRESSION_POSTFIX)
    {
        return calculator_evaluate_postfix(result, expression, evaluation, error);
    }

    if (expression->type == CALCULATOR_EXPRESSION_BINARY)
    {
        BigDecimal *left;
        BigDecimal *right;

        status = calculator_evaluate_expression(&left, expression->data.binary.left, evaluation, error);
        if (status != CALCULATOR_OK)
        {
            return status;
        }

        status = calculator_evaluate_expression(&right, expression->data.binary.right, evaluation, error);
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(left);
            return status;
        }

        value = bigdecimal_create();
        if (value == NULL)
        {
            bigdecimal_destroy(left);
            bigdecimal_destroy(right);
            calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, expression->offset);
            return CALCULATOR_OUT_OF_MEMORY;
        }

        switch (expression->data.binary.operation)
        {
            case CALCULATOR_BINARY_ADD:
                status = calculator_from_bigdecimal_status(bigdecimal_add(value, left, right));
                break;
            case CALCULATOR_BINARY_SUBTRACT:
                status = calculator_from_bigdecimal_status(bigdecimal_sub(value, left, right));
                break;
            case CALCULATOR_BINARY_MULTIPLY:
                status = calculator_from_bigdecimal_status(bigdecimal_mul(value, left, right));
                break;
            case CALCULATOR_BINARY_DIVIDE:
                status = calculator_from_bigdecimal_status(evaluation->context->significant_division ?
                    bigdecimal_div_exact_or_significant(value, left, right, evaluation->context->division_scale,
                                               evaluation->context->rounding) :
                    bigdecimal_div(value, left, right, evaluation->context->division_scale,
                                   evaluation->context->rounding));
                break;
            case CALCULATOR_BINARY_POWER:
                status = calculator_bigdecimal_pow(value, left, right, evaluation);
                break;
            default:
                status = CALCULATOR_INVALID_ARGUMENT;
                break;
        }

        if (status == CALCULATOR_OK && calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
        }

        bigdecimal_destroy(left);
        bigdecimal_destroy(right);
        if (status != CALCULATOR_OK)
        {
            bigdecimal_destroy(value);
            calculator_error_set(error, status, expression->offset);
            return status;
        }

        *result = value;
        return CALCULATOR_OK;
    }

    calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, expression->offset);
    return CALCULATOR_INVALID_ARGUMENT;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
static CalculatorStatus calculator_evaluate_impl(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    CalculatorError *error
)
{
    CalculatorEvaluation evaluation;
    BigDecimal *temporary;
    CalculatorStatus status;

    if (result == NULL || expression == NULL || context == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (!calculator_valid_rounding(context->rounding))
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (context->output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (context->time_limit_ms < 0)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    if (context->output_scale > CALCULATOR_MAX_OUTPUT_SCALE ||
        context->division_scale > CALCULATOR_MAX_OUTPUT_SCALE + CALCULATOR_DIVISION_GUARD_DIGITS)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, 0U);
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    evaluation.context = context;
    status = calculator_evaluate_expression(&temporary, expression, &evaluation, error);
    if (status != CALCULATOR_OK)
    {
        return status;
    }

    status = calculator_budget_status(CALCULATOR_OK);
    if (status == CALCULATOR_OK)
        status = calculator_from_bigdecimal_status(bigdecimal_copy(result, temporary));
    bigdecimal_destroy(temporary);
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, expression->offset);
        return status;
    }

    calculator_error_clear(error);
    return CALCULATOR_OK;
}

CalculatorStatus calculator_evaluate(BigDecimal *result, const CalculatorExpression *expression,
                                    const CalculatorContext *context, CalculatorError *error)
{
    bool owner;
    CalculatorStatus status;
    if (context == NULL) return calculator_evaluate_impl(result, expression, context, error);
    owner = numforge_budget_begin(context->time_limit_ms < 0 ? 0U : (uint64_t)context->time_limit_ms,
        CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    status = calculator_evaluate_impl(result, expression, context, error);
    if (status != CALCULATOR_OK && context->time_limit_ms >= 0) status = calculator_budget_status(status);
    if (status != CALCULATOR_OK && (error == NULL || error->status != status))
        calculator_error_set(error, status, error == NULL ? 0U : error->offset);
    if (owner) numforge_budget_end();
    return status;
}
