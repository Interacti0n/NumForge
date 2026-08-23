#include "calculator_internal.h"

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
    context->rounding = BIGDECIMAL_ROUND_HALF_EVEN;
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
