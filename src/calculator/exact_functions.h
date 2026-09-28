#ifndef NUMFORGE_CALCULATOR_EXACT_FUNCTIONS_H
#define NUMFORGE_CALCULATOR_EXACT_FUNCTIONS_H

#include "calculator_internal.h"
#include "functions.h"

#include <numforge/bigint.h>

/* Exact calculator operations used by the expression dispatcher. */
CalculatorStatus calculator_from_rational_status(BigRationalStatus status);
CalculatorStatus calculator_from_integer_status(BigIntStatus status);
CalculatorStatus calculator_from_decimal_status(BigDecimalStatus status);
CalculatorStatus calculator_exact_power(BigRational *result, const BigRational *base,
                                        const BigRational *exponent);
CalculatorStatus calculator_exact_sqrt(BigRational *result, const BigRational *operand);
CalculatorStatus calculator_exact_nth_root(BigRational *result, const BigRational *operand, uint32_t degree);
CalculatorStatus calculator_exact_integer_operation(BigRational *result, BigRational *const *items, size_t count,
                                                    CalculatorFunctionImplementation function);
CalculatorStatus calculator_exact_rounding(BigRational *result, BigRational *const *items, size_t count,
                                           CalculatorFunctionImplementation function);

#endif
