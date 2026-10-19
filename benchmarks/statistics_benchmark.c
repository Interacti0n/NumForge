#include <numforge/statistics.h>
#include <numforge/runtime.h>
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int set_size(BigRational *value, size_t number)
{
    char text[32]; BigInt *integer = bigint_create(); int ok = 0;
    (void)snprintf(text, sizeof(text), "%zu", number);
    if (integer && bigint_set_string(integer, text) == BIGINT_OK && bigrational_from_bigint(value, integer) == BIGRATIONAL_OK) ok = 1;
    bigint_destroy(integer); return ok;
}
static int power(BigInt *value, size_t exponent)
{
    char *text = malloc(exponent + 2); if (!text) return 0;
    text[0] = '1'; memset(text + 1, '0', exponent); text[exponent + 1] = '\0';
    int ok = bigint_set_string(value, text) == BIGINT_OK; free(text); return ok;
}
static int equal(const BigRational *a, const BigRational *b)
{
    int comparison = 1;
    return bigrational_compare(&comparison, a, b) == BIGRATIONAL_OK && comparison == 0;
}
typedef const BigRational *(*Accessor)(const NumForgeStatisticsSummary *);
static const Accessor accessors[] = {numforge_statistics_summary_sum, numforge_statistics_summary_minimum,
    numforge_statistics_summary_maximum, numforge_statistics_summary_mean,
    numforge_statistics_summary_variance_population, numforge_statistics_summary_variance_sample};

/* x_j = offset + j/d, j=0..n-1. Independent closed forms outside timing:
 * mean=offset+(n-1)/(2*d), population variance=(n*n-1)/(12*d*d),
 * sample variance=n*(n+1)/(12*d*d). This corpus has bounded n<=1024. */
static int run_case(const char *label, size_t n, size_t offset_exponent, size_t spread_exponent)
{
    BigRational **values = calloc(n, sizeof(*values)); const BigRational **borrowed = malloc(n * sizeof(*borrowed));
    BigRational *offset = bigrational_create(), *scale = bigrational_create(), *term = bigrational_create(), *denominator = bigrational_create();
    BigRational *expected[6] = {0}; BigInt *integer = bigint_create(); NumForgeStatisticsSummary *summary = NULL;
    int outcome = 1; size_t calls = 0, bytes = 0; double elapsed = 0;
    if (!values || !borrowed || !offset || !scale || !term || !denominator || !integer) goto done;
    if (offset_exponent && (!power(integer, offset_exponent) || bigrational_from_bigint(offset, integer) != BIGRATIONAL_OK)) goto done;
    if (!power(integer, spread_exponent) || bigrational_from_bigint(scale, integer) != BIGRATIONAL_OK) goto done;
    for (size_t i = 0; i < n; ++i) {
        values[i] = bigrational_create(); borrowed[i] = values[i];
        if (!values[i] || !set_size(values[i], i) || bigrational_div(values[i], values[i], scale) != BIGRATIONAL_OK ||
            bigrational_add(values[i], values[i], offset) != BIGRATIONAL_OK) goto done;
    }
    for (size_t i = 0; i < (n == 1 ? 5U : 6U); ++i) { expected[i] = bigrational_create(); if (!expected[i]) goto done; }
    if (bigrational_copy(expected[1], offset) != BIGRATIONAL_OK || !set_size(term, n - 1) ||
        bigrational_div(term, term, scale) != BIGRATIONAL_OK || bigrational_add(expected[2], offset, term) != BIGRATIONAL_OK ||
        !set_size(denominator, 2) || bigrational_div(term, term, denominator) != BIGRATIONAL_OK ||
        bigrational_add(expected[3], offset, term) != BIGRATIONAL_OK || !set_size(term, n) ||
        bigrational_mul(expected[0], term, expected[3]) != BIGRATIONAL_OK) goto done;
    if (bigrational_mul(term, scale, scale) != BIGRATIONAL_OK || !set_size(denominator, 12) ||
        bigrational_mul(denominator, denominator, term) != BIGRATIONAL_OK || !set_size(expected[4], n*n - 1) ||
        bigrational_div(expected[4], expected[4], denominator) != BIGRATIONAL_OK) goto done;
    if (n > 1 && (!set_size(expected[5], n*(n+1)) || bigrational_div(expected[5], expected[5], denominator) != BIGRATIONAL_OK)) goto done;
    for (size_t repeat = 0; repeat < 3; ++repeat) {
        if (!numforge_budget_begin(5000, 128U * 1024U * 1024U, 2U * 1024U * 1024U)) goto done;
        numforge_alloc_stats_reset(); double start = benchmark_seconds();
        BigRationalStatus status = numforge_statistics_describe(&summary, borrowed, n);
        double stop = benchmark_seconds(); calls += numforge_alloc_stats_calls(); bytes += numforge_alloc_stats_bytes();
        NumForgeBudgetFailure failure = numforge_budget_failure(); numforge_budget_end();
        if (status != BIGRATIONAL_OK || failure != NUMFORGE_BUDGET_OK || start < 0 || stop < start) goto done;
        elapsed += stop - start;
        if (numforge_statistics_summary_count(summary) != n) goto done;
        for (size_t i = 0; i < 6; ++i) {
            const BigRational *actual = accessors[i](summary);
            if (expected[i] == NULL) { if (actual != NULL) goto done; }
            else if (!actual || !equal(actual, expected[i])) goto done;
        }
        numforge_statistics_summary_destroy(summary); summary = NULL;
    }
    printf("%s,%zu,%zu,%zu,3,%.6f,%zu,%zu,verified\n", label, n, offset_exponent, spread_exponent, elapsed * 1000.0 / 3, calls / 3, bytes / 3);
    outcome = 0;
done:
    if (outcome) fprintf(stderr, "statistics benchmark failed: %s\n", label);
    if (values) for (size_t i = 0; i < n; ++i) bigrational_destroy(values[i]);
    free(values); free(borrowed); bigrational_destroy(offset); bigrational_destroy(scale); bigrational_destroy(term); bigrational_destroy(denominator);
    for (size_t i = 0; i < 6; ++i) bigrational_destroy(expected[i]);
    bigint_destroy(integer); numforge_statistics_summary_destroy(summary); return outcome;
}
int main(void)
{
    puts("case,count,offset_exponent,spread_exponent,repetitions,mean_ms,mean_allocations,mean_requested_bytes,verification");
    return run_case("singleton", 1, 0, 0) || run_case("small", 16, 0, 0) || run_case("medium", 128, 0, 0) ||
        run_case("large", 1024, 0, 0) || run_case("offset1000", 128, 1000, 0) || run_case("tiny1000", 128, 0, 1000);
}
