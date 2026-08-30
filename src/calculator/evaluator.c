#include "evaluator.h"
#include "expression_internal.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <numforge/bigint.h>

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
    clock_t started_at;
    bool can_check_time;
} CalculatorEvaluation;

static bool calculator_time_limit_reached(const CalculatorEvaluation *evaluation)
{
    clock_t now;

    if (!evaluation->can_check_time)
    {
        return false;
    }

    now = clock();
    return now != (clock_t)-1 &&
           (double)(now - evaluation->started_at) * 1000.0 / (double)CLOCKS_PER_SEC >=
               (double)evaluation->context->time_limit_ms;
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
        char *normalized = malloc(length + 1U);

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

static CalculatorStatus calculator_bigdecimal_to_bigint(BigInt **result, const BigDecimal *value)
{
    BigDecimalStatus decimal_status;
    BigInt *integer;
    char *text = NULL;
    CalculatorStatus status;

    decimal_status = bigdecimal_to_string(value, &text);
    status = calculator_from_bigdecimal_status(decimal_status);
    if (status != CALCULATOR_OK)
    {
        return status;
    }
    if (strchr(text, '.') != NULL)
    {
        free(text);
        return CALCULATOR_INVALID_ARGUMENT;
    }

    integer = bigint_create();
    if (integer == NULL)
    {
        free(text);
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigint_status(bigint_set_string(integer, text));
    free(text);
    if (status != CALCULATOR_OK)
    {
        bigint_destroy(integer);
        return status;
    }

    *result = integer;
    return CALCULATOR_OK;
}

static CalculatorStatus calculator_set_bigdecimal_from_bigint(BigDecimal *value, const BigInt *integer)
{
    char *text = bigint_to_string(integer);
    CalculatorStatus status;

    if (text == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigdecimal_status(bigdecimal_set_string(value, text));
    free(text);
    return status;
}

static CalculatorStatus calculator_check_factorial_limit(const BigInt *value)
{
    BigInt *limit = bigint_create();
    CalculatorStatus status;

    if (limit == NULL)
    {
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigint_status(bigint_set_string(limit, "5000"));
    if (status == CALCULATOR_OK && bigint_compare(value, limit) > 0)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    bigint_destroy(limit);
    return status;
}

static CalculatorStatus calculator_bigdecimal_pow(
    BigDecimal *result,
    const BigDecimal *base,
    const BigDecimal *exponent,
    const CalculatorEvaluation *evaluation
)
{
    BigInt *integer_exponent = NULL;
    BigDecimal *accumulator = NULL;
    BigDecimal *factor = NULL;
    CalculatorStatus status = calculator_bigdecimal_to_bigint(&integer_exponent, exponent);

    if (status != CALCULATOR_OK)
    {
        return status;
    }
    if (bigint_is_negative(integer_exponent))
    {
        bigint_destroy(integer_exponent);
        return CALCULATOR_INVALID_ARGUMENT;
    }

    accumulator = bigdecimal_create();
    factor = bigdecimal_create();
    if (accumulator == NULL || factor == NULL)
    {
        bigint_destroy(integer_exponent);
        bigdecimal_destroy(accumulator);
        bigdecimal_destroy(factor);
        return CALCULATOR_OUT_OF_MEMORY;
    }

    status = calculator_from_bigdecimal_status(bigdecimal_set_string(accumulator, "1"));
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_copy(factor, base));
    }

    while (status == CALCULATOR_OK && !bigint_is_zero(integer_exponent))
    {
        if (calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
            break;
        }
        if (bigint_is_odd(integer_exponent))
        {
            status = calculator_from_bigdecimal_status(bigdecimal_mul(accumulator, accumulator, factor));
        }
        if (status == CALCULATOR_OK)
        {
            status = calculator_from_bigint_status(bigint_shift_right(integer_exponent, integer_exponent, 1U));
        }
        if (status == CALCULATOR_OK && !bigint_is_zero(integer_exponent))
        {
            status = calculator_from_bigdecimal_status(bigdecimal_mul(factor, factor, factor));
        }
    }

    if (status == CALCULATOR_OK)
    {
        if (calculator_time_limit_reached(evaluation))
        {
            status = CALCULATOR_TIME_LIMIT;
        }
    }
    if (status == CALCULATOR_OK)
    {
        status = calculator_from_bigdecimal_status(bigdecimal_copy(result, accumulator));
    }

    bigint_destroy(integer_exponent);
    bigdecimal_destroy(accumulator);
    bigdecimal_destroy(factor);
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

    status = calculator_bigdecimal_to_bigint(&integer, operand);
    bigdecimal_destroy(operand);
    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, expression->offset);
        return status;
    }

    status = calculator_check_factorial_limit(integer);
    if (status != CALCULATOR_OK)
    {
        bigint_destroy(integer);
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
                status = calculator_from_bigdecimal_status(
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
CalculatorStatus calculator_evaluate(
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

    evaluation.context = context;
    evaluation.started_at = clock();
    evaluation.can_check_time = evaluation.started_at != (clock_t)-1;
    status = calculator_evaluate_expression(&temporary, expression, &evaluation, error);
    if (status != CALCULATOR_OK)
    {
        return status;
    }

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
