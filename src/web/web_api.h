#ifndef NUMFORGE_WEB_API_H
#define NUMFORGE_WEB_API_H

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal adapter between the local HTTP server and the calculator pipeline.
    It accepts plain expression text and returns the owned decimal text produced
    by the same parser and BigDecimal evaluator used by the console program.
------------------------------------------------------------------------------------------------------------------------------
*/

#define NUMFORGE_WEB_MAX_EXPRESSION_LENGTH 4096U

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluate one expression with NumForge's default calculator policy and the
    default 10-decimal-place output format.

    On success, result receives an owned string released with free(). On
    failure, result receives NULL and error identifies the calculator failure.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus numforge_web_evaluate(
    const char *input,
    char **result,
    CalculatorError *error
);
CalculatorStatus numforge_web_evaluate_with_output_scale(
    const char *input,
    int64_t output_scale,
    char **result,
    CalculatorError *error
);

#endif
