#ifndef NUMFORGE_CALCULATOR_COMPLEX_EVALUATOR_H
#define NUMFORGE_CALCULATOR_COMPLEX_EVALUATOR_H
#include "expression_internal.h"
bool calculator_expression_has_complex(const CalculatorExpression *expression, const CalculatorValue *answer);
/* Includes real sqrt calls whose result may require complex promotion. */
bool calculator_expression_may_be_complex(const CalculatorExpression *expression, const CalculatorValue *answer);
CalculatorStatus calculator_evaluate_complex(CalculatorValue *result,
    const CalculatorExpression *expression, const CalculatorContext *context,
    const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error);
#endif
