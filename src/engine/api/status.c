#include "src/engine/api/status.h"

CalculatorStatus calculator_from_rational_status(BigRationalStatus status)
{
    switch (status)
    {
        case BIGRATIONAL_OK:
            return CALCULATOR_OK;
        case BIGRATIONAL_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGRATIONAL_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGRATIONAL_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGRATIONAL_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW:
            return CALCULATOR_SCALE_OVERFLOW;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

CalculatorStatus calculator_from_integer_status(BigIntStatus status)
{
    switch (status)
    {
        case BIGINT_OK:
            return CALCULATOR_OK;
        case BIGINT_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGINT_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGINT_DIVISION_BY_ZERO:
            return CALCULATOR_DIVISION_BY_ZERO;
        case BIGINT_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}

CalculatorStatus calculator_from_decimal_status(BigDecimalStatus status)
{
    switch (status)
    {
        case BIGDECIMAL_OK:
            return CALCULATOR_OK;
        case BIGDECIMAL_NULL_ARGUMENT:
            return CALCULATOR_NULL_ARGUMENT;
        case BIGDECIMAL_OUT_OF_MEMORY:
            return CALCULATOR_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE:
            return CALCULATOR_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW:
            return CALCULATOR_SCALE_OVERFLOW;
        default:
            return CALCULATOR_INVALID_ARGUMENT;
    }
}


CalculatorStatus calculator_from_bigdecimal_status(BigDecimalStatus status)
{
    return status == BIGDECIMAL_DIVISION_BY_ZERO ? CALCULATOR_DIVISION_BY_ZERO :
        calculator_from_decimal_status(status);
}

CalculatorStatus calculator_from_bigint_status(BigIntStatus status)
{
    return calculator_from_integer_status(status);
}


CalculatorStatus calculator_from_complex_status(BigComplexStatus status)
{
    switch (status) {
        case BIGCOMPLEX_OK: return CALCULATOR_OK;
        case BIGCOMPLEX_NULL_ARGUMENT: return CALCULATOR_NULL_ARGUMENT;
        case BIGCOMPLEX_OUT_OF_MEMORY: return CALCULATOR_OUT_OF_MEMORY;
        case BIGCOMPLEX_DIVISION_BY_ZERO: return CALCULATOR_DIVISION_BY_ZERO;
        case BIGCOMPLEX_VALUE_TOO_LARGE: return CALCULATOR_VALUE_TOO_LARGE;
        case BIGCOMPLEX_SCALE_OVERFLOW: return CALCULATOR_SCALE_OVERFLOW;
        default: return CALCULATOR_INVALID_ARGUMENT;
    }
}
