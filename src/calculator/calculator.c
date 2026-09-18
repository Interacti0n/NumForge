#include "calculator_internal.h"
#include "parser.h"
#include "evaluator.h"
#include "formatter.h"
#include "expression_internal.h"

#include <numforge/runtime.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared calculator utilities. The tokenizer, parser, and evaluator use this
    module for a common status model, source-positioned errors, and explicit
    BigDecimal evaluation defaults.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared utility functions for calculator operations.
------------------------------------------------------------------------------------------------------------------------------
*/

const char *calculator_status_to_string(
    CalculatorStatus status
)
{
    switch (status)
    {
        case CALCULATOR_OK:
            return "success";
        case CALCULATOR_NULL_ARGUMENT:
            return "null argument";
        case CALCULATOR_OUT_OF_MEMORY:
            return "out of memory";
        case CALCULATOR_INVALID_ARGUMENT:
            return "invalid argument";
        case CALCULATOR_INVALID_TOKEN:
            return "invalid token";
        case CALCULATOR_SYNTAX_ERROR:
            return "syntax error";
        case CALCULATOR_DIVISION_BY_ZERO:
            return "division by zero";
        case CALCULATOR_VALUE_TOO_LARGE:
            return "value too large";
        case CALCULATOR_SCALE_OVERFLOW:
            return "scale overflow";
        case CALCULATOR_TIME_LIMIT:
            return "TLE: time limit exceeded";
        case CALCULATOR_NOT_IMPLEMENTED:
            return "not implemented";
        case CALCULATOR_ARGUMENT_COUNT:
            return "wrong number of arguments";
        default:
            return "unknown status";
    }
}

void calculator_context_init(
    CalculatorContext *context
)
{
    if (context == NULL)
    {
        return;
    }

    context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
    context->output_scale = CALCULATOR_DEFAULT_OUTPUT_SCALE;
    context->time_limit_ms = CALCULATOR_DEFAULT_TIME_LIMIT_MS;
    context->rounding = BIGDECIMAL_ROUND_HALF_EVEN;
    context->angle_unit = CALCULATOR_ANGLE_RADIANS;
    context->significant_division = true;
}

CalculatorStatus calculator_context_set_angle_unit(
    CalculatorContext *context,
    CalculatorAngleUnit angle_unit
)
{
    if (context == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }

    if (angle_unit != CALCULATOR_ANGLE_RADIANS &&
        angle_unit != CALCULATOR_ANGLE_DEGREES)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    context->angle_unit = angle_unit;
    return CALCULATOR_OK;
}

