#include "constants.h"

#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Built-in constants. Each value has 200 decimal places, which keeps normal
    calculator use fast and deterministic without pretending that irrational
    constants are exact. A future arbitrary-precision constants module can
    replace these strings without changing tokenizer, parser, or evaluator.
------------------------------------------------------------------------------------------------------------------------------
*/
/*
------------------------------------------------------------------------------------------------------------------------------
    Internal helper functions for constant operations.
------------------------------------------------------------------------------------------------------------------------------
*/
static bool calculator_text_equals(
    const char *text,
    size_t length,
    const char *expected
)
{
    if (strlen(expected) != length)
    {
        return false;
    }

    return memcmp(text, expected, length) == 0;
}

static CalculatorStatus calculator_from_bigdecimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK: return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT: return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Constant operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
bool calculator_constant_from_text(
    const char *text,
    size_t length,
    CalculatorConstant *constant
)
{
    if (text == NULL || constant == NULL)
    {
        return false;
    }

    if (calculator_text_equals(text, length, "\xCF\x80"))
    {
        *constant = CALCULATOR_CONSTANT_PI;
        return true;
    }
    if (calculator_text_equals(text, length, "e"))
    {
        *constant = CALCULATOR_CONSTANT_E;
        return true;
    }
    if (calculator_text_equals(text, length, "\xCF\x86"))
    {
        *constant = CALCULATOR_CONSTANT_PHI;
        return true;
    }

    return false;
}

CalculatorStatus calculator_constant_set_value(BigDecimal *value, CalculatorConstant constant)
{
    BigDecimalConstant numeric_constant;

    switch (constant)
    {
        case CALCULATOR_CONSTANT_PI: numeric_constant = BIGDECIMAL_CONSTANT_PI; break;
        case CALCULATOR_CONSTANT_E: numeric_constant = BIGDECIMAL_CONSTANT_E; break;
        case CALCULATOR_CONSTANT_PHI: numeric_constant = BIGDECIMAL_CONSTANT_PHI; break;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }

    return calculator_from_bigdecimal_status(bigdecimal_set_constant(value, numeric_constant));
}
