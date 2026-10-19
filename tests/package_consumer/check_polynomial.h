#ifndef NUMFORGE_CONSUMER_CHECK_POLYNOMIAL_H
#define NUMFORGE_CONSUMER_CHECK_POLYNOMIAL_H
#include <numforge/polynomial.h>
#include <stdlib.h>
#include <string.h>
static int polynomial_api_checks(void)
{
    BigInt *number = bigint_create();
    BigRational *a = bigrational_create(), *b = bigrational_create(), *c = bigrational_create();
    NumForgePolynomialSolution *solution = NULL;
    BigRationalComplex *root = NULL;
    char *text = NULL;
    int outcome = 1;
    if (number == NULL || a == NULL || b == NULL || c == NULL) goto cleanup;
    if (bigint_set_string(number, "1") != BIGINT_OK || bigrational_from_bigint(a, number) != BIGRATIONAL_OK ||
        bigrational_from_bigint(c, number) != BIGRATIONAL_OK ||
        numforge_polynomial_solve(&solution, a, b, c) != BIGRATIONAL_OK ||
        numforge_polynomial_kind(solution) != NUMFORGE_POLYNOMIAL_FINITE ||
        numforge_polynomial_root_count(solution) != 2U || !numforge_polynomial_is_imaginary(solution) ||
        numforge_polynomial_root_exact(&root, solution, 1U) != BIGRATIONAL_OK ||
        bigrationalcomplex_to_string(root, 1024U, &text) != BIGCOMPLEX_OK || strcmp(text, "i") != 0) goto cleanup;
    outcome = 0;
cleanup:
    free(text); bigrationalcomplex_destroy(root); numforge_polynomial_destroy(solution);
    bigint_destroy(number); bigrational_destroy(a); bigrational_destroy(b); bigrational_destroy(c);
    return outcome;
}
#endif
