#include "parser.h"

#include <stddef.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Parser implementation. It will consume CalculatorTokenizer tokens and own
    every AST node it creates; the AST remains opaque outside this module.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Parser operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_parse(
    const char *input,
    CalculatorExpression **result,
    CalculatorError *error
)
{
    if (input == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }

    calculator_error_set(error, CALCULATOR_NOT_IMPLEMENTED, 0);
    return CALCULATOR_NOT_IMPLEMENTED;
}

void calculator_expression_destroy(CalculatorExpression *expression)
{
    (void)expression;
}
