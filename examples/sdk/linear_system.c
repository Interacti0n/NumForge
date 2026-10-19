#include <numforge/matrix.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* 2*y+4*z=10, 3*y+6*z=15: (x,y,z)=(s,5-2*t,t). */
    const char *texts[] = {"0", "2", "4", "0", "3", "6", "10", "15"};
    BigRational *values[8] = {0}; const BigRational *entries[8] = {0};
    BigInt *number = bigint_create(); NumForgeMatrix *a = NULL, *b = NULL;
    NumForgeLinearSolution *solution = NULL; char *y = NULL, *kernel = NULL; int outcome = 1;
    if (number == NULL) goto done;
    for (size_t i = 0; i < 8; ++i) {
        entries[i] = values[i] = bigrational_create();
        if (values[i] == NULL || bigint_set_string(number, texts[i]) != BIGINT_OK ||
            bigrational_from_bigint(values[i], number) != BIGRATIONAL_OK) goto done;
    }
    if (numforge_matrix_create(&a, 2, 3, entries) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_create(&b, 2, 1, entries + 6) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_solve_general(&solution, a, b) != NUMFORGE_MATRIX_OK ||
        numforge_linear_solution_kind(solution) != NUMFORGE_LINEAR_INFINITE ||
        bigrational_to_string(numforge_matrix_entry(numforge_linear_solution_particular(solution), 1, 0), &y) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_matrix_entry(numforge_linear_solution_nullspace(solution), 1, 1), &kernel) != BIGRATIONAL_OK) goto done;
    printf("rank=%zu; nullity=%zu; kind=infinite\n", numforge_linear_solution_rank(solution), numforge_linear_solution_nullity(solution));
    printf("(x,y,z) = (s, %s + (%s)*t, t), exact rational s,t\n", y, kernel); outcome = 0;
done:
    free(y); free(kernel); bigint_destroy(number);
    for (size_t i = 0; i < 8; ++i) bigrational_destroy(values[i]);
    numforge_matrix_destroy(a); numforge_matrix_destroy(b); numforge_linear_solution_destroy(solution); return outcome;
}
