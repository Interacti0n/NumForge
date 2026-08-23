#include "evaluator.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator implementation. It will map AST operators to BigDecimal
    operations while applying the explicit CalculatorContext rounding policy.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluator operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_evaluate(
    BigDecimal *result,
    const CalculatorExpression *expression,
    const CalculatorContext *context,
    CalculatorError *error
)
{
    if (result == NULL || expression == NULL || context == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }

    calculator_error_set(error, CALCULATOR_NOT_IMPLEMENTED, 0);
    return CALCULATOR_NOT_IMPLEMENTED;
}
