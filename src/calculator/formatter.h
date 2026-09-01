#ifndef NUMFORGE_CALCULATOR_FORMATTER_H
#define NUMFORGE_CALCULATOR_FORMATTER_H

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Result formatting functions. The formatter applies the context's requested
    output scale and uses scientific notation for very large or very small
    non-zero values, keeping ordinary results easy to read. On success, result
    receives an owned string released with free(); on failure, it receives NULL.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_format_result(
    const BigDecimal *value,
    const CalculatorContext *context,
    char **result
);

#endif
