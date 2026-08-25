#ifndef NUMFORGE_CALCULATOR_CONSTANTS_H
#define NUMFORGE_CALCULATOR_CONSTANTS_H

#include <stdbool.h>

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Built-in mathematical constants. The calculator recognizes only the exact
    UTF-8 symbols π, e, and φ. Their decimal expansions are stored as fixed,
    high-precision text and converted to BigDecimal only during evaluation.
    They are calculator syntax, not part of the public C API.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef enum CalculatorConstant
{
    CALCULATOR_CONSTANT_PI,
    CALCULATOR_CONSTANT_E,
    CALCULATOR_CONSTANT_PHI
} CalculatorConstant;

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
    CalculatorConstant constant
);

#endif
