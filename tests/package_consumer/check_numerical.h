#ifndef NUMFORGE_CONSUMER_CHECK_NUMERICAL_H
#define NUMFORGE_CONSUMER_CHECK_NUMERICAL_H
#include <numforge/numerical.h>
#include <stdlib.h>
#include <string.h>
static NumForgeNumericalStatus consumer_grid(const BigRational *x, size_t index, void *user)
{
    const char *expected[] = {"-1", "0", "1"};
    size_t *count = (size_t *)user; char *text = NULL;
    if (index >= 3 || index != *count || bigrational_to_string(x, &text) != BIGRATIONAL_OK)
        return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    int equal = strcmp(text, expected[index]) == 0; free(text); ++*count;
    return equal ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
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
static NumForgeNumericalStatus consumer_identity_scan(BigDecimal *value, BigRational *proposal,
    NumForgeRootSample *sample, const BigRational *x, int64_t digits, void *user)
{
    NumForgeNumericalStatus status = consumer_identity(value, sample, x, digits, user);
    if (status == NUMFORGE_NUMERICAL_OK && proposal && bigrational_copy(proposal, x) != BIGRATIONAL_OK)
        status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    return status;
}
static int numerical_api_checks(void)
{
    BigRational *zero = bigrational_create(), *lower = bigrational_create(), *upper = bigrational_create();
    BigInt *integer = bigint_create(); NumForgeRoot *root = NULL; char *text = NULL; int outcome = 1;
    NumForgeRootScan *scan = NULL;
    size_t visited = 0;
    if (!zero || !lower || !upper || !integer || bigint_set_string(integer, "-1") != BIGINT_OK ||
        bigrational_from_bigint(lower, integer) != BIGRATIONAL_OK || bigint_set_string(integer, "1") != BIGINT_OK ||
        bigrational_from_bigint(upper, integer) != BIGRATIONAL_OK) goto done;
    if (numforge_grid_visit(lower, upper, 3, consumer_grid, &visited) != NUMFORGE_NUMERICAL_OK || visited != 3) goto done;
    if (numforge_root_solve(&root, consumer_identity, NULL, zero, lower, upper, NULL) != NUMFORGE_NUMERICAL_OK ||
        numforge_root_kind(root) != NUMFORGE_ROOT_FOUND || !numforge_root_exact_zero(root) ||
        numforge_root_evaluations(root) != 3 || numforge_root_iterations(root) != 1 ||
        !numforge_root_lower(root) || !numforge_root_upper(root) || !numforge_root_residual(root) ||
        bigrational_to_string(numforge_root_x(root), &text) != BIGRATIONAL_OK || strcmp(text, "0") != 0) goto done;
    if (numforge_roots_scan(&scan, consumer_identity_scan, NULL, zero, lower, upper, NULL) != NUMFORGE_NUMERICAL_OK ||
        numforge_root_scan_count(scan) != 1 || numforge_root_scan_stop(scan) != NUMFORGE_SCAN_FINISHED ||
        !numforge_root_exact_zero(numforge_root_scan_root(scan, 0)) || numforge_root_scan_root(scan, 1) != NULL) goto done;
    {
        NumForgeRootScanStats stats;
        numforge_root_scan_stats(scan, &stats);
        if (stats.scanned_segments != 128 || stats.evaluations > 4096) goto done;
    }
    outcome = 0;
done:
    numforge_root_scan_destroy(scan);
    free(text); numforge_root_destroy(root); bigint_destroy(integer);
    bigrational_destroy(zero); bigrational_destroy(lower); bigrational_destroy(upper); return outcome;
}
#endif