CalculatorStatus calculator_context_set_output_scale(
    CalculatorContext *context,
    int64_t output_scale
)
{
    if (context == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }

    if (output_scale < CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    if (output_scale == CALCULATOR_UNLIMITED_OUTPUT_SCALE)
    {
        context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
        context->output_scale = output_scale;

        return CALCULATOR_OK;
    }

    if (output_scale > INT64_MAX - CALCULATOR_DIVISION_GUARD_DIGITS)
    {
        return CALCULATOR_SCALE_OVERFLOW;
    }

    if (output_scale > CALCULATOR_MAX_OUTPUT_SCALE)
    {
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    context->division_scale = output_scale + CALCULATOR_DIVISION_GUARD_DIGITS;

    if (context->division_scale < CALCULATOR_DEFAULT_DIVISION_SCALE)
    {
        context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
    }

    context->output_scale = output_scale;

    return CALCULATOR_OK;
}

CalculatorStatus calculator_budget_status(
    CalculatorStatus status
)
{
    (void)numforge_budget_check();

    if (numforge_budget_failure() == NUMFORGE_BUDGET_TIME)
    {
        return CALCULATOR_TIME_LIMIT;
    }

    if (numforge_budget_failure() == NUMFORGE_BUDGET_MEMORY)
    {
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    return status;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Conservative proof of independence from working precision. Only explicitly
    exact operations qualify, and every operand must qualify too. Unknown/new
    operations default to context-dependent; no inference from numeric output.
------------------------------------------------------------------------------------------------------------------------------
*/

static bool calculator_expression_independent(const CalculatorExpression *expression)
{
    switch (expression->type)
    {
        case CALCULATOR_EXPRESSION_NUMBER:
            return true;
        case CALCULATOR_EXPRESSION_UNARY:
            return calculator_expression_independent(expression->data.unary.operand);
        case CALCULATOR_EXPRESSION_POSTFIX:
            return (expression->data.postfix.operation == CALCULATOR_POSTFIX_SQUARE ||
                    expression->data.postfix.operation == CALCULATOR_POSTFIX_CUBE ||
                    expression->data.postfix.operation == CALCULATOR_POSTFIX_FACTORIAL) &&
                   calculator_expression_independent(expression->data.postfix.operand);
        case CALCULATOR_EXPRESSION_BINARY:
            return (expression->data.binary.operation == CALCULATOR_BINARY_ADD ||
                    expression->data.binary.operation == CALCULATOR_BINARY_SUBTRACT ||
                    expression->data.binary.operation == CALCULATOR_BINARY_MULTIPLY ||
                    expression->data.binary.operation == CALCULATOR_BINARY_POWER) &&
                   calculator_expression_independent(expression->data.binary.left) &&
                   calculator_expression_independent(expression->data.binary.right);
        case CALCULATOR_EXPRESSION_CALL:
            switch (expression->data.call.function->implementation)
            {
                case CALCULATOR_FUNCTION_POWER:
                case CALCULATOR_FUNCTION_FACTORIAL:
                case CALCULATOR_FUNCTION_ABS:
                case CALCULATOR_FUNCTION_SIGN:
                case CALCULATOR_FUNCTION_MIN:
                case CALCULATOR_FUNCTION_MAX:
                case CALCULATOR_FUNCTION_GCD:
                case CALCULATOR_FUNCTION_LCM:
                case CALCULATOR_FUNCTION_MOD:
                case CALCULATOR_FUNCTION_ISQRT:
                    break;
                default:
                    return false;
            }
            for (size_t index = 0U; index < expression->data.call.count; index++)
            {
                if (!calculator_expression_independent(expression->data.call.arguments[index]))
                {
                    return false;
                }
            }
            return true;
        default:
            return false;
    }
}

void calculator_value_destroy(CalculatorValue *value)
{
    if (value != NULL)
    {
        bigdecimal_destroy(value->number);
        memset(value, 0, sizeof(*value));
    }
}

bool calculator_value_matches(const CalculatorValue *value, const CalculatorContext *context)
{
    return value != NULL && value->number != NULL && context != NULL &&
           value->context.angle_unit == context->angle_unit &&
           value->context.rounding == context->rounding &&
           (value->independent ||
            (value->context.division_scale == context->division_scale &&
             value->context.significant_division == context->significant_division));
}

CalculatorStatus calculator_compute_value(
    const char *input,
    const CalculatorContext *context,
    CalculatorValue *result,
    CalculatorError *error
)
{
    CalculatorExpression *expression = NULL;
    BigDecimal *value = NULL;
    CalculatorStatus status;
    bool owner;
    size_t length = 0U;

    if (result != NULL)
    {
        memset(result, 0, sizeof(*result));
    }

    if (input == NULL || context == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);

        return CALCULATOR_NULL_ARGUMENT;
    }

    if (context->time_limit_ms < 0)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);

        return CALCULATOR_INVALID_ARGUMENT;
    }

    owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    calculator_error_clear(error);

    while (length <= CALCULATOR_MAX_INPUT_BYTES && input[length] != '\0')
    {
        length++;
    }

    status = length > CALCULATOR_MAX_INPUT_BYTES ? CALCULATOR_VALUE_TOO_LARGE : CALCULATOR_OK;

    if (status == CALCULATOR_OK)
    {
        status = calculator_parse(input, &expression, error);
    }

    if (status == CALCULATOR_OK)
    {
        value = bigdecimal_create();
        status =
            value == NULL ? CALCULATOR_OUT_OF_MEMORY : calculator_evaluate(value, expression, context, error);
    }

    if (status == CALCULATOR_OK)
    {
        result->independent = calculator_expression_independent(expression);
    }
    calculator_expression_destroy(expression);
    status = calculator_budget_status(status);

    if (status != CALCULATOR_OK)
    {
        bigdecimal_destroy(value);

        if (error == NULL || error->status != status)
        {
            calculator_error_set(error, status, error == NULL ? 0U : error->offset);
        }
    }
    else
    {
        result->number = value;
        result->context = *context;
        calculator_error_clear(error);
    }

    if (owner)
    {
        numforge_budget_end();
    }

    return status;
}

/* Keep the original one-shot API and its shared compute/format time budget. */
CalculatorStatus calculator_compute(
    const char *input,
    const CalculatorContext *context,
    char **result,
    CalculatorError *error
)
{
    CalculatorValue value = {0};
    CalculatorStatus status;
    bool owner;

    if (result != NULL)
    {
        *result = NULL;
    }
    if (input == NULL || context == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0U);
        return CALCULATOR_NULL_ARGUMENT;
    }
    if (context->time_limit_ms < 0)
    {
        calculator_error_set(error, CALCULATOR_INVALID_ARGUMENT, 0U);
        return CALCULATOR_INVALID_ARGUMENT;
    }
    owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);
    status = calculator_compute_value(input, context, &value, error);
    if (status == CALCULATOR_OK)
    {
        status = calculator_format_result(value.number, context, result);
        calculator_error_set(error, status, 0U);
    }
    calculator_value_destroy(&value);
    if (owner)
    {
        numforge_budget_end();
    }
    return status;
}

void calculator_error_clear(
    CalculatorError *error
)
{
    calculator_error_set(error, CALCULATOR_OK, 0);
}

void calculator_error_set(
    CalculatorError *error,
    CalculatorStatus status,
    size_t offset
)
{
    if (error != NULL)
    {
        error->status = status;
        error->offset = offset;
    }
}

size_t calculator_error_column(
    const char *input,
    size_t byte_offset
)
{
    size_t column = 1U;
    size_t index;

    if (input == NULL)
    {
        return column;
    }

    for (index = 0U; index < byte_offset && input[index] != '\0'; index++)
    {
        unsigned char byte = (unsigned char)input[index];

        if (input[index] == '\n' || input[index] == '\r')
        {
            column = 1U;
        }
        else if ((byte & 0xC0U) != 0x80U)
        {
            column++;
        }
    }

    return column;
}
