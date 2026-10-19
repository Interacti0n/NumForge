#include <numforge/polynomial.h>
#include <numforge/runtime.h>
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Exact invariants are checked after timing, never inferred from decimal text. */
static int verify(const NumForgePolynomialSolution *solution, const BigRational *a,
    const BigRational *b, const BigRational *c)
{
    BigRational *sum = bigrational_create(), *product = bigrational_create();
    BigRational *temporary = bigrational_create(), *expected = bigrational_create();
    int comparison = 1, valid = 0;
    const BigRational *center = numforge_polynomial_center(solution);
    const BigRational *radicand = numforge_polynomial_radicand(solution);
    if (sum == NULL || product == NULL || temporary == NULL || expected == NULL || center == NULL || radicand == NULL) goto done;
    if (bigrational_add(sum, center, center) != BIGRATIONAL_OK || bigrational_negate(expected, b) != BIGRATIONAL_OK ||
        bigrational_div(expected, expected, a) != BIGRATIONAL_OK || bigrational_compare(&comparison, sum, expected) != BIGRATIONAL_OK || comparison) goto done;
    if (bigrational_mul(product, center, center) != BIGRATIONAL_OK) goto done;
    if ((numforge_polynomial_is_imaginary(solution) ? bigrational_add(temporary, product, radicand) :
        bigrational_sub(temporary, product, radicand)) != BIGRATIONAL_OK ||
        bigrational_div(expected, c, a) != BIGRATIONAL_OK ||
        bigrational_compare(&comparison, temporary, expected) != BIGRATIONAL_OK || comparison) goto done;
    valid = 1;
done:
    bigrational_destroy(sum); bigrational_destroy(product); bigrational_destroy(temporary); bigrational_destroy(expected);
    return valid;
}
static int run_case(const char *label, const char *a, const char *b, const char *c)
{
    const char *texts[] = {a, b, c}; BigRational *coefficients[3] = {0};
    BigInt *number = bigint_create(); NumForgePolynomialSolution *solution = NULL;
    double total = 0.0; int outcome = 1;
    size_t calls = 0U, bytes = 0U;
    if (number == NULL) goto done;
    for (size_t i = 0U; i < 3U; ++i) {
        coefficients[i] = bigrational_create();
        if (coefficients[i] == NULL || bigint_set_string(number, texts[i]) != BIGINT_OK ||
            bigrational_from_bigint(coefficients[i], number) != BIGRATIONAL_OK) goto done;
    }
    for (size_t repeat = 0U; repeat < 10U; ++repeat) {
        if (!numforge_budget_begin(5000U, 64U*1024U*1024U, 512U*1024U)) goto done;
        numforge_alloc_stats_reset();
        double start = benchmark_seconds();
        BigRationalStatus status = numforge_polynomial_solve(&solution, coefficients[0], coefficients[1], coefficients[2]);
        double stop = benchmark_seconds();
        calls += numforge_alloc_stats_calls(); bytes += numforge_alloc_stats_bytes();
        NumForgeBudgetFailure failure = numforge_budget_failure(); numforge_budget_end();
        if (start < 0.0 || stop < start || status != BIGRATIONAL_OK || failure != NUMFORGE_BUDGET_OK ||
            numforge_polynomial_root_count(solution) == 0U || !verify(solution, coefficients[0], coefficients[1], coefficients[2])) goto done;
        total += stop - start; numforge_polynomial_destroy(solution); solution = NULL;
    }
    printf("%s,10,%.6f,%zu,%zu,verified\n", label, total*1000.0/10.0, calls/10U, bytes/10U); outcome = 0;
done:
    numforge_polynomial_destroy(solution); bigint_destroy(number);
    for (size_t i = 0U; i < 3U; ++i) bigrational_destroy(coefficients[i]);
    return outcome;
}
int main(void)
{
    char large[1002]; large[0] = '1'; memset(large+1, '0', 1000U); large[1001] = '\0';
    puts("case,repetitions,mean_ms,mean_allocations,mean_requested_bytes,verification");
    if (run_case("rational", "15", "16", "-7") || run_case("irrational", "1", "0", "-2") ||
        run_case("complex", "9", "0", "1") || run_case("large-coefficients", "1", large, "1")) return 1;
    return 0;
}
