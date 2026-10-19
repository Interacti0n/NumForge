#include <numforge/numerical.h>
#include <stdio.h>
#include <stdlib.h>

/* This callback evaluates x*x exactly. Other callers can collect typed values,
 * evaluate a prepared engine expression, or apply their own numerical policy. */
static NumForgeNumericalStatus square(const BigRational *x, size_t index, void *user)
{
    BigRational *value = user; char *coordinate = NULL, *ordinate = NULL;
    BigRationalStatus status = bigrational_mul(value, x, x);
    if (status == BIGRATIONAL_OK) status = bigrational_to_string(x, &coordinate);
    if (status == BIGRATIONAL_OK) status = bigrational_to_string(value, &ordinate);
    if (status == BIGRATIONAL_OK) printf("%zu: x=%s; x*x=%s (exact)\n", index, coordinate, ordinate);
    free(coordinate); free(ordinate);
    return status == BIGRATIONAL_OK ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
int main(void)
{
    BigRational *lower = bigrational_create(), *upper = bigrational_create(), *value = bigrational_create();
    BigInt *one = bigint_create(); int outcome = 1;
    if (lower && upper && value && one && bigint_set_string(one, "1") == BIGINT_OK &&
        bigrational_from_bigint(upper, one) == BIGRATIONAL_OK &&
        numforge_grid_visit(lower, upper, 4, square, value) == NUMFORGE_NUMERICAL_OK) outcome = 0;
    bigrational_destroy(lower); bigrational_destroy(upper); bigrational_destroy(value); bigint_destroy(one);
    return outcome;
}
