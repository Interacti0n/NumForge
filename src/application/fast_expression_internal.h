#ifndef NUMFORGE_APPLICATION_FAST_EXPRESSION_INTERNAL_H
#define NUMFORGE_APPLICATION_FAST_EXPRESSION_INTERNAL_H
#include "fast_expression.h"
#include "expression_internal.h"
CalculatorStatus application_fast_compile_tree(ApplicationFastExpression **result,
    const CalculatorExpression *tree,const CalculatorValue *parameter,
    const CalculatorValue *answer,const CalculatorContext *context,CalculatorError *error);
#endif
