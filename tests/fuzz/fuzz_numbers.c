#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include "../../src/internal/numforge_alloc.h"

/* Budget huge compact exponents; verify successful decimal serialization is
 * stable after reparsing. Failed resource-limited conversions are legitimate. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char input[257];
    char *text = NULL;
    char *again = NULL;
    if (size > sizeof(input) - 1U) return 0;
    memcpy(input, data, size);
    input[size] = '\0';
    (void)numforge_budget_begin(100, 1024U * 1024U, 65536U);
    BigInt *integer = bigint_create();
    BigDecimal *decimal = bigdecimal_create();
    BigDecimal *copy = bigdecimal_create();
    if (integer != NULL && bigint_set_string(integer, input) == BIGINT_OK)
    {
        text = bigint_to_string(integer);
        if (text != NULL)
        {
            BigIntStatus status = bigint_set_string(integer, text);
            if (status != BIGINT_OK && status != BIGINT_OUT_OF_MEMORY) abort();
            if (status == BIGINT_OK)
            {
                again = bigint_to_string(integer);
                if (again != NULL && strcmp(text, again) != 0) abort();
            }
        }
        free(text);
        free(again);
        text = again = NULL;
    }
    if (decimal != NULL && copy != NULL && bigdecimal_set_string(decimal, input) == BIGDECIMAL_OK &&
        bigdecimal_to_string(decimal, &text) == BIGDECIMAL_OK)
    {
        BigDecimalStatus status = bigdecimal_set_string(copy, text);
        if (status != BIGDECIMAL_OK && status != BIGDECIMAL_OUT_OF_MEMORY) abort();
        if (status == BIGDECIMAL_OK && bigdecimal_to_string(copy, &again) == BIGDECIMAL_OK &&
            strcmp(text, again) != 0) abort();
    }
    free(text);
    free(again);
    bigint_destroy(integer);
    bigdecimal_destroy(decimal);
    bigdecimal_destroy(copy);
    numforge_budget_end();
    return 0;
}
