#include <numforge/engine.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    NumForgeContext *context = numforge_context_create();
    NumForgeResult *result = NULL;
    NumForgeError error = {NUMFORGE_OK, 0};
    char *text = NULL;
    int exit_code = 1;
    if (context == NULL) goto cleanup;
    if (numforge_context_set_precision(context, 1000) != NUMFORGE_OK) goto cleanup;
    if (numforge_evaluate(&result, "sqrt(2)", context, &error) != NUMFORGE_OK) {
        fprintf(stderr, "%s at byte %zu\n", numforge_status_to_string(error.status), error.offset);
        goto cleanup;
    }
    if (numforge_result_to_string(result, &text) != NUMFORGE_OK) goto cleanup;
    printf("sqrt(2) = %s\n", text);
    printf("exact = %s\n", numforge_result_is_exact(result) ? "yes" : "no");
    exit_code = 0;
cleanup:
    free(text);
    numforge_result_destroy(result);
    numforge_context_destroy(context);
    return exit_code;
}
