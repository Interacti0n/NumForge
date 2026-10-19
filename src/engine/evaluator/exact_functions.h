#ifndef NUMFORGE_CALCULATOR_EXACT_FUNCTIONS_H
#define NUMFORGE_CALCULATOR_EXACT_FUNCTIONS_H

#include "src/engine/api/calculator_internal.h"
#include "src/engine/api/status.h"
#include "src/engine/evaluator/functions.h"

#include <numforge/bigint.h>

/* Exact calculator operations used by the expression dispatcher. */
CalculatorStatus calculator_exact_power(BigRational *result, const BigRational *base,
                                        const BigRational *exponent);
CalculatorStatus calculator_exact_sqrt(BigRational *result, const BigRational *operand);
CalculatorStatus calculator_exact_nth_root(BigRational *result, const BigRational *operand, uint32_t degree);
CalculatorStatus calculator_exact_integer_operation(BigRational *result, BigRational *const *items, size_t count,
                                                    CalculatorFunctionImplementation function);
CalculatorStatus calculator_exact_rounding(BigRational *result, BigRational *const *items, size_t count,
                                           CalculatorFunctionImplementation function);

#endif
