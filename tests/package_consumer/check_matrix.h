#ifndef NUMFORGE_CONSUMER_CHECK_MATRIX_H
#define NUMFORGE_CONSUMER_CHECK_MATRIX_H
#include <numforge/matrix.h>
#include <stdlib.h>
#include <string.h>

static int matrix_api_checks(void)
{
    BigInt *one = bigint_create(); BigRational *v = bigrational_create(), *det = bigrational_create();
    NumForgeMatrix *a = NULL, *x = NULL, *product = NULL; char *text = NULL; int outcome = 1;
    NumForgeLinearSolution *solution = NULL; size_t rank = 99;
    const BigRational *entries[1] = {v};
    if (!one || !v || !det || bigint_set_string(one, "3") != BIGINT_OK ||
        bigrational_from_bigint(v, one) != BIGRATIONAL_OK ||
        numforge_matrix_create(&a, 1, 1, entries) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_determinant(det, a) != NUMFORGE_MATRIX_OK ||
        bigrational_to_string(det, &text) != BIGRATIONAL_OK || strcmp(text, "3") != 0) goto done;
    free(text); text = NULL;
    if (numforge_matrix_solve(&x, a, a) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_multiply(&product, a, x) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_rows(product) != 1 || numforge_matrix_columns(product) != 1 ||
        bigrational_to_string(numforge_matrix_entry(product, 0, 0), &text) != BIGRATIONAL_OK || strcmp(text, "3") != 0) goto done;
    free(text); text = NULL;
    if (numforge_matrix_rank(&rank, a) != NUMFORGE_MATRIX_OK || rank != 1 ||
        numforge_matrix_solve_general(&solution, a, a) != NUMFORGE_MATRIX_OK ||
        numforge_linear_solution_kind(solution) != NUMFORGE_LINEAR_UNIQUE ||
        numforge_linear_solution_rank(solution) != 1 || numforge_linear_solution_nullity(solution) != 0 ||
        numforge_linear_solution_pivot_column(solution, 0) != 0 ||
        numforge_linear_solution_free_column(solution, 0) != SIZE_MAX ||
        numforge_linear_solution_nullspace(solution) != NULL ||
        bigrational_to_string(numforge_matrix_entry(numforge_linear_solution_particular(solution), 0, 0), &text) != BIGRATIONAL_OK ||
        strcmp(text, "1") != 0) goto done;
    outcome = 0;
done:
    free(text); bigint_destroy(one); bigrational_destroy(v); bigrational_destroy(det);
    numforge_linear_solution_destroy(solution);
    numforge_matrix_destroy(a); numforge_matrix_destroy(x); numforge_matrix_destroy(product); return outcome;
}
#endif
