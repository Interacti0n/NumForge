#include <numforge/polynomial.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    BigInt *number = bigint_create();
    BigRational *a = bigrational_create(), *b = bigrational_create(), *c = bigrational_create();
    NumForgePolynomialSolution *solution = NULL;
    char *center = NULL, *radicand = NULL;
    int outcome = 1;
    if (number == NULL || a == NULL || b == NULL || c == NULL) goto cleanup;
    if (bigint_set_string(number, "1") != BIGINT_OK || bigrational_from_bigint(a, number) != BIGRATIONAL_OK ||
        bigint_set_string(number, "-2") != BIGINT_OK || bigrational_from_bigint(c, number) != BIGRATIONAL_OK ||
        numforge_polynomial_solve(&solution, a, b, c) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_polynomial_center(solution), &center) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_polynomial_radicand(solution), &radicand) != BIGRATIONAL_OK) goto cleanup;
    printf("x^2 - 2 = 0: center = %s; radicand = %s\n", center, radicand);
    printf("exact roots: center - sqrt(radicand), center + sqrt(radicand)\n");
    outcome = 0;
cleanup:
    free(center); free(radicand); numforge_polynomial_destroy(solution);
    bigint_destroy(number); bigrational_destroy(a); bigrational_destroy(b); bigrational_destroy(c);
    return outcome;
}
