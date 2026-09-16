#ifndef NUMFORGE_CALCULATOR_CONSTANTS_H
#define NUMFORGE_CALCULATOR_CONSTANTS_H

#include <stdbool.h>

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Built-in mathematical constants. The calculator recognizes only the exact
    UTF-8 symbols π, e, and φ. Values come from the precision-aware public
    BigDecimal API and are prepared only during evaluation. Symbol recognition
    remains client syntax.

    Implementation: src/calculator/constants.c
------------------------------------------------------------------------------------------------------------------------------
*/

typedef enum CalculatorConstant
{
    CALCULATOR_CONSTANT_PI,
    CALCULATOR_CONSTANT_E,
    CALCULATOR_CONSTANT_PHI
} CalculatorConstant;

#define CALCULATOR_CONSTANT_COUNT 3U

/*
------------------------------------------------------------------------------------------------------------------------------
    Constant operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/

bool calculator_constant_from_text(
    const char *text,
    size_t length,
    CalculatorConstant *constant
);
CalculatorStatus calculator_constant_set_value(
    BigDecimal *value,
    CalculatorConstant constant,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

#endif
