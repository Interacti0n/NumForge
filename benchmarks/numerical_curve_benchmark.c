#include "examples/sdk/curve_function.h"
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>

static int run(const char *name, unsigned shape)
{
    CurveFunction f; if (!curve_function_init(&f)) return 1;
    bool owner = false; int outcome = 1;
    NumForgeRefinementDecision expected = NUMFORGE_REFINEMENT_ACCEPT;
    if (shape == 1) {
        if (bigdecimal_set_string(f.values[12], "4") != BIGDECIMAL_OK) goto done;
        expected = NUMFORGE_REFINEMENT_SPLIT;
    } else if (shape == 2) {
        if (bigdecimal_set_string(f.values[7], "1E1000") != BIGDECIMAL_OK ||
            bigdecimal_set_string(f.values[9], "1E1000") != BIGDECIMAL_OK ||
            bigdecimal_set_string(f.values[8], "1E-1000") != BIGDECIMAL_OK ||
            bigdecimal_add(f.values[8], f.values[8], f.values[7]) != BIGDECIMAL_OK ||
            bigdecimal_set_string(f.values[0], "1E-1001") != BIGDECIMAL_OK || !curve_function_policy(&f)) goto done;
        expected = NUMFORGE_REFINEMENT_SPLIT;
    }
    for (unsigned repetition = 1; repetition <= 3; ++repetition) {
        owner = numforge_budget_begin(30000, 512U*1024U*1024U, 8U*1024U*1024U); if (!owner) goto done;
        numforge_alloc_stats_reset(); double start = benchmark_seconds(); NumForgeCurveAssessment result;
        for (unsigned i = 0; i < 1000; ++i)
            if (numforge_curve_assess(&result, f.classifier, f.samples) != NUMFORGE_NUMERICAL_OK) goto done;
        double stop = benchmark_seconds(); size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
        if (result.decision != expected || result.screen_clipped || start < 0 || stop < start) goto done;
        numforge_budget_end(); owner = false;
        printf("%s,%u,1000,%.6f,%zu,%zu\n", name, repetition, 1000*(stop-start), calls, bytes);
    }
    outcome = 0;
done:
    if (owner) numforge_budget_end();
    curve_function_destroy(&f); return outcome;
}
int main(void)
{
    puts("case,repeat,assessments,milliseconds,allocation_calls,requested_bytes");
    return run("linear", 0) || run("phase_guard", 1) || run("cancellation_1000", 2);
}
