#include "functions.h"

#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Function names and arities. Domains and rounding belong to numerical
    implementations, not the parser. Pending calls fail before evaluating
    arguments, rather than producing an approximate or placeholder result.
------------------------------------------------------------------------------------------------------------------------------
*/

static const CalculatorFunction calculator_functions[] =
{
    { "abs", 1, 1, CALCULATOR_FUNCTION_ABS },
    { "sign", 1, 1, CALCULATOR_FUNCTION_SIGN },
    { "min", 2, 0, CALCULATOR_FUNCTION_MIN },
    { "max", 2, 0, CALCULATOR_FUNCTION_MAX },
    { "gcd", 2, 2, CALCULATOR_FUNCTION_GCD },
    { "lcm", 2, 2, CALCULATOR_FUNCTION_LCM },
    { "mod", 2, 2, CALCULATOR_FUNCTION_MOD },
    { "factorial", 1, 1, CALCULATOR_FUNCTION_FACTORIAL },
    { "isqrt", 1, 1, CALCULATOR_FUNCTION_ISQRT },
    { "pow", 2, 2, CALCULATOR_FUNCTION_POWER },
    { "sqrt", 1, 1, CALCULATOR_FUNCTION_SQRT },
    { "cbrt", 1, 1, CALCULATOR_FUNCTION_CBRT },
    { "root", 2, 2, CALCULATOR_FUNCTION_ROOT },
    { "exp", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "ln", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "log", 1, 2, CALCULATOR_FUNCTION_PENDING },
    { "sin", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "cos", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "tan", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "asin", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "acos", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "atan", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "radians", 1, 1, CALCULATOR_FUNCTION_PENDING },
    { "degrees", 1, 1, CALCULATOR_FUNCTION_PENDING }
};

const CalculatorFunction *calculator_function_find(
    const char *text,
    size_t length
)
{
    if (text == NULL)
    {
        return NULL;
    }

    for (size_t index = 0; index < sizeof(calculator_functions) / sizeof(calculator_functions[0]); index++)
    {
        const CalculatorFunction *function = &calculator_functions[index];

        if (strlen(function->name) == length && memcmp(function->name, text, length) == 0)
        {
            return function;
        }
    }

    return NULL;
}
