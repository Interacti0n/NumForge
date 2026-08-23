#ifndef NUMFORGE_CALCULATOR_EVALUATOR_H
#define NUMFORGE_CALCULATOR_EVALUATOR_H

#include "calculator_internal.h"
#include "parser.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator operation functions.

    Evaluate a parsed expression to an owned BigDecimal destination. The result
    is not modified on failure.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_evaluate(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    CalculatorError *error
);

#endif
