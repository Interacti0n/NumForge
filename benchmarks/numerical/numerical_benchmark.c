#include "examples/sdk/numerical_function.h"
#include "benchmarks/common/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include "src/runtime/instrumentation/decimal_profile.h"
#include <numforge/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int run_case(size_t digits, bool profile, size_t allocation_limit)
{
    NumericalFunction f = {0}; NumForgeRoot *root = NULL;
    BigRational *lower = bigrational_create(), *width = bigrational_create();
    BigDecimal *tolerance = bigdecimal_create(), *expected = bigdecimal_create(), *actual = bigdecimal_create();
    char text[40]; int outcome = 1, order = 1; bool budget = false;
    if (!numerical_function_init(&f) || !lower || !width || !tolerance || !expected || !actual) goto done;
    (void)snprintf(text, sizeof(text), "1E-%zu", digits);
    if (bigdecimal_set_string(tolerance, text) != BIGDECIMAL_OK ||
        bigrational_from_bigdecimal(width, tolerance) != BIGRATIONAL_OK) goto done;
    NumForgeRootOptions options; numforge_root_options_init(&options);
    options.decimal_places = (int64_t)digits + 20; options.max_iterations = digits * 4 + 32;
    options.max_evaluations = options.max_iterations + 8;
    options.x_tolerance = width; options.residual_tolerance = tolerance;
    /* Cumulative requested bytes can exceed live memory substantially at 1000
     * places; report that traffic instead of imposing the application's cap. */
    budget = numforge_budget_begin(120000, allocation_limit, 8U*1024U*1024U);
    if (!budget) goto done;
    numforge_alloc_stats_reset();
    if (profile) numforge_decimal_profile_reset();
    double start = benchmark_seconds();
    NumForgeNumericalStatus status = numforge_root_solve(&root, numerical_function_evaluate,
        numerical_function_domain, &f, lower, f.two, &options);
    double stop = benchmark_seconds();
    if (profile) {
        numforge_decimal_profile_stop();
        const char *names[] = {"add","sub","mul","div","mod","pow","gcd","divmod"};
        for (int i = 0; i < NUMFORGE_DECIMAL_OPERATIONS; ++i)
            fprintf(stderr,"profile,%zu,%s,%zu,%.3f\n",digits,names[i],
                numforge_decimal_operation_calls((NumForgeDecimalOperation)i),
                numforge_decimal_operation_seconds((NumForgeDecimalOperation)i)*1e9);
        for (int i = 0; i < NUMFORGE_DECIMAL_PHASES; ++i)
            fprintf(stderr,"phase,%zu,%d,%.3f\n",digits,i,numforge_decimal_profile_seconds((NumForgeDecimalPhase)i)*1e9);
        if (!numforge_decimal_profile_complete()) goto done;
    }
    size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
    if (status != NUMFORGE_NUMERICAL_OK || numforge_root_kind(root) != NUMFORGE_ROOT_FOUND || start < 0 || stop < start) {
        fprintf(stderr,"root_stop,%zu,status=%d,budget=%d,requested_bytes=%zu\n",digits,(int)status,(int)numforge_budget_failure(),bytes);
        goto done;
    }
    /* Independent Newton sqrt implementation and original residual, outside timing. */
    if (bigdecimal_set_string(expected, "2") != BIGDECIMAL_OK ||
        bigdecimal_sqrt(expected, expected, (int64_t)digits + 10, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigrational_to_bigdecimal(actual, numforge_root_x(root), (int64_t)digits + 10, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK ||
        bigdecimal_sub(actual, actual, expected) != BIGDECIMAL_OK || bigdecimal_abs(actual, actual) != BIGDECIMAL_OK ||
        bigdecimal_compare(&order, actual, tolerance) != BIGDECIMAL_OK || order > 0 ||
        bigdecimal_compare(&order, numforge_root_residual(root), tolerance) != BIGDECIMAL_OK || order > 0 ||
        numforge_budget_failure() != NUMFORGE_BUDGET_OK) goto done;
    printf("sqrt2,%zu,%.6f,%zu,%zu,%zu,%zu,verified\n", digits, (stop-start)*1000,
        numforge_root_iterations(root), numforge_root_evaluations(root), calls, bytes); outcome = 0;
done:
    if (budget) numforge_budget_end();
    numerical_function_destroy(&f); numforge_root_destroy(root); bigrational_destroy(lower); bigrational_destroy(width);
    bigdecimal_destroy(tolerance); bigdecimal_destroy(expected); bigdecimal_destroy(actual); return outcome;
}
int main(int argc, char **argv)
{
    bool high = false, profile = false;
    size_t limit = SIZE_MAX;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i],"--high-precision")) high = true;
        else if (!strcmp(argv[i],"--profile")) profile = true;
        else if (!strcmp(argv[i],"--allocation-limit-mib") && i+1 < argc) {
            char *end; unsigned long mib = strtoul(argv[++i], &end, 10);
            if (!mib || mib > 1024 || *end) return 2;
            limit = (size_t)mib * 1048576U;
        } else return 2;
    }
    puts("case,absolute_digits,elapsed_ms,iterations,evaluations,allocations,requested_bytes,verification");
    if (run_case(12, profile, limit) || run_case(100, profile, limit)) return 1;
    return high ? run_case(1000, profile, limit) : 0;
}
