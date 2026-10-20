#include "examples/sdk/refinement_function.h"
#include "examples/sdk/curve_function.h"
#include "examples/sdk/phase_function.h"
#include "benchmarks/common/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>
#include <string.h>

/* Fixture setup and independent checks remain outside the measured interval. */
static void record(const char *kernel, const char *name, unsigned repetition,
    size_t operations, size_t points, size_t evaluations, double seconds,
    size_t calls, size_t bytes)
{
    printf("%s,%s,%u,%zu,%zu,%zu,%.6f,%zu,%zu\n", kernel, name, repetition,
        operations, points, evaluations, 1000 * seconds, calls, bytes);
}
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
static int run_grid(const char *name, const char *offset, const char *width, size_t count)
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
        record("grid", name, repetition, count, count, 0, stop-start, allocations, bytes);
    }
    outcome = 0;
done:
    if (owner) numforge_budget_end();
    bigrational_destroy(lower); bigrational_destroy(upper); bigrational_destroy(expected);
    bigrational_destroy(factor); bigrational_destroy(two); bigrational_destroy(zero);
    bigdecimal_destroy(number); bigrational_destroy(sum.value); return outcome;
}

static int run_refinement(const char *name, const char *lower, const char *width, const char *tolerance, size_t expected_count)
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
        record("refinement", name, repetition, 1, stats.points, stats.probes, stop-start, allocations, bytes);
        outcome = 0;
done:
        if (owner) numforge_budget_end();
        bigrational_destroy(step); bigdecimal_destroy(number); refinement_function_destroy(&f);
        if (outcome) return outcome;
    }
    return 0;
}

static int run_curve(const char *name, unsigned shape)
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
        record("curve", name, repetition, 1000, 0, 0, stop-start, calls, bytes);
    }
    outcome = 0;
done:
    if (owner) numforge_budget_end();
    curve_function_destroy(&f); return outcome;
}

static int run_phase(const char *name, unsigned shape)
{
    const unsigned searches = shape == 1 ? 1 : 32;
    for (unsigned repetition = 1; repetition <= 3; ++repetition) {
        PhaseFunction f; if (!phase_function_init(&f)) return 1;
        NumForgePhaseOptions options; numforge_phase_options_init(&options); options.period = f.period; options.tolerance = f.tolerance;
        NumForgePhaseLocator *locator = NULL; NumForgePhaseReport report = {0}; bool owner = false; int outcome = 1;
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
        record("phase", name, repetition, searches, 0, f.calls, stop-start, calls, bytes); outcome = 0;
done:
        if (owner) numforge_budget_end();
        numforge_phase_locator_destroy(locator); phase_function_destroy(&f); if (outcome) return outcome;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *kernel = argc == 2 ? argv[1] : "all";
    if (argc > 2 || (strcmp(kernel, "all") && strcmp(kernel, "grid") &&
        strcmp(kernel, "refinement") && strcmp(kernel, "curve") && strcmp(kernel, "phase"))) {
        fputs("Usage: numerical_sampling_benchmark [all|grid|refinement|curve|phase]\n", stderr);
        return 2;
    }
    puts("kernel,case,repeat,operations,points,evaluations,milliseconds,allocation_calls,requested_bytes");
    if ((!strcmp(kernel, "all") || !strcmp(kernel, "grid")) && (run_grid("regular", "-10", "20", 1024) ||
        run_grid("large_offset", "1E1000", "1", 33) || run_grid("tiny_interval", "0", "1E-1000", 33))) return 1;
    if ((!strcmp(kernel, "all") || !strcmp(kernel, "refinement")) && (run_refinement("quadratic", "0", "1", "0.00006103515625", 129) ||
        run_refinement("large_offset", "1E1000", "1", "0.015625", 9) ||
        run_refinement("tiny_interval", "0", "1E-1000", "1E-2002", 17))) return 1;
    if ((!strcmp(kernel, "all") || !strcmp(kernel, "curve")) && (run_curve("linear", 0) || run_curve("phase_guard", 1) || run_curve("cancellation_1000", 2))) return 1;
    if ((!strcmp(kernel, "all") || !strcmp(kernel, "phase")) && (run_phase("linear", 0) || run_phase("quadratic", 1) || run_phase("offset_span_1000", 2))) return 1;
    return 0;
}
