#include "web_api.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Local web adapter operations. This module intentionally contains no HTTP or
    platform code, so its exact C-core behaviour can be covered by unit tests.
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus numforge_web_evaluate(
    const char *input,
    char **result,
    CalculatorError *error
)
{
    return numforge_web_evaluate_with_output_scale(input, CALCULATOR_DEFAULT_OUTPUT_SCALE, result, error);
}

CalculatorStatus numforge_web_evaluate_with_output_scale(
    const char *input,
    int64_t output_scale,
    char **result,
    CalculatorError *error
)
{
    return numforge_web_evaluate_with_options(
        input, output_scale, CALCULATOR_ANGLE_RADIANS, result, error);
}

CalculatorStatus numforge_web_evaluate_with_options(
    const char *input,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    char **result,
    CalculatorError *error
)
{
    CalculatorContext context;
    CalculatorStatus status;
    size_t length;

    if (result != NULL)
    {
        *result = NULL;
    }

    if (input == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);

        return CALCULATOR_NULL_ARGUMENT;
    }

    length = 0U;

    while (length <= NUMFORGE_WEB_MAX_EXPRESSION_LENGTH && input[length] != '\0')
    {
        length++;
    }

    if (length > NUMFORGE_WEB_MAX_EXPRESSION_LENGTH)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, length);

        return CALCULATOR_VALUE_TOO_LARGE;
    }

    calculator_context_init(&context);
    status = calculator_context_set_output_scale(&context, output_scale);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0);

        return status;
    }

    status = calculator_context_set_angle_unit(&context, angle_unit);

    if (status != CALCULATOR_OK)
    {
        calculator_error_set(error, status, 0);

        return status;
    }

    return calculator_compute(input, &context, result, error);
}
