#ifndef NUMFORGE_CALCULATOR_UNIT_STATUS_H
#define NUMFORGE_CALCULATOR_UNIT_STATUS_H

#include "calculator_internal.h"
#include <numforge/units.h>

static inline CalculatorStatus calculator_from_unit_status(NumForgeUnitStatus status)
{
    switch (status)
    {
        case NUMFORGE_UNIT_OK: return CALCULATOR_OK;
        case NUMFORGE_UNIT_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case NUMFORGE_UNIT_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case NUMFORGE_UNIT_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        case NUMFORGE_UNIT_UNKNOWN_UNIT: return CALCULATOR_UNKNOWN_UNIT;
        case NUMFORGE_UNIT_INCOMPATIBLE_UNITS: return CALCULATOR_INCOMPATIBLE_UNITS;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}

#endif
