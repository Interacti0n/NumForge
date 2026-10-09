#ifndef NUMFORGE_CALCULATOR_VALUE_INTERNAL_H
#define NUMFORGE_CALCULATOR_VALUE_INTERNAL_H

#include "calculator_internal.h"
CalculatorStatus calculator_from_complex_status(BigComplexStatus status);

/* Materialize the authoritative exact value using the requested precision. */
CalculatorStatus calculator_materialize_exact(
    BigDecimal *result, const CalculatorValue *value, const CalculatorContext *context);

#endif
