#ifndef NUMFORGE_CALCULATOR_STATUS_H
#define NUMFORGE_CALCULATOR_STATUS_H
#include "src/engine/api/calculator_internal.h"

/* Private engine-boundary conversions; callers need no AST/evaluator storage.
 * Keep the established decimal policies distinct: from_bigdecimal maps division
 * by zero explicitly, while from_decimal retains INVALID_ARGUMENT for that case. */
CalculatorStatus calculator_from_rational_status(BigRationalStatus status);
CalculatorStatus calculator_from_integer_status(BigIntStatus status);
CalculatorStatus calculator_from_decimal_status(BigDecimalStatus status);
CalculatorStatus calculator_from_bigdecimal_status(BigDecimalStatus status);
CalculatorStatus calculator_from_bigint_status(BigIntStatus status);
CalculatorStatus calculator_from_complex_status(BigComplexStatus status);
#endif
