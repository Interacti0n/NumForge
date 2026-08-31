#include "calculator_internal.h"

#include <limits.h>

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
const char *calculator_status_to_string(CalculatorStatus status)
{
    switch (status)
    {
        case CALCULATOR_OK: return "success";
        case CALCULATOR_NULL_ARGUMENT: return "null argument";
        case CALCULATOR_OUT_OF_MEMORY: return "out of memory";
        case CALCULATOR_INVALID_ARGUMENT: return "invalid argument";
        case CALCULATOR_INVALID_TOKEN: return "invalid token";
        case CALCULATOR_SYNTAX_ERROR: return "syntax error";
        case CALCULATOR_DIVISION_BY_ZERO: return "division by zero";
        case CALCULATOR_VALUE_TOO_LARGE: return "value too large";
        case CALCULATOR_SCALE_OVERFLOW: return "scale overflow";
        case CALCULATOR_TIME_LIMIT: return "TLE: time limit exceeded";
        case CALCULATOR_NOT_IMPLEMENTED: return "not implemented";
        default: return "unknown status";
    }
}

void calculator_context_init(CalculatorContext *context)
{
    if (context == NULL)
    {
        return;
    }

    context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
    context->output_scale = CALCULATOR_DEFAULT_OUTPUT_SCALE;
    context->time_limit_ms = CALCULATOR_DEFAULT_TIME_LIMIT_MS;
    context->rounding = BIGDECIMAL_ROUND_HALF_EVEN;
}

CalculatorStatus calculator_context_set_output_scale(CalculatorContext *context, int64_t output_scale)
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

    context->division_scale = output_scale + CALCULATOR_DIVISION_GUARD_DIGITS;
    if (context->division_scale < CALCULATOR_DEFAULT_DIVISION_SCALE)
    {
        context->division_scale = CALCULATOR_DEFAULT_DIVISION_SCALE;
    }
    context->output_scale = output_scale;
    return CALCULATOR_OK;
}

void calculator_error_clear(CalculatorError *error)
{
    calculator_error_set(error, CALCULATOR_OK, 0);
}

void calculator_error_set(CalculatorError *error, CalculatorStatus status, size_t offset)
{
    if (error != NULL)
    {
        error->status = status;
        error->offset = offset;
    }
}

size_t calculator_error_column(const char *input, size_t byte_offset)
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
