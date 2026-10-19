#ifndef NUMFORGE_APPLICATION_NUMERICAL_STATUS_H
#define NUMFORGE_APPLICATION_NUMERICAL_STATUS_H
#include <numforge/numerical.h>
#include "src/engine/api/calculator_internal.h"

/* Callback-specific application errors remain in the adapter, not the kernel. */
static inline CalculatorStatus application_from_numerical_status(
    NumForgeNumericalStatus status, CalculatorStatus callback_status)
{
    switch (status) {
        case NUMFORGE_NUMERICAL_OK: return CALCULATOR_OK;
        case NUMFORGE_NUMERICAL_NULL_ARGUMENT: return CALCULATOR_NULL_ARGUMENT;
        case NUMFORGE_NUMERICAL_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case NUMFORGE_NUMERICAL_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case NUMFORGE_NUMERICAL_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        case NUMFORGE_NUMERICAL_CALLBACK_FAILED: return callback_status;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}
#endif
