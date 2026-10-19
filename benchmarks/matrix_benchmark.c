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
    NumForgeLinearSolution *analysis = NULL;
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
    for (unsigned operation = 0; operation < 5; ++operation) {
        double elapsed = 0; size_t calls = 0, bytes = 0;
        for (size_t repeat = 0; repeat < 3; ++repeat) {
            if (!numforge_budget_begin(5000, 128U * 1024U * 1024U, 2U * 1024U * 1024U)) goto done;
            numforge_alloc_stats_reset(); double start = benchmark_seconds();
            size_t rank = 0;
            NumForgeMatrixStatus status = operation == 0 ? numforge_matrix_multiply(&output, a, a) :
                operation == 1 ? numforge_matrix_determinant(det, a) :
                operation == 2 ? numforge_matrix_solve(&output, a, b) :
                operation == 3 ? numforge_matrix_rank(&rank, a) : numforge_matrix_solve_general(&analysis, a, b);
            double stop = benchmark_seconds();
            calls += numforge_alloc_stats_calls(); bytes += numforge_alloc_stats_bytes();
            NumForgeBudgetFailure failure = numforge_budget_failure(); numforge_budget_end();
            if (status != NUMFORGE_MATRIX_OK || failure != NUMFORGE_BUDGET_OK || start < 0 || stop < start) goto done;
            elapsed += stop - start;
            if (operation == 3) { if (rank != n) goto done; }
            else if (operation == 4) {
                if (numforge_linear_solution_kind(analysis) != NUMFORGE_LINEAR_UNIQUE ||
                    numforge_linear_solution_rank(analysis) != n || numforge_linear_solution_nullity(analysis) != 0 ||
                    numforge_linear_solution_nullspace(analysis) != NULL) goto done;
                for (size_t i = 0; i < n; ++i)
                    if (numforge_linear_solution_pivot_column(analysis, i) != i ||
                        !equal(numforge_matrix_entry(numforge_linear_solution_particular(analysis), i, 0), one)) goto done;
            }
            else if (operation == 1) { if (!equal(det, expected)) goto done; }
            else for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j < (operation == 0 ? n : 1); ++j)
                    if (!equal(numforge_matrix_entry(output, i, j), operation == 2 ? one :
                        i == j ? product_diagonal : product_off)) goto done;
            numforge_matrix_destroy(output); output = NULL;
            numforge_linear_solution_destroy(analysis); analysis = NULL;
        }
        static const char *names[] = {"multiply", "determinant", "solve", "rank", "solve_general"};
        printf("%s,%zu,%zu,3,%.6f,%zu,%zu,verified\n", names[operation],
            n, digits, elapsed * 1000.0 / 3.0, calls / 3, bytes / 3);
    }
    outcome = 0;
