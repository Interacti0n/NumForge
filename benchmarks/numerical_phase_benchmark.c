#include "examples/sdk/phase_function.h"
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>

static int run(const char *name, unsigned shape)
{
    const unsigned searches = shape == 1 ? 1 : 32;
    for (unsigned repetition = 1; repetition <= 3; ++repetition) {
        PhaseFunction f; if (!phase_function_init(&f)) return 1;
        NumForgePhaseOptions options; numforge_phase_options_init(&options); options.period = f.period; options.tolerance = f.tolerance;
        NumForgePhaseLocator *locator = NULL; NumForgePhaseReport report; bool owner = false; int outcome = 1;
        if (shape == 1) {
            f.quadratic = true; f.decimal_places = options.significant_digits = 64; options.max_evaluations = 128;
            if (bigdecimal_set_string(f.tolerance, "1E-30") != BIGDECIMAL_OK) goto done;
        } else if (shape == 2) {
            f.decimal_places = options.significant_digits = 1010;
            if (!phase_function_range(&f, "1E1000", "1E-1000", "0", "3") || bigdecimal_set_string(f.tolerance, "1E-1000") != BIGDECIMAL_OK) goto done;
        }
        if (numforge_phase_locator_create(&locator, &options) != NUMFORGE_NUMERICAL_OK) goto done;
        owner = numforge_budget_begin(30000, 512U*1024U*1024U, 8U*1024U*1024U); if (!owner) goto done;
        numforge_alloc_stats_reset(); double start = benchmark_seconds();
        for (unsigned i = 0; i < searches; ++i)
            if (numforge_phase_locate(&report, locator, f.lower, f.upper, f.phase_lower, f.phase_upper,
                f.offset, phase_function_evaluate, &f) != NUMFORGE_NUMERICAL_OK || report.kind != NUMFORGE_PHASE_FOUND) goto done;
        double stop = benchmark_seconds(); size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
        int comparison = 1;
        if (start < 0 || stop < start || f.calls != searches*report.evaluations ||
            bigrational_compare(&comparison, f.last, numforge_phase_locator_x(locator)) != BIGRATIONAL_OK || comparison) goto done;
        if (shape == 2) {
            /* Closed-form ideal coordinate is lower+span/6. */
            if (bigrational_sub(f.temporary, f.last, f.lower) != BIGRATIONAL_OK ||
                bigrational_div(f.temporary, f.temporary, f.span) != BIGRATIONAL_OK || !phase_function_set(&f, f.factor, "6") ||
                bigrational_mul(f.temporary, f.temporary, f.factor) != BIGRATIONAL_OK ||
                bigrational_to_bigdecimal(f.scratch, f.temporary, 1010, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK ||
                bigdecimal_set_string(f.phase_lower, "1") != BIGDECIMAL_OK || bigdecimal_set_string(f.phase_upper, "1E-998") != BIGDECIMAL_OK) goto done;
        } else {
            if (bigrational_to_bigdecimal(f.scratch, f.last, 40, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK ||
                (shape == 1 ? bigdecimal_sqrt(f.phase_lower, f.offset, 40, BIGDECIMAL_ROUND_HALF_EVEN) : bigdecimal_copy(f.phase_lower, f.offset)) != BIGDECIMAL_OK ||
                bigdecimal_set_string(f.phase_upper, "1E-28") != BIGDECIMAL_OK) goto done;
        }
        if (bigdecimal_sub(f.scratch, f.scratch, f.phase_lower) != BIGDECIMAL_OK || bigdecimal_abs(f.scratch, f.scratch) != BIGDECIMAL_OK ||
            bigdecimal_compare(&comparison, f.scratch, f.phase_upper) != BIGDECIMAL_OK || comparison > 0) goto done;
        numforge_budget_end(); owner = false;
        printf("%s,%u,%u,%zu,%.6f,%zu,%zu\n", name, repetition, searches, f.calls, 1000*(stop-start), calls, bytes); outcome = 0;
done:
        if (owner) numforge_budget_end();
        numforge_phase_locator_destroy(locator); phase_function_destroy(&f); if (outcome) return outcome;
    }
    return 0;
}
int main(void)
{
    puts("case,repeat,searches,evaluations,milliseconds,allocation_calls,requested_bytes");
    return run("linear", 0) || run("quadratic", 1) || run("offset_span_1000", 2);
}
