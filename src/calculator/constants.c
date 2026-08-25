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
static const char CALCULATOR_PI[] =
    "3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196";
static const char CALCULATOR_E[] =
    "2.71828182845904523536028747135266249775724709369995957496696762772407663035354759457138217852516642742746639193200305992181741359662904357290033429526059563073813232862794349076323382988075319525101901";
static const char CALCULATOR_PHI[] =
    "1.61803398874989484820458683436563811772030917980576286213544862270526046281890244970720720418939113748475408807538689175212663386222353693179318006076672635443338908659593958290563832266131992829026788";

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
    const char *text;

    switch (constant)
    {
        case CALCULATOR_CONSTANT_PI: text = CALCULATOR_PI; break;
        case CALCULATOR_CONSTANT_E: text = CALCULATOR_E; break;
        case CALCULATOR_CONSTANT_PHI: text = CALCULATOR_PHI; break;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }

    return calculator_from_bigdecimal_status(bigdecimal_set_string(value, text));
}
