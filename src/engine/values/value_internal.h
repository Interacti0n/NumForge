#ifndef NUMFORGE_CALCULATOR_VALUE_INTERNAL_H
#define NUMFORGE_CALCULATOR_VALUE_INTERNAL_H

#include "src/engine/api/calculator_internal.h"
#include "src/engine/api/status.h"

/* Materialize the authoritative exact value using the requested precision. */
CalculatorStatus calculator_materialize_exact(
    BigDecimal *result, const CalculatorValue *value, const CalculatorContext *context);

#endif
