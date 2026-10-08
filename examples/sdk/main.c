#include <stdio.h>
#include <stdlib.h>
#include <numforge/bigdecimal.h>

int main(void)
{
    BigDecimal *a = bigdecimal_create(), *b = bigdecimal_create();
    char *text = NULL;
    int result = 1;
    if (a == NULL || b == NULL) goto cleanup;
    if (bigdecimal_set_string(a, "0.1") != BIGDECIMAL_OK ||
        bigdecimal_set_string(b, "0.2") != BIGDECIMAL_OK ||
        bigdecimal_add(a, a, b) != BIGDECIMAL_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK) goto cleanup;
    printf("0.1 + 0.2 = %s\n", text);
    result = 0;
cleanup:
    free(text);
    bigdecimal_destroy(a);
    bigdecimal_destroy(b);
    return result;
}
