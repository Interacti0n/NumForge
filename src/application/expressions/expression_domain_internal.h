#ifndef NUMFORGE_EXPRESSION_DOMAIN_INTERNAL_H
#define NUMFORGE_EXPRESSION_DOMAIN_INTERNAL_H
#include "src/engine/parser/expression_internal.h"

/* Exact rational enclosures and a deliberately restricted real-continuity
 * whitelist. Unknown is not a domain error or a claim of discontinuity. */
CalculatorStatus application_expression_domain_tree(bool *verified,
    const CalculatorExpression *tree, const char *parameter,
    const BigRational *lower, const BigRational *upper);
#endif
