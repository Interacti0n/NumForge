#include <stdlib.h>
#include <string.h>

#include <numforge/bigdecimal.h>

#include "evaluator.h"
#include "formatter.h"
#include "parser.h"
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
    return numforge_web_evaluate_with_output_scale(
        input, CALCULATOR_DEFAULT_OUTPUT_SCALE, result, error);
}

CalculatorStatus numforge_web_evaluate_with_output_scale(
    const char *input,
    int64_t output_scale,
    char **result,
    CalculatorError *error
)
{
    CalculatorContext context;
    CalculatorExpression *expression = NULL;
    BigDecimal *decimal = NULL;
    CalculatorStatus status;
    size_t length;

    if (input == NULL || result == NULL)
    {
        calculator_error_set(error, CALCULATOR_NULL_ARGUMENT, 0);
        return CALCULATOR_NULL_ARGUMENT;
    }

    *result = NULL;
    length = strlen(input);
    if (length == 0U || length > NUMFORGE_WEB_MAX_EXPRESSION_LENGTH)
    {
        calculator_error_set(error, CALCULATOR_VALUE_TOO_LARGE, length);
        return CALCULATOR_VALUE_TOO_LARGE;
    }

    status = calculator_parse(input, &expression, error);
    if (status != CALCULATOR_OK)
    {
        return status;
    }

    decimal = bigdecimal_create();
    if (decimal == NULL)
    {
        calculator_expression_destroy(expression);
        calculator_error_set(error, CALCULATOR_OUT_OF_MEMORY, 0);
        return CALCULATOR_OUT_OF_MEMORY;
    }

    calculator_context_init(&context);
    status = calculator_context_set_output_scale(&context, output_scale);
    if (status != CALCULATOR_OK)
    {
        calculator_expression_destroy(expression);
        bigdecimal_destroy(decimal);
        calculator_error_set(error, status, 0);
        return status;
    }
    status = calculator_evaluate(decimal, expression, &context, error);
    calculator_expression_destroy(expression);
    if (status == CALCULATOR_OK)
    {
        status = calculator_format_result(decimal, &context, result);
        if (status != CALCULATOR_OK)
        {
            calculator_error_set(error, status, 0);
        }
    }

    bigdecimal_destroy(decimal);
    return status;
}
