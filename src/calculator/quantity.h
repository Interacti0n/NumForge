#ifndef NUMFORGE_CALCULATOR_QUANTITY_H
#define NUMFORGE_CALCULATOR_QUANTITY_H

#include "expression_internal.h"

/* Client-layer quantities retain numeric ownership plus dimensions and unit ID.
 * Composite results use canonical units; no general unit-string parser. */
bool calculator_expression_has_quantity(const CalculatorExpression *expression,
                                        const CalculatorValue *answer);
CalculatorStatus calculator_evaluate_quantity(CalculatorValue *result,
    const CalculatorExpression *expression, const CalculatorContext *context,
    const CalculatorValue *answer, uint64_t *random_state, CalculatorError *error);
CalculatorStatus calculator_quantity_symbol(const CalculatorValue *value,
                                             char *text, size_t capacity);

#endif
