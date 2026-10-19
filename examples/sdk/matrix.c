#include <numforge/matrix.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    /* [0 2; 3 4] * x = [7; 15], with exact solution [1/3; 7/2]. */
    const char *texts[] = {"0", "2", "3", "4", "7", "15"};
    BigRational *values[6] = {0}, *det = bigrational_create();
    const BigRational *entries[6] = {0}; BigInt *number = bigint_create();
    NumForgeMatrix *a = NULL, *b = NULL, *x = NULL;
    char *det_text = NULL, *first = NULL, *second = NULL; int outcome = 1;
    if (det == NULL || number == NULL) goto done;
    for (size_t i = 0; i < 6; ++i) {
        entries[i] = values[i] = bigrational_create();
        if (values[i] == NULL || bigint_set_string(number, texts[i]) != BIGINT_OK ||
            bigrational_from_bigint(values[i], number) != BIGRATIONAL_OK) goto done;
    }
    if (numforge_matrix_create(&a, 2, 2, entries) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_create(&b, 2, 1, entries + 4) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_determinant(det, a) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_solve(&x, a, b) != NUMFORGE_MATRIX_OK ||
        bigrational_to_string(det, &det_text) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_matrix_entry(x, 0, 0), &first) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_matrix_entry(x, 1, 0), &second) != BIGRATIONAL_OK) goto done;
    printf("det(A) = %s; x = [%s; %s] (exact)\n", det_text, first, second);
    outcome = 0;
done:
    free(det_text); free(first); free(second); bigint_destroy(number); bigrational_destroy(det);
    for (size_t i = 0; i < 6; ++i) bigrational_destroy(values[i]);
    numforge_matrix_destroy(a); numforge_matrix_destroy(b); numforge_matrix_destroy(x); return outcome;
}
