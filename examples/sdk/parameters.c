#include <numforge/engine.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    const char *names[] = {"x", "y"};
    NumForgeExpression *expression = NULL;
    NumForgeResult *x = NULL, *y = NULL, *result = NULL;
    const NumForgeResult *arguments[2] = {NULL, NULL};
    BigInt *number = bigint_create();
    char *text = NULL;
    int outcome = 1;
    if (number == NULL) goto cleanup;
    if (numforge_expression_prepare_parameters(&expression, "x*x+y", names, 2U, NULL, NULL) != NUMFORGE_OK ||
        bigint_set_string(number, "3") != BIGINT_OK || numforge_result_from_integer(&x, number, NULL, NULL) != NUMFORGE_OK ||
        bigint_set_string(number, "4") != BIGINT_OK || numforge_result_from_integer(&y, number, NULL, NULL) != NUMFORGE_OK) goto cleanup;
    arguments[0] = x; arguments[1] = y;
    if (numforge_expression_evaluate_arguments(&result, expression, arguments, 2U, NULL, NULL) != NUMFORGE_OK ||
        numforge_result_to_string(result, &text) != NUMFORGE_OK) goto cleanup;
    printf("x*x+y at x=3, y=4: %s\n", text); outcome = 0;
cleanup:
    free(text); bigint_destroy(number); numforge_expression_destroy(expression);
    numforge_result_destroy(x); numforge_result_destroy(y); numforge_result_destroy(result);
    return outcome;
}
