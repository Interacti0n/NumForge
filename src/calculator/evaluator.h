#ifndef NUMFORGE_CALCULATOR_EVALUATOR_H
#define NUMFORGE_CALCULATOR_EVALUATOR_H

#include "calculator_internal.h"
#include "parser.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator operation functions.

    Evaluate a parsed expression to an owned BigDecimal destination. The result
    is not modified on failure.

    Implementation: src/calculator/evaluator.c and evaluator_* modules
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus calculator_evaluate(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    CalculatorError *error
);

/* Private shared typed evaluation stage. The AST has bound variable pointers;
 * result is empty (uses_variables may be set), and context has been validated.
 * Caller owns the AST and the enclosing operation's resource budget. */
CalculatorStatus calculator_evaluate_parsed_value(
    const CalculatorExpression *expression, const CalculatorContext *context,
    const CalculatorValue *answer, uint64_t *random_state,
    CalculatorValue *result, CalculatorError *error);

CalculatorStatus calculator_evaluate_with_answer(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    const BigDecimal *answer,
    const CalculatorValue *typed_answer,
    uint64_t *random_state,
    CalculatorError *error
);

#endif
