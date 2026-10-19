#include "examples/sdk/refinement_function.h"
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>

static int run(const char *name, const char *lower, const char *width, const char *tolerance, size_t expected_count)
{
    for (unsigned repetition = 1; repetition <= 3; ++repetition) {
        RefinementFunction f;
        if (!refinement_function_init(&f, lower, width, tolerance)) return 1;
        BigRational *step = bigrational_create(); BigDecimal *number = bigdecimal_create();
        bool owner = false; int outcome = 1;
        NumForgeRefinementOptions options; numforge_refinement_options_init(&options);
        options.max_points = REFINEMENT_EXAMPLE_CAPACITY;
        char text[32]; (void)snprintf(text, sizeof(text), "%zu", expected_count - 1);
        if (!step || !number || bigrational_copy(step, f.points[1]) != BIGRATIONAL_OK ||
            bigrational_add(f.points[1], f.points[0], f.points[1]) != BIGRATIONAL_OK ||
            !refinement_function_set(f.chord, number, text) || bigrational_div(step, step, f.chord) != BIGRATIONAL_OK) goto done;
        owner = numforge_budget_begin(30000, 512U*1024U*1024U, 8U*1024U*1024U); if (!owner) goto done;
        numforge_alloc_stats_reset(); double start = benchmark_seconds(); NumForgeRefinementStats stats;
        NumForgeNumericalStatus status = numforge_refine_intervals(&stats, f.count, &options, refinement_function_probe, &f);
        double stop = benchmark_seconds(); size_t allocations = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
        if (status != NUMFORGE_NUMERICAL_OK || stats.points != expected_count || f.count != expected_count ||
            stats.probes != expected_count - 2 || stats.limits || stats.unresolved_intervals || start < 0 || stop < start) goto done;
        /* Independent closed-form expected count and spacing for this quadratic. */
        for (size_t i = 1; i < f.count; ++i) {
            int comparison = 1;
            if (bigrational_sub(f.delta, f.points[i], f.points[i - 1]) != BIGRATIONAL_OK ||
                bigrational_compare(&comparison, f.delta, step) != BIGRATIONAL_OK || comparison) goto done;
        }
        numforge_budget_end(); owner = false;
        printf("%s,%u,%zu,%zu,%.6f,%zu,%zu\n", name, repetition, stats.points, stats.probes, 1000*(stop-start), allocations, bytes);
        outcome = 0;
done:
        if (owner) numforge_budget_end();
        bigrational_destroy(step); bigdecimal_destroy(number); refinement_function_destroy(&f);
        if (outcome) return outcome;
    }
    return 0;
}
int main(void)
{
    puts("case,repeat,points,probes,milliseconds,allocation_calls,requested_bytes");
    return run("quadratic", "0", "1", "0.00006103515625", 129) ||
        run("large_offset", "1E1000", "1", "0.015625", 9) ||
        run("tiny_interval", "0", "1E-1000", "1E-2002", 17);
}
