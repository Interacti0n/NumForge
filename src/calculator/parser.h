#ifndef NUMFORGE_CALCULATOR_PARSER_H
#define NUMFORGE_CALCULATOR_PARSER_H

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    The AST is private to parser.c. The evaluator consumes it through this
    opaque handle, so parser internals can evolve without changing callers.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef struct CalculatorExpression CalculatorExpression;

/*
------------------------------------------------------------------------------------------------------------------------------
    Parser operation functions.
------------------------------------------------------------------------------------------------------------------------------
*/
CalculatorStatus calculator_parse( /*Parse one input expression into an owned AST*/
    const char *input,
    CalculatorExpression **result,
    CalculatorError *error
);
void calculator_expression_destroy(
    CalculatorExpression *expression
);

#endif
