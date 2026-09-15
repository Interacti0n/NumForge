#ifndef NUMFORGE_CALCULATOR_FUNCTIONS_H
#define NUMFORGE_CALCULATOR_FUNCTIONS_H

#include <stddef.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal function registry. Names are lowercase ASCII letters only.
    A zero maximum denotes variadic arity, bounded by input/allocation limits.
    Recognized calls need not have a numerical implementation yet.
------------------------------------------------------------------------------------------------------------------------------
*/
typedef enum CalculatorFunctionImplementation
{
    CALCULATOR_FUNCTION_PENDING,
    CALCULATOR_FUNCTION_POWER,
    CALCULATOR_FUNCTION_FACTORIAL,
    CALCULATOR_FUNCTION_ABS,
    CALCULATOR_FUNCTION_SIGN,
    CALCULATOR_FUNCTION_MIN,
    CALCULATOR_FUNCTION_MAX,
    CALCULATOR_FUNCTION_GCD,
    CALCULATOR_FUNCTION_LCM,
    CALCULATOR_FUNCTION_MOD,
    CALCULATOR_FUNCTION_ISQRT,
    CALCULATOR_FUNCTION_SQRT,
    CALCULATOR_FUNCTION_CBRT,
    CALCULATOR_FUNCTION_ROOT
} CalculatorFunctionImplementation;

typedef struct CalculatorFunction
{
    const char *name;
    size_t minimum_arguments;
    size_t maximum_arguments;
    CalculatorFunctionImplementation implementation;
} CalculatorFunction;

/* Returns an immutable registry entry, or NULL for an unknown name. */
const CalculatorFunction *calculator_function_find(const char *text, size_t length);

#endif
