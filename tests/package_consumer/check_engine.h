#ifndef NUMFORGE_CONSUMER_CHECK_ENGINE_H
#define NUMFORGE_CONSUMER_CHECK_ENGINE_H
#include <numforge/engine.h>
#include <stdlib.h>
#include <string.h>

/* Built as both C and C++ using only the relocated installed SDK. */
static int engine_api_checks(void)
{
    NumForgeContext *context = numforge_context_create();
    NumForgeExpression *expression = NULL;
    NumForgeResult *result = NULL;
    NumForgeResult *argument = NULL;
    const NumForgeResult *arguments[1] = {NULL};
    const char *parameters[1] = {"x"};
    NumForgeError error = {NUMFORGE_OK, 0};
    char *text = NULL;
    int outcome = 1;
    if (context == NULL) goto cleanup;
    if (numforge_expression_prepare(&expression, "1/3+1/6", context, &error) != NUMFORGE_OK ||
        numforge_expression_evaluate(&result, expression, context, &error) != NUMFORGE_OK ||
        !numforge_result_is_exact(result) || numforge_result_rational(result) == NULL ||
        numforge_result_to_string(result, &text) != NUMFORGE_OK || strcmp(text, "1/2") != 0)
        goto cleanup;
    free(text); text = NULL; numforge_result_destroy(result); result = NULL;
    numforge_expression_destroy(expression); expression = NULL;
    if (numforge_evaluate(&argument, "1/3", NULL, NULL) != NUMFORGE_OK ||
        numforge_expression_prepare_parameters(&expression, "x+x", parameters, 1U, NULL, NULL) != NUMFORGE_OK)
        goto cleanup;
    arguments[0] = argument;
    if (numforge_expression_evaluate_arguments(&result, expression, arguments, 1U, NULL, NULL) != NUMFORGE_OK ||
        numforge_result_to_string(result, &text) != NUMFORGE_OK || strcmp(text, "2/3") != 0)
        goto cleanup;
    free(text); text = NULL;
    numforge_result_destroy(result); result = NULL;
    numforge_expression_destroy(expression); expression = NULL;
    if (numforge_context_set_precision(context, 1000) != NUMFORGE_OK ||
        numforge_evaluate(&result, "sqrt(2)", context, &error) != NUMFORGE_OK ||
        numforge_result_is_exact(result) || numforge_result_decimal(result) == NULL ||
        bigdecimal_to_string(numforge_result_decimal(result), &text) != BIGDECIMAL_OK ||
        strlen(text) != 1001U || strncmp(text, "1.414213562373095048801688724209698", 35U) != 0)
        goto cleanup;
    outcome = 0;
cleanup:
    free(text);
    numforge_result_destroy(result);
    numforge_result_destroy(argument);
    numforge_expression_destroy(expression);
    numforge_context_destroy(context);
    return outcome;
}
#endif
