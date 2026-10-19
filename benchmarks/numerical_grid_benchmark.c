#include <numforge/numerical.h>
#include <numforge/runtime.h>
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>

typedef struct Sum { BigRational *value; size_t count; } Sum;
static NumForgeNumericalStatus accumulate(const BigRational *x, size_t index, void *user)
{
    Sum *sum = user;
    if (index != sum->count) return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    ++sum->count;
    return bigrational_add(sum->value, sum->value, x) == BIGRATIONAL_OK ?
        NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
static bool set(BigRational *value, BigDecimal *number, const char *text)
{
    return bigdecimal_set_string(number, text) == BIGDECIMAL_OK &&
        bigrational_from_bigdecimal(value, number) == BIGRATIONAL_OK;
}
static int run(const char *name, const char *offset, const char *width, size_t count)
{
    BigRational *lower = bigrational_create(), *upper = bigrational_create(), *expected = bigrational_create();
    BigRational *factor = bigrational_create(), *two = bigrational_create(), *zero = bigrational_create();
    BigDecimal *number = bigdecimal_create(); Sum sum = {bigrational_create(), 0}; int outcome = 1;
    bool owner = false;
    if (!lower || !upper || !expected || !factor || !two || !zero || !number || !sum.value) goto done;
    if (!set(lower, number, offset) || !set(upper, number, width) ||
        bigrational_add(upper, lower, upper) != BIGRATIONAL_OK || !set(two, number, "2")) goto done;
    char text[32]; (void)snprintf(text, sizeof(text), "%zu", count);
    if (!set(factor, number, text) || bigrational_add(expected, lower, upper) != BIGRATIONAL_OK ||
        bigrational_mul(expected, expected, factor) != BIGRATIONAL_OK ||
        bigrational_div(expected, expected, two) != BIGRATIONAL_OK) goto done;
    for (unsigned repetition = 1; repetition <= 3; ++repetition) {
        if (bigrational_copy(sum.value, zero) != BIGRATIONAL_OK) goto done;
        sum.count = 0; owner = numforge_budget_begin(30000, 512U*1024U*1024U, 8U*1024U*1024U);
        if (!owner) goto done;
        numforge_alloc_stats_reset(); double start = benchmark_seconds();
        NumForgeNumericalStatus status = numforge_grid_visit(lower, upper, count, accumulate, &sum);
        double stop = benchmark_seconds(); size_t allocations = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
        int comparison = 1;
        if (status != NUMFORGE_NUMERICAL_OK || sum.count != count || start < 0 || stop < start ||
            bigrational_compare(&comparison, sum.value, expected) != BIGRATIONAL_OK || comparison) goto done;
        numforge_budget_end(); owner = false;
        printf("%s,%u,%zu,%.6f,%zu,%zu\n", name, repetition, count, 1000*(stop-start), allocations, bytes);
    }
    outcome = 0;
done:
    if (owner) numforge_budget_end();
    bigrational_destroy(lower); bigrational_destroy(upper); bigrational_destroy(expected);
    bigrational_destroy(factor); bigrational_destroy(two); bigrational_destroy(zero);
    bigdecimal_destroy(number); bigrational_destroy(sum.value); return outcome;
}
int main(void)
{
    puts("case,repeat,points,milliseconds,allocation_calls,requested_bytes");
    return run("regular", "-10", "20", 1024) ||
        run("large_offset", "1E1000", "1", 33) || run("tiny_interval", "0", "1E-1000", 33);
}
