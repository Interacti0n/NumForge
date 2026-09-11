#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "../../src/internal/numforge_alloc.h"

/* Exercise arbitrary bytes without evaluating random powers or factorials. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char input[257];
    CalculatorExpression *expression = NULL;
    CalculatorError error;
    if (size > sizeof(input) - 1U) return 0;
    memcpy(input, data, size);
    input[size] = '\0';
    (void)numforge_budget_begin(100, 1024U * 1024U, 65536U);
    CalculatorStatus status = calculator_parse(input, &expression, &error);
    if (status == CALCULATOR_OK && expression == NULL) abort();
    if (error.offset > strlen(input)) abort();
    calculator_expression_destroy(expression);
    numforge_budget_end();
    return 0;
}