done:
    free(text); free(entries); numforge_matrix_destroy(a); numforge_matrix_destroy(b); numforge_matrix_destroy(output);
    numforge_linear_solution_destroy(analysis);
    bigrational_destroy(diagonal); bigrational_destroy(off); bigrational_destroy(rhs); bigrational_destroy(expected);
    bigrational_destroy(det); bigrational_destroy(product_diagonal); bigrational_destroy(product_off); bigrational_destroy(one);
    bigint_destroy(d); bigint_destroy(power); bigint_destroy(square); bigint_destroy(unit);
    return outcome;
}
static int run_family(bool inconsistent)
{
    /* A=[0 2 4; 0 3 6], b=[10; 15/16]. Rank=1, nullity=2,
     * particular=[0;5;0] when consistent, kernel=[1 0;0 -2;0 1]. */
    const size_t numerators[] = {0, 2, 4, 0, 3, 6, 10, inconsistent ? 16 : 15, 1, 5};
    BigRational *values[10] = {0}, *negative = bigrational_create(); const BigRational *entries[8];
    BigInt *one = bigint_create(); NumForgeMatrix *a = NULL, *b = NULL;
    NumForgeLinearSolution *analysis = NULL; int outcome = 1;
    if (!one || !negative || bigint_set_string(one, "1") != BIGINT_OK) goto done;
    for (size_t i = 0; i < 10; ++i) {
        values[i] = bigrational_create();
        if (!values[i] || !set_fraction(values[i], numerators[i], one)) goto done;
        if (i < 8) entries[i] = values[i];
    }
    if (bigrational_negate(negative, values[1]) != BIGRATIONAL_OK ||
        numforge_matrix_create(&a, 2, 3, entries) != NUMFORGE_MATRIX_OK ||
        numforge_matrix_create(&b, 2, 1, entries + 6) != NUMFORGE_MATRIX_OK) goto done;
    for (unsigned operation = 0; operation < 2; ++operation) {
        double elapsed = 0; size_t calls = 0, bytes = 0;
        for (size_t repeat = 0; repeat < 3; ++repeat) {
            size_t rank = 0;
            if (!numforge_budget_begin(5000, 128U * 1024U * 1024U, 2U * 1024U * 1024U)) goto done;
            numforge_alloc_stats_reset(); double start = benchmark_seconds();
            NumForgeMatrixStatus status = operation ? numforge_matrix_solve_general(&analysis, a, b) : numforge_matrix_rank(&rank, a);
            double stop = benchmark_seconds();
            calls += numforge_alloc_stats_calls(); bytes += numforge_alloc_stats_bytes();
            NumForgeBudgetFailure failure = numforge_budget_failure(); numforge_budget_end();
            if (status != NUMFORGE_MATRIX_OK || failure != NUMFORGE_BUDGET_OK || start < 0 || stop < start) goto done;
            elapsed += stop - start;
            if (!operation) { if (rank != 1) goto done; }
            else {
                if (numforge_linear_solution_kind(analysis) != (inconsistent ? NUMFORGE_LINEAR_NONE : NUMFORGE_LINEAR_INFINITE) ||
                    numforge_linear_solution_rank(analysis) != 1 || numforge_linear_solution_nullity(analysis) != 2 ||
                    numforge_linear_solution_pivot_column(analysis, 0) != 1 ||
                    numforge_linear_solution_free_column(analysis, 0) != 0 || numforge_linear_solution_free_column(analysis, 1) != 2) goto done;
                const NumForgeMatrix *particular = numforge_linear_solution_particular(analysis);
                if (inconsistent) { if (particular != NULL) goto done; }
                else for (size_t i = 0; i < 3; ++i)
                    if (!equal(numforge_matrix_entry(particular, i, 0), i == 1 ? values[9] : values[0])) goto done;
                const BigRational *expected[] = {values[8], values[0], values[0], negative, values[0], values[8]};
                for (size_t i = 0; i < 6; ++i)
                    if (!equal(numforge_matrix_entry(numforge_linear_solution_nullspace(analysis), i / 2, i % 2), expected[i])) goto done;
            }
            numforge_linear_solution_destroy(analysis); analysis = NULL;
        }
        printf("%s,2x3-%s,0,3,%.6f,%zu,%zu,verified\n", operation ? "solve_general" : "rank",
            inconsistent ? "inconsistent" : "consistent", elapsed * 1000.0 / 3, calls / 3, bytes / 3);
    }
    outcome = 0;
done:
    numforge_linear_solution_destroy(analysis); numforge_matrix_destroy(a); numforge_matrix_destroy(b);
    bigint_destroy(one); bigrational_destroy(negative);
    for (size_t i = 0; i < 10; ++i) bigrational_destroy(values[i]);
    return outcome;
}
int main(void)
{
    puts("operation,dimension,denominator_exponent,repetitions,mean_ms,mean_allocations,mean_requested_bytes,verification");
    return run_case(2, 1) || run_case(8, 1) || run_case(16, 1) || run_case(2, 1000) || run_family(false) || run_family(true);
}
