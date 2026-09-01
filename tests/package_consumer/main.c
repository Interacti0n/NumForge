#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

int main(void)
{
    BigInt *integer = bigint_create();
    BigDecimal *decimal = bigdecimal_create();
    char *integer_text = NULL;
    char *decimal_text = NULL;
    bool is_square = false;
    int result = 1;

    if (integer == NULL || decimal == NULL)
    {
        goto cleanup;
    }

    if (bigint_set_string(integer, "15241578750190521") != BIGINT_OK ||
        bigint_is_perfect_square(&is_square, integer) != BIGINT_OK ||
        !is_square)
    {
        goto cleanup;
    }

    integer_text = bigint_to_string(integer);
    if (integer_text == NULL ||
        strcmp(integer_text, "15241578750190521") != 0)
    {
        goto cleanup;
    }

    if (bigdecimal_set_string(decimal, "12300e-4") != BIGDECIMAL_OK ||
        bigdecimal_to_string(decimal, &decimal_text) != BIGDECIMAL_OK ||
        decimal_text == NULL || strcmp(decimal_text, "1.23") != 0)
    {
        goto cleanup;
    }

    result = 0;

cleanup:
    free(integer_text);
    free(decimal_text);
    bigint_destroy(integer);
    bigdecimal_destroy(decimal);
    return result;
}
