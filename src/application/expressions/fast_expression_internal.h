#ifndef NUMFORGE_APPLICATION_FAST_EXPRESSION_INTERNAL_H
#define NUMFORGE_APPLICATION_FAST_EXPRESSION_INTERNAL_H
#include "src/application/expressions/fast_expression.h"
#include "src/engine/parser/expression_internal.h"
CalculatorStatus application_fast_compile_tree(ApplicationFastExpression **result,
    const CalculatorExpression *tree,const CalculatorValue *parameter,
    const CalculatorValue *answer,const CalculatorContext *context,CalculatorError *error);
#endif
