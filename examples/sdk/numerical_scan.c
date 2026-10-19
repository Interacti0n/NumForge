#include "numerical_function.h"
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    NumericalFunction f = {0}; NumForgeRootScan *scan = NULL;
    BigRational *lower = bigrational_create(); BigDecimal *decimal = bigdecimal_create();
    char *text = NULL; int outcome = 1;
    if (!numerical_function_init(&f) || !lower || !decimal ||
        bigrational_negate(lower, f.two) != BIGRATIONAL_OK) goto done;
    if (numforge_roots_scan(&scan, numerical_function_scan_evaluate, numerical_function_domain,
        &f, lower, f.two, NULL) != NUMFORGE_NUMERICAL_OK) goto done;
    printf("roots=%zu; further roots may exist\n", numforge_root_scan_count(scan));
    for (size_t i = 0; i < numforge_root_scan_count(scan); ++i) {
        const NumForgeRoot *root = numforge_root_scan_root(scan, i);
        if (bigrational_to_bigdecimal(decimal, numforge_root_x(root), 16, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK ||
            bigdecimal_format(decimal, 12, BIGDECIMAL_ROUND_HALF_EVEN, &text) != BIGDECIMAL_OK) goto done;
        puts(text); free(text); text = NULL;
    }
    outcome = numforge_root_scan_count(scan) == 2 ? 0 : 1;
done:
    free(text); numforge_root_scan_destroy(scan); bigrational_destroy(lower); bigdecimal_destroy(decimal);
    numerical_function_destroy(&f); return outcome;
}
