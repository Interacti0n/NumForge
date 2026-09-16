#include "calculator_internal.h"
#include "parser.h"
#include "evaluator.h"
#include "formatter.h"

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

CalculatorStatus calculator_compute(
    const char *input,
    const CalculatorContext *context,
    char **result,
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
        status = calculator_format_result(value, context, result);
    }

    if (status == CALCULATOR_OK && strlen(*result) > CALCULATOR_MAX_OUTPUT_BYTES)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    bigdecimal_destroy(value);
    calculator_expression_destroy(expression);
    status = calculator_budget_status(status);

    if (status != CALCULATOR_OK)
    {
        free(*result);
        *result = NULL;

        if (error == NULL || error->status != status)
        {
            calculator_error_set(error, status, error == NULL ? 0U : error->offset);
        }
    }
    else
    {
        calculator_error_clear(error);
    }

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
