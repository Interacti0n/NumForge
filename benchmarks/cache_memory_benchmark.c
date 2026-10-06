#include "benchmark_clock.h"
#include "web_api.h"
#include "session.h"
#include "../src/internal/numforge_alloc.h"
#include "../src/internal/benchmark_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Each sample builds a fresh fixture, then times one selected transition.
 * --case makes process peak interpretable in a fresh process. Memory tracking
 * is deliberately separate from latency and phase sampling. */
static const char *cases[] = {
    "legacy_cold", "legacy_hit", "legacy_precision", "legacy_notation",
    "legacy_angle", "legacy_expression", "legacy_stale", "legacy_approx_precision",
    "session_cold", "session_hit", "session_commit", "session_random",
    "rescale_fresh", "rescale_reused"
};
static bool invoke(const char *name, bool memory, double *seconds,
                   double *phases, size_t *calls, size_t *bytes,
                   size_t *baseline, size_t *live, size_t *peak)
{
    NumForgeWebCache cache = {0};
    CalculatorSession session = {0};
    CalculatorContext context;
    CalculatorError error;
    BigDecimal *source = NULL, *destination = NULL;
    char *output = NULL, *expected = NULL;
    bool reused = false;
    bool success = false;
    bool legacy = strncmp(name, "legacy_", 7U) == 0;
    bool buffer = strncmp(name, "rescale_", 8U) == 0;
    const char *input = strcmp(name, "legacy_approx_precision") == 0 ? "sqrt(2)" : "1/3";
    int64_t places = 10;
    CalculatorNotation notation = CALCULATOR_NOTATION_AUTO;
    CalculatorAngleUnit angle = CALCULATOR_ANGLE_RADIANS;
    uint64_t revision = 2U;
    bool expect_hit = false;
    double begin;
    calculator_context_init(&context);
    if (memory && !numforge_alloc_stats_track(true)) return false;
    if (buffer)
    {
        source = bigdecimal_create();
        if (source == NULL || bigdecimal_set_string(source, "12345.6789012345") != BIGDECIMAL_OK) goto cleanup;
        if (strcmp(name, "rescale_reused") == 0)
        {
            destination = bigdecimal_create();
            if (destination == NULL || bigdecimal_rescale(destination, source, 4,
                BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK) goto cleanup;
        }
    }
    else if (legacy && strcmp(name, "legacy_cold") != 0)
    {
        if (numforge_web_evaluate_cached_mode(&cache, 1, input, 10, angle, notation,
            &output, &error, &reused) != CALCULATOR_OK || reused) goto cleanup;
        free(output); output = NULL;
        expect_hit = true;
        if (strcmp(name, "legacy_precision") == 0) places = 100;
        if (strcmp(name, "legacy_notation") == 0) notation = CALCULATOR_NOTATION_SCIENTIFIC;
        if (strcmp(name, "legacy_angle") == 0) { angle = CALCULATOR_ANGLE_DEGREES; expect_hit = false; }
        if (strcmp(name, "legacy_expression") == 0) { input = "2/3"; expect_hit = false; }
        if (strcmp(name, "legacy_stale") == 0) revision = 0;
        if (strcmp(name, "legacy_approx_precision") == 0) { places = 100; expect_hit = false; }
    }
    else if (!legacy && !buffer && strcmp(name, "session_cold") != 0)
    {
        if (strcmp(name, "session_random") == 0) input = "rand()";
        if (calculator_session_compute(&session, 1, false, input, &context,
            &output, &error, &reused) != CALCULATOR_OK || reused) goto cleanup;
        expected = output; output = NULL;
        expect_hit = true;
    }
    *baseline = numforge_alloc_stats_live();
    numforge_alloc_stats_reset();
    if (!memory) numforge_profile_reset();
    begin = benchmark_seconds();
    if (begin < 0.0) goto cleanup;
    if (buffer)
    {
        if (destination == NULL) destination = bigdecimal_create();
        if (destination == NULL || bigdecimal_rescale(destination, source, 4,
            BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK) goto cleanup;
    }
    else if (legacy)
    {
        if (numforge_web_evaluate_cached_mode(&cache, revision, input, places, angle, notation,
            &output, &error, &reused) != CALCULATOR_OK) goto cleanup;
    }
    else
    {
        if (calculator_session_compute(&session, 2, strcmp(name, "session_commit") == 0,
            input, &context, &output, &error, &reused) != CALCULATOR_OK) goto cleanup;
    }
    *seconds = benchmark_seconds() - begin;
    if (!memory)
    {
        numforge_profile_stop();
        if (!numforge_profile_complete()) goto cleanup;
        for (int phase = 0; phase < NUMFORGE_PHASE_COUNT; phase++)
            phases[phase] = numforge_profile_seconds((NumForgeProfilePhase)phase);
    }
    *calls = numforge_alloc_stats_calls(); *bytes = numforge_alloc_stats_bytes();
    *live = numforge_alloc_stats_live(); *peak = numforge_alloc_stats_peak();
    /* Validation stays outside the timed region. */
    if (buffer)
    {
        if (bigdecimal_to_string(destination, &output) != BIGDECIMAL_OK ||
            strcmp(output, "12345.6789") != 0) goto cleanup;
    }
    else
    {
        if (reused != expect_hit) goto cleanup;
        if (strcmp(name, "session_random") == 0)
        {
            if (strcmp(output, expected) != 0) goto cleanup;
        }
        else
        {
            char *fresh = NULL;
            if (numforge_web_evaluate_cached_mode(NULL, 0, input, places, angle, notation,
                &fresh, &error, NULL) != CALCULATOR_OK) { free(fresh); goto cleanup; }
            success = strcmp(output, fresh) == 0;
            free(fresh);
            if (!success) goto cleanup;
        }
        if (strcmp(name, "session_commit") == 0 && session.count != 1U) goto cleanup;
        if (strcmp(name, "legacy_stale") == 0 && cache.revision != 1U) goto cleanup;
    }
    success = *seconds >= 0.0;
cleanup:
    if (!memory) numforge_profile_stop();
    free(output); free(expected);
    numforge_web_cache_clear(&cache); calculator_session_destroy(&session);
    bigdecimal_destroy(source); bigdecimal_destroy(destination);
    if (memory)
    {
        if (numforge_alloc_stats_live() != 0U || !numforge_alloc_stats_complete() ||
            !numforge_alloc_stats_track(false)) success = false;
    }
    return success;
}

static int run(const char *name, bool quick)
{
    double samples[7], phases[NUMFORGE_PHASE_COUNT] = {0};
    size_t count = quick ? 3U : 7U;
    size_t calls, bytes, baseline, live, peak;
    double ignored;
    for (size_t i = 0; i < count; i++)
    {
        if (!invoke(name, false, &samples[i], phases, &calls, &bytes, &baseline, &live, &peak)) return 1;
    }
    for (size_t i = 1; i < count; i++)
        for (size_t j = i; j > 0 && samples[j] < samples[j - 1]; j--)
        { double swap = samples[j]; samples[j] = samples[j - 1]; samples[j - 1] = swap; }
    if (!invoke(name, true, &ignored, phases, &calls, &bytes, &baseline, &live, &peak)) return 1;
    printf("%s,%zu,%.3f,%.3f,%.3f,%zu,%zu,%zu,%zu,%zu,%llu", name, count,
        samples[0] * 1e9, samples[count / 2] * 1e9, samples[count - 1] * 1e9,
        calls, bytes, baseline, live, peak, numforge_profile_process_peak());
    for (int phase = 0; phase < NUMFORGE_PHASE_COUNT; phase++) printf(",%.3f", phases[phase] * 1e9);
    putchar('\n');
    return 0;
}
int main(int argc, char **argv)
{
    const char *selected = NULL;
    bool quick = false, found = false;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--quick") == 0) quick = true;
        else if (strcmp(argv[i], "--case") == 0 && i + 1 < argc) selected = argv[++i];
        else { fputs("Usage: cache_memory_benchmark [--quick] [--case name]\n", stderr); return 1; }
    }
    puts("case,samples,min_ns,median_ns,max_ns,alloc_calls,requested_bytes,baseline_live_bytes,"
         "live_after_bytes,peak_tracked_payload_bytes,process_peak_bytes,other_ns,parse_ns,"
         "evaluate_ns,format_ns,convert_ns,round_ns,compose_ns,serialize_ns,send_ns");
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (selected != NULL && strcmp(selected, cases[i]) != 0) continue;
        found = true;
        if (run(cases[i], quick)) { fprintf(stderr, "Failed: %s\n", cases[i]); return 1; }
    }
    return found ? 0 : 1;
}
