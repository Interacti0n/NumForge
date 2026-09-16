#include "formatter.h"

#include <numforge/runtime.h>

#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Formatting adapter. The public BigDecimal API performs numeric formatting;
    this module adds calculator output and resource limits.
------------------------------------------------------------------------------------------------------------------------------
*/

static CalculatorStatus calculator_from_bigdecimal_status(
    BigDecimalStatus status
)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return CALCULATOR_SCALE_OVERFLOW;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

CalculatorStatus calculator_format_result(
    const BigDecimal *value,
    const CalculatorContext *context,
    char **result
)
{
    bool owner;
    CalculatorStatus status;

    if (result != NULL)
    {
        *result = NULL;
    }

    if (context == NULL)
    {
        return CALCULATOR_NULL_ARGUMENT;
    }

    if (context->time_limit_ms < 0)
    {
        return CALCULATOR_INVALID_ARGUMENT;
    }

    owner = numforge_budget_begin(
        (uint64_t)context->time_limit_ms, CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION);

    if (context->output_scale > CALCULATOR_MAX_OUTPUT_SCALE)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }
    else
    {
        status = calculator_from_bigdecimal_status(
            bigdecimal_format(value, context->output_scale, context->rounding, result));
    }

    status = calculator_budget_status(status);

    if (status == CALCULATOR_OK && strlen(*result) > CALCULATOR_MAX_OUTPUT_BYTES)
    {
        status = CALCULATOR_VALUE_TOO_LARGE;
    }

    if (status != CALCULATOR_OK && result != NULL)
    {
        free(*result);
        *result = NULL;
    }

    if (owner)
    {
        numforge_budget_end();
    }

    return status;
}
