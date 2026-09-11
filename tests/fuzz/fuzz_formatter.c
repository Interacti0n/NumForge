#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "formatter.h"
#include "../../src/internal/numforge_alloc.h"

/* Formatting is deterministic and non-mutating. Full output round-trips
 * exactly when its exponent can be represented by the numeric parser. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char input[257];
    char *first = NULL, *second = NULL;
    CalculatorContext context;
    if (size > sizeof(input) - 1U) return 0;
    memcpy(input, data, size);
    input[size] = '\0';
    calculator_context_init(&context);
    context.output_scale = size % 2U == 0 ? CALCULATOR_UNLIMITED_OUTPUT_SCALE : (int64_t)(size % 20U);
    context.rounding = (BigDecimalRoundingMode)(size % 6U);
    (void)numforge_budget_begin(100, 1024U * 1024U, 65536U);
    BigDecimal *value = bigdecimal_create();
    BigDecimal *copy = bigdecimal_create();
    if (value != NULL && copy != NULL && bigdecimal_set_string(value, input) == BIGDECIMAL_OK &&
        calculator_format_result(value, &context, &first) == CALCULATOR_OK)
    {
        if (strlen(first) == 0 || strspn(first, "0123456789.E+-") != strlen(first)) abort();
        if (calculator_format_result(value, &context, &second) == CALCULATOR_OK && strcmp(first, second) != 0) abort();
        if (bigdecimal_set_string(copy, input) == BIGDECIMAL_OK)
        {
            int unchanged = 0;
            if (bigdecimal_compare(&unchanged, copy, value) == BIGDECIMAL_OK && unchanged != 0) abort();
        }
        BigDecimalStatus status = bigdecimal_set_string(copy, first);
        if (status != BIGDECIMAL_OK && status != BIGDECIMAL_SCALE_OVERFLOW &&
            status != BIGDECIMAL_OUT_OF_MEMORY && status != BIGDECIMAL_VALUE_TOO_LARGE) abort();
        if (status == BIGDECIMAL_OK && context.output_scale == CALCULATOR_UNLIMITED_OUTPUT_SCALE)
        {
            int comparison = 0;
            if (bigdecimal_compare(&comparison, copy, value) == BIGDECIMAL_OK && comparison != 0) abort();
        }
    }
    free(first);
    free(second);
    bigdecimal_destroy(value);
    bigdecimal_destroy(copy);
    numforge_budget_end();
    return 0;
}
