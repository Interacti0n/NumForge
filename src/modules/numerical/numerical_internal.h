#ifndef NUMFORGE_NUMERICAL_INTERNAL_H
#define NUMFORGE_NUMERICAL_INTERNAL_H
#include <numforge/numerical.h>
static inline NumForgeNumericalStatus rational(BigRationalStatus status)
{
    switch (status) {
        case BIGRATIONAL_OK: return NUMFORGE_NUMERICAL_OK;
        case BIGRATIONAL_OUT_OF_MEMORY: return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        case BIGRATIONAL_VALUE_TOO_LARGE: return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW: return NUMFORGE_NUMERICAL_SCALE_OVERFLOW;
        default: return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
}
static inline NumForgeNumericalStatus decimal(BigDecimalStatus status)
{
    switch (status) {
        case BIGDECIMAL_OK: return NUMFORGE_NUMERICAL_OK;
        case BIGDECIMAL_OUT_OF_MEMORY: return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        case BIGDECIMAL_VALUE_TOO_LARGE: return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
        case BIGDECIMAL_SCALE_OVERFLOW: return NUMFORGE_NUMERICAL_SCALE_OVERFLOW;
        default: return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
}
static inline NumForgeNumericalStatus midpoint(BigRational *result, const BigRational *a,
    const BigRational *b, const BigRational *two)
{
    NumForgeNumericalStatus status = rational(bigrational_add(result, a, b));
    return status == NUMFORGE_NUMERICAL_OK ? rational(bigrational_div(result, result, two)) : status;
}
static inline bool numerical_options_valid(const NumForgeRootOptions *options)
{
    return options->decimal_places >= 1 && options->decimal_places <= INT64_MAX/2 &&
        options->max_iterations > 0 && options->max_evaluations >= 4;
}
#endif
