#ifndef NUMFORGE_CALCULATOR_EXACT_EVALUATOR_H
#define NUMFORGE_CALCULATOR_EXACT_EVALUATOR_H

#include "calculator_internal.h"
#include "parser.h"

/* Exact arithmetic subtree supported by the initial typed evaluator. An
 * unsupported tree is handed intact to the established decimal evaluator. */
bool calculator_exact_supported(const CalculatorExpression *expression, const CalculatorValue *answer);
CalculatorStatus calculator_evaluate_exact(BigRational **result, const CalculatorExpression *expression,
                                           const CalculatorValue *answer, CalculatorError *error);

#endif
