#ifndef NUMFORGE_CONSUMER_CHECK_NUMERICAL_H
#define NUMFORGE_CONSUMER_CHECK_NUMERICAL_H
#include <numforge/numerical.h>
#include <stdlib.h>
#include <string.h>
/* Exact identity callback: its zero proof uses the original rational coordinate. */
static NumForgeNumericalStatus consumer_identity(BigDecimal *value, NumForgeRootSample *sample,
    const BigRational *x, int64_t digits, void *user)
{
    int sign = 1;
    if (bigrational_compare(&sign, x, (const BigRational *)user) != BIGRATIONAL_OK ||
        bigrational_to_bigdecimal(value, x, digits, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK)
        return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    sample->real = true; sample->exact_zero = sign == 0; return NUMFORGE_NUMERICAL_OK;
}
static int numerical_api_checks(void)
{
    BigRational *zero = bigrational_create(), *lower = bigrational_create(), *upper = bigrational_create();
    BigInt *integer = bigint_create(); NumForgeRoot *root = NULL; char *text = NULL; int outcome = 1;
    if (!zero || !lower || !upper || !integer || bigint_set_string(integer, "-1") != BIGINT_OK ||
        bigrational_from_bigint(lower, integer) != BIGRATIONAL_OK || bigint_set_string(integer, "1") != BIGINT_OK ||
        bigrational_from_bigint(upper, integer) != BIGRATIONAL_OK) goto done;
    if (numforge_root_solve(&root, consumer_identity, NULL, zero, lower, upper, NULL) != NUMFORGE_NUMERICAL_OK ||
        numforge_root_kind(root) != NUMFORGE_ROOT_FOUND || !numforge_root_exact_zero(root) ||
        numforge_root_evaluations(root) != 3 || numforge_root_iterations(root) != 1 ||
        !numforge_root_lower(root) || !numforge_root_upper(root) || !numforge_root_residual(root) ||
        bigrational_to_string(numforge_root_x(root), &text) != BIGRATIONAL_OK || strcmp(text, "0") != 0) goto done;
    outcome = 0;
done:
    free(text); numforge_root_destroy(root); bigint_destroy(integer);
    bigrational_destroy(zero); bigrational_destroy(lower); bigrational_destroy(upper); return outcome;
}
#endif
