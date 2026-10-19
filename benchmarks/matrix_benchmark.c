#include <numforge/matrix.h>
#include <numforge/runtime.h>
#include "benchmark_clock.h"
#include "../src/internal/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A=(I+J)/d, J is all ones: det(A)=(n+1)/d^n;
 * A*ones=(n+1)/d; A*A=((n+2)J+I)/d^2. References are scalar
 * constructions outside timing, not another call to a matrix algorithm. */
static int set_fraction(BigRational *value, size_t numerator, const BigInt *denominator)
{
    char text[32]; BigInt *top = bigint_create(); int ok = 0;
    (void)snprintf(text, sizeof(text), "%zu", numerator);
    if (top && bigint_set_string(top, text) == BIGINT_OK &&
        bigrational_set_fraction(value, top, denominator) == BIGRATIONAL_OK) ok = 1;
    bigint_destroy(top); return ok;
}
static int equal(const BigRational *a, const BigRational *b)
{
    int comparison = 1;
    return bigrational_compare(&comparison, a, b) == BIGRATIONAL_OK && comparison == 0;
}
static int run_case(size_t n, size_t digits)
{
    NumForgeMatrix *a = NULL, *b = NULL, *output = NULL;
    BigRational *diagonal = bigrational_create(), *off = bigrational_create(), *rhs = bigrational_create();
    BigRational *expected = bigrational_create(), *det = bigrational_create();
    BigRational *product_diagonal = bigrational_create(), *product_off = bigrational_create(), *one = bigrational_create();
    BigInt *d = bigint_create(), *power = bigint_create(), *square = bigint_create(), *unit = bigint_create();
    const BigRational **entries = NULL; char *text = NULL; int outcome = 1;
    if (!diagonal || !off || !rhs || !expected || !det || !product_diagonal || !product_off || !one ||
        !d || !power || !square || !unit) goto done;
    text = malloc(digits + 2); entries = malloc(n * n * sizeof(*entries));
    if (!text || !entries) goto done;
    text[0] = '1'; memset(text + 1, '0', digits); text[digits] = '1'; text[digits + 1] = '\0';
    if (bigint_set_string(d, text) != BIGINT_OK || bigint_set_string(unit, "1") != BIGINT_OK ||
        bigint_copy(power, unit) != BIGINT_OK || bigint_mul(square, d, d) != BIGINT_OK) goto done;
    for (size_t i = 0; i < n; ++i) if (bigint_mul(power, power, d) != BIGINT_OK) goto done;
    if (!set_fraction(diagonal, 2, d) || !set_fraction(off, 1, d) || !set_fraction(rhs, n + 1, d) ||
        !set_fraction(expected, n + 1, power) || !set_fraction(product_diagonal, n + 3, square) ||
        !set_fraction(product_off, n + 2, square) || !set_fraction(one, 1, unit)) goto done;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) entries[i * n + j] = i == j ? diagonal : off;
    if (numforge_matrix_create(&a, n, n, entries) != NUMFORGE_MATRIX_OK) goto done;
    for (size_t i = 0; i < n; ++i) entries[i] = rhs;
    if (numforge_matrix_create(&b, n, 1, entries) != NUMFORGE_MATRIX_OK) goto done;
    for (unsigned operation = 0; operation < 3; ++operation) {
        double elapsed = 0; size_t calls = 0, bytes = 0;
        for (size_t repeat = 0; repeat < 3; ++repeat) {
            if (!numforge_budget_begin(5000, 128U * 1024U * 1024U, 2U * 1024U * 1024U)) goto done;
            numforge_alloc_stats_reset(); double start = benchmark_seconds();
            NumForgeMatrixStatus status = operation == 0 ? numforge_matrix_multiply(&output, a, a) :
                operation == 1 ? numforge_matrix_determinant(det, a) : numforge_matrix_solve(&output, a, b);
            double stop = benchmark_seconds();
            calls += numforge_alloc_stats_calls(); bytes += numforge_alloc_stats_bytes();
            NumForgeBudgetFailure failure = numforge_budget_failure(); numforge_budget_end();
            if (status != NUMFORGE_MATRIX_OK || failure != NUMFORGE_BUDGET_OK || start < 0 || stop < start) goto done;
            elapsed += stop - start;
            if (operation == 1) { if (!equal(det, expected)) goto done; }
            else for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j < (operation == 0 ? n : 1); ++j)
                    if (!equal(numforge_matrix_entry(output, i, j), operation == 2 ? one :
                        i == j ? product_diagonal : product_off)) goto done;
            numforge_matrix_destroy(output); output = NULL;
        }
        printf("%s,%zu,%zu,3,%.6f,%zu,%zu,verified\n", operation == 0 ? "multiply" : operation == 1 ? "determinant" : "solve",
            n, digits, elapsed * 1000.0 / 3.0, calls / 3, bytes / 3);
    }
    outcome = 0;
done:
    free(text); free(entries); numforge_matrix_destroy(a); numforge_matrix_destroy(b); numforge_matrix_destroy(output);
    bigrational_destroy(diagonal); bigrational_destroy(off); bigrational_destroy(rhs); bigrational_destroy(expected);
    bigrational_destroy(det); bigrational_destroy(product_diagonal); bigrational_destroy(product_off); bigrational_destroy(one);
    bigint_destroy(d); bigint_destroy(power); bigint_destroy(square); bigint_destroy(unit);
    return outcome;
}
int main(void)
{
    puts("operation,dimension,denominator_exponent,repetitions,mean_ms,mean_allocations,mean_requested_bytes,verification");
    return run_case(2, 1) || run_case(8, 1) || run_case(16, 1) || run_case(2, 1000);
}
