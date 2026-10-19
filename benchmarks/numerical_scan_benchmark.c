#include "examples/sdk/numerical_function.h"
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <numforge/runtime.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct Function { NumericalFunction base; BigRational *center; size_t calls; bool contact; } Function;
static NumForgeNumericalStatus evaluate(BigDecimal *value, BigRational *proposal,
    NumForgeRootSample *sample, const BigRational *x, int64_t digits, void *user)
{
    Function *f = user; ++f->calls;
    if (!f->contact) return numerical_function_scan_evaluate(value, proposal, sample, x, digits, &f->base);
    BigRationalStatus status = bigrational_sub(f->base.value, x, f->center); int sign = 1;
    if (status == BIGRATIONAL_OK) status = bigrational_mul(f->base.value, f->base.value, f->base.value);
    if (status == BIGRATIONAL_OK) status = bigrational_compare(&sign, f->base.value, f->base.zero);
    if (status == BIGRATIONAL_OK) status = bigrational_to_bigdecimal(value, f->base.value, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    if (status == BIGRATIONAL_OK && proposal) status = bigrational_copy(proposal, f->base.value);
    sample->real = true; sample->exact_zero = sign == 0;
    return status == BIGRATIONAL_OK ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_CALLBACK_FAILED;
}
static int set(BigRational *value, BigDecimal *temporary, const char *text)
{ return bigdecimal_set_string(temporary, text) == BIGDECIMAL_OK && bigrational_from_bigdecimal(value, temporary) == BIGRATIONAL_OK; }
static int run_case(const char *name, size_t segments, unsigned shape)
{
    Function f = {0}; NumForgeRootScan *scan = NULL;
    BigRational *lower = bigrational_create(), *upper = bigrational_create(), *one = bigrational_create(), *three = bigrational_create();
    BigDecimal *expected = bigdecimal_create(), *actual = bigdecimal_create(), *tolerance = bigdecimal_create();
    bool budget = false; int outcome = 1; f.center = bigrational_create(); f.contact = shape != 0;
    if (!numerical_function_init(&f.base) || !f.center || !lower || !upper || !one || !three || !expected || !actual || !tolerance) goto done;
    if (!set(one, actual, "1") || !set(three, actual, "3") || bigrational_div(f.center, one, three) != BIGRATIONAL_OK) goto done;
    if (shape == 0) {
        if (bigrational_negate(lower, f.base.two) != BIGRATIONAL_OK || bigrational_copy(upper, f.base.two) != BIGRATIONAL_OK) goto done;
    } else if (shape == 2) {
        if (!set(lower, actual, "1E1000") || bigrational_add(upper, lower, one) != BIGRATIONAL_OK ||
            bigrational_add(f.center, f.center, lower) != BIGRATIONAL_OK) goto done;
    } else if (shape == 3) {
        if (!set(upper, actual, "1E-1000") || !set(f.center, actual, "1E-1001")) goto done;
    } else if (bigrational_copy(upper, one) != BIGRATIONAL_OK) goto done;
    if (bigdecimal_set_string(tolerance, "1E-12") != BIGDECIMAL_OK || bigdecimal_set_string(expected, "2") != BIGDECIMAL_OK ||
        bigdecimal_sqrt(expected, expected, 40, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK) goto done;
    NumForgeRootScanOptions options; numforge_root_scan_options_init(&options); options.segments = segments;
    for (unsigned repetition = 1; repetition <= 3; ++repetition) {
        f.calls = 0; budget = numforge_budget_begin(30000, 512U*1024U*1024U, 8U*1024U*1024U); if (!budget) goto done;
        numforge_alloc_stats_reset(); double start = benchmark_seconds();
        NumForgeNumericalStatus status = numforge_roots_scan(&scan, evaluate,
            f.contact ? NULL : numerical_function_domain, &f, lower, upper, &options);
        double stop = benchmark_seconds(); size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
        size_t count = f.contact ? 1 : 2; NumForgeRootScanStats stats; numforge_root_scan_stats(scan, &stats);
        if (status != NUMFORGE_NUMERICAL_OK || numforge_root_scan_count(scan) != count ||
            numforge_root_scan_stop(scan) != NUMFORGE_SCAN_FINISHED || start < 0 || stop < start || stats.evaluations != f.calls) goto done;
        for (size_t i = 0; i < count; ++i) {
            const NumForgeRoot *root = numforge_root_scan_root(scan, i); int order = 1;
            if (f.contact) {
                if (!numforge_root_exact_zero(root) || bigrational_compare(&order, f.center, numforge_root_x(root)) != BIGRATIONAL_OK || order) goto done;
            } else {
                if (bigrational_to_bigdecimal(actual, numforge_root_x(root), 40, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK ||
                    bigdecimal_abs(actual, actual) != BIGDECIMAL_OK || bigdecimal_sub(actual, actual, expected) != BIGDECIMAL_OK ||
                    bigdecimal_abs(actual, actual) != BIGDECIMAL_OK || bigdecimal_compare(&order, actual, tolerance) != BIGDECIMAL_OK || order > 0 ||
                    bigdecimal_compare(&order, numforge_root_residual(root), tolerance) != BIGDECIMAL_OK || order > 0) goto done;
            }
        }
        if (numforge_budget_failure() != NUMFORGE_BUDGET_OK) goto done;
        printf("%s,%zu,%u,%.6f,%zu,%zu,%zu,%zu,%zu,verified\n", name, segments, repetition,
            (stop-start)*1000, count, stats.evaluations, stats.candidate_samples, calls, bytes);
        numforge_budget_end(); budget = false; numforge_root_scan_destroy(scan); scan = NULL;
    }
    outcome = 0;
done:
    if (budget) numforge_budget_end();
    numerical_function_destroy(&f.base); bigrational_destroy(f.center); numforge_root_scan_destroy(scan);
    bigrational_destroy(lower); bigrational_destroy(upper); bigrational_destroy(one); bigrational_destroy(three);
    bigdecimal_destroy(expected); bigdecimal_destroy(actual); bigdecimal_destroy(tolerance); return outcome;
}
int main(void)
{
    puts("case,segments,repeat,elapsed_ms,roots,evaluations,candidate_samples,allocations,requested_bytes,verification");
    return run_case("quadratic", 128, 0) || run_case("two-in-one-cell", 1, 0) ||
        run_case("rational-contact", 128, 1) || run_case("offset1000", 128, 2) || run_case("tiny1000", 128, 3);
}
