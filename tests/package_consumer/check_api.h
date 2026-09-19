#ifndef NUMFORGE_CONSUMER_CHECK_API_H
#define NUMFORGE_CONSUMER_CHECK_API_H
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include <numforge/runtime.h>
#include <stdlib.h>
#include <string.h>

/* Compiled as both C and C++; no source-tree or private-header access. */
static int public_api_checks(void)
{
    BigDecimal *a = bigdecimal_create(), *b = bigdecimal_create();
    BigInt *n = bigint_create(), *r = bigint_create();
    char *text = NULL;
    bool integer = false;
    int sign = 0, result = 1;
    if (a == NULL || b == NULL || n == NULL || r == NULL) goto cleanup;
    if (bigint_set_string(n, "3") != BIGINT_OK ||
        bigdecimal_set_string(a, "1.5") != BIGDECIMAL_OK ||
        bigdecimal_pow(a, a, n) != BIGDECIMAL_OK ||
        bigdecimal_root(a, a, 3, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_floor(b, a) != BIGDECIMAL_OK ||
        bigdecimal_ceil(b, a) != BIGDECIMAL_OK ||
        bigdecimal_trunc(b, a) != BIGDECIMAL_OK ||
        bigdecimal_round(b, a, 1) != BIGDECIMAL_OK ||
        bigdecimal_format(a, -1, BIGDECIMAL_ROUND_HALF_EVEN, &text) != BIGDECIMAL_OK ||
        strcmp(text, "1.5") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigdecimal_set_constant(a, BIGDECIMAL_CONSTANT_PI) != BIGDECIMAL_OK ||
        bigdecimal_set_constant_significant(
            b, BIGDECIMAL_CONSTANT_PI, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_sin(b, b, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_atan(b, b, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_sign(&sign, a) != BIGDECIMAL_OK || sign != 1 ||
        bigdecimal_is_integer(&integer, a) != BIGDECIMAL_OK || integer ||
        bigdecimal_set_string(a, "0") != BIGDECIMAL_OK ||
        bigdecimal_exp(a, a, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (
        bigdecimal_set_string(a, "81") != BIGDECIMAL_OK ||
        bigdecimal_sqrt(a, a, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_to_bigint(n, a) != BIGDECIMAL_OK ||
        bigint_isqrt(n, n) != BIGINT_OK ||
        bigdecimal_from_bigint(a, n) != BIGDECIMAL_OK ||
        bigdecimal_set_string(b, "6") != BIGDECIMAL_OK ||
        bigdecimal_min(a, a, b) != BIGDECIMAL_OK ||
        bigdecimal_max(b, a, b) != BIGDECIMAL_OK ||
        bigdecimal_div_significant(a, a, b, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_div_exact_or_significant(b, a, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_cbrt(b, b, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_to_string(b, &text) != BIGDECIMAL_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigint_set_string(n, "5") != BIGINT_OK ||
        bigint_set_string(r, "2") != BIGINT_OK ||
        bigint_permutation(n, n, r) != BIGINT_OK ||
        (text = bigint_to_string(n)) == NULL || strcmp(text, "20") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigint_set_string(n, "5") != BIGINT_OK ||
        bigint_combination(n, n, r) != BIGINT_OK ||
        (text = bigint_to_string(n)) == NULL || strcmp(text, "10") != 0) goto cleanup;
    free(text); text = NULL;
    if (!numforge_budget_begin(5000, 1024, 512)) goto cleanup;
    if (!numforge_budget_check() || numforge_budget_failure() != NUMFORGE_BUDGET_OK)
    {
        numforge_budget_end();
        goto cleanup;
    }
    numforge_budget_end();
    result = 0;
cleanup:
    free(text); bigint_destroy(n); bigint_destroy(r); bigdecimal_destroy(a); bigdecimal_destroy(b);
    return result;
}
#endif
