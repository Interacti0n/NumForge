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

    context->division_scale = 34;
    context->output_scale = CALCULATOR_DEFAULT_OUTPUT_SCALE;
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
        context->division_scale = 34;
        context->output_scale = output_scale;
        return CALCULATOR_OK;
    }
    if (output_scale > INT64_MAX - 4)
    {
        return CALCULATOR_SCALE_OVERFLOW;
    }

    context->division_scale = output_scale + 4;
    if (context->division_scale < 34)
    {
        context->division_scale = 34;
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
