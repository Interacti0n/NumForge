#ifndef NUMFORGE_CALCULATOR_PARSER_H
#define NUMFORGE_CALCULATOR_PARSER_H

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Callers own an opaque AST handle. Its private representation is shared by
    parser.c and evaluator.c through expression_internal.h.

    Implementation: src/calculator/parser.c
------------------------------------------------------------------------------------------------------------------------------
*/

typedef struct CalculatorExpression CalculatorExpression;

/*
------------------------------------------------------------------------------------------------------------------------------
    Parser operation functions. Parsing accepts the grammar in
    docs/CALCULATOR_DESIGN.md and creates an owned AST. On failure, result is
    unchanged and error identifies the unexpected token or character.
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
