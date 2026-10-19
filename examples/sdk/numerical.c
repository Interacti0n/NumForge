#include "numerical_function.h"
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    NumericalFunction f = {0}; NumForgeRoot *root = NULL;
    BigRational *lower = bigrational_create(); BigDecimal *text = bigdecimal_create();
    char *printed = NULL; int outcome = 1;
    if (!numerical_function_init(&f) || !lower || !text) goto done;
    if (numforge_root_solve(&root, numerical_function_evaluate, numerical_function_domain,
        &f, lower, f.two, NULL) != NUMFORGE_NUMERICAL_OK || numforge_root_kind(root) != NUMFORGE_ROOT_FOUND) goto done;
    if (bigrational_to_bigdecimal(text, numforge_root_x(root), 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK ||
        bigdecimal_format(text, 12, BIGDECIMAL_ROUND_HALF_EVEN, &printed) != BIGDECIMAL_OK) goto done;
    printf("sqrt(2) approximately %s; evaluations=%zu\n", printed, numforge_root_evaluations(root)); outcome = 0;
done:
    free(printed); numforge_root_destroy(root); bigrational_destroy(lower); bigdecimal_destroy(text);
    numerical_function_destroy(&f); return outcome;
}
