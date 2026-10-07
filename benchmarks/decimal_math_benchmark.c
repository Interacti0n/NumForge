#include "benchmark_clock.h"
#include "../src/bigdecimal/bigdecimal_internal.h"
#include "../src/internal/numforge_alloc.h"
#include "../src/internal/benchmark_profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Independent fixtures are prepared outside this process. Only the API call
 * is timed. Alias inputs are restored each iteration; memory and phase probes
 * are separate from timing samples. Internal access is only for normalization
 * of deliberately noncanonical coefficients, never for a public API client. */
typedef struct Case {
    const char *name, *op, *mode, *input;
    BigDecimal *a, *b, *expected, *tolerance;
    int64_t parameter;
    BigDecimalRoundingMode rounding;
    BigDecimalStatus expected_status;
} Case;
typedef struct Measurement {
    double seconds, phases[NUMFORGE_DECIMAL_PHASES];
    size_t calls, bytes, baseline, live, peak, iterations;
    int bounded;
} Measurement;

static BigDecimalStatus operate(const Case *c, BigDecimal *out, BigDecimal *a, BigDecimal *b)
{
    if (!strcmp(c->op, "normalize")) return bigdecimal_normalize(a);
    if (!strcmp(c->op, "add")) return bigdecimal_add(out, a, b);
    if (!strcmp(c->op, "sub")) return bigdecimal_sub(out, a, b);
    if (!strcmp(c->op, "mul")) return bigdecimal_mul(out, a, b);
    if (!strcmp(c->op, "rescale")) return bigdecimal_rescale(out, a, c->parameter, c->rounding);
    if (!strcmp(c->op, "div")) return bigdecimal_div(out, a, b, c->parameter, c->rounding);
    if (!strcmp(c->op, "divsig")) return bigdecimal_div_significant(out, a, b, c->parameter, c->rounding);
    if (!strcmp(c->op, "divexact")) return bigdecimal_div_exact_or_significant(out, a, b, c->parameter, c->rounding);
    if (!strcmp(c->op, "sqrt")) return bigdecimal_sqrt(out, a, c->parameter, c->rounding);
    if (!strcmp(c->op, "cbrt")) return bigdecimal_cbrt(out, a, c->parameter, c->rounding);
    if (!strcmp(c->op, "root")) return bigdecimal_root(out, a, (uint32_t)strtoul(c->input, NULL, 10), c->parameter, c->rounding);
    if (!strcmp(c->op, "log")) return bigdecimal_log(out, a, b, c->parameter, c->rounding);
#define UNARY(name) if (!strcmp(c->op, #name)) return bigdecimal_##name(out, a, c->parameter, c->rounding)
    UNARY(exp); UNARY(ln); UNARY(log10);
    UNARY(sin); UNARY(cos); UNARY(tan); UNARY(asin); UNARY(acos); UNARY(atan);
    UNARY(sinh); UNARY(cosh); UNARY(tanh); UNARY(asinh); UNARY(acosh); UNARY(atanh);
#undef UNARY
    if (!strcmp(c->op, "pi") || !strcmp(c->op, "e") || !strcmp(c->op, "phi"))
    {
        BigDecimalConstant kind = !strcmp(c->op, "pi") ? BIGDECIMAL_CONSTANT_PI :
            !strcmp(c->op, "e") ? BIGDECIMAL_CONSTANT_E : BIGDECIMAL_CONSTANT_PHI;
        return bigdecimal_set_constant_significant(out, kind, c->parameter, c->rounding);
    }
    return BIGDECIMAL_INVALID_ARGUMENT;
}

static int check_value(const Case *c, BigDecimal *out, BigDecimal *original, BigDecimalStatus status)
{
    int comparison;
    if (status != c->expected_status) return -1;
    if (status != BIGDECIMAL_OK)
        return bigdecimal_compare(&comparison, out, original) == BIGDECIMAL_OK && comparison == 0 ? 0 : -1;
    if (bigdecimal_compare(&comparison, out, c->expected) != BIGDECIMAL_OK) return -1;
    if (comparison == 0) return 0;
    /* Only explicitly imported low-precision directed fixtures have a nonzero
     * tolerance. New high-precision references and nearest modes are exact. */
    bool zero;
    if (c->rounding >= BIGDECIMAL_ROUND_HALF_UP ||
        bigdecimal_is_zero(&zero, c->tolerance) != BIGDECIMAL_OK || zero) return -1;
    BigDecimal *difference = bigdecimal_create();
    int bounded = difference && bigdecimal_sub(difference, out, c->expected) == BIGDECIMAL_OK &&
        bigdecimal_abs(difference, difference) == BIGDECIMAL_OK &&
        bigdecimal_compare(&comparison, difference, c->tolerance) == BIGDECIMAL_OK && comparison <= 0;
    bigdecimal_destroy(difference);
    return bounded ? 1 : -1;
}

static bool measure(const Case *c, int probe, double target, Measurement *m)
{
    BigDecimal *a = NULL, *b = NULL, *result = NULL, *original = NULL, *out;
    bool success = false;
    memset(m, 0, sizeof(*m));
    if (probe == 1 && !numforge_alloc_stats_track(true)) return false;
    a = bigdecimal_create(); b = bigdecimal_create(); result = bigdecimal_create(); original = bigdecimal_create();
    if (!a || !b || !result || !original) goto cleanup;
    out = !strcmp(c->mode, "alias_a") ? a : !strcmp(c->mode, "alias_b") ? b : result;
    if (bigdecimal_copy(a, c->a) != BIGDECIMAL_OK || bigdecimal_copy(b, c->b) != BIGDECIMAL_OK ||
        bigdecimal_set_string(result, "777") != BIGDECIMAL_OK) goto cleanup;
    if (out == result && !strcmp(c->mode, "reuse") && c->expected_status == BIGDECIMAL_OK &&
        operate(c, out, a, b) != BIGDECIMAL_OK) goto cleanup;
    do
    {
        if (bigdecimal_copy(a, c->a) != BIGDECIMAL_OK || bigdecimal_copy(b, c->b) != BIGDECIMAL_OK) goto cleanup;
        if (!strcmp(c->op, "normalize"))
        {
            if (bigint_set_string(a->coefficient, c->input) != BIGINT_OK) goto cleanup;
            a->scale = c->parameter;
        }
        if (!strcmp(c->mode, "fresh"))
        {
            bigdecimal_destroy(result); result = bigdecimal_create(); out = result;
            if (!result) goto cleanup;
        }
        if (bigdecimal_copy(original, out) != BIGDECIMAL_OK) goto cleanup;
        m->baseline = numforge_alloc_stats_live();
        numforge_alloc_stats_reset();
        if (probe == 2) numforge_decimal_profile_reset();
        double begin = benchmark_seconds();
        BigDecimalStatus status = operate(c, out, a, b);
        double end = benchmark_seconds();
        if (probe == 2) numforge_decimal_profile_stop();
        m->calls = numforge_alloc_stats_calls(); m->bytes = numforge_alloc_stats_bytes();
        m->live = numforge_alloc_stats_live(); m->peak = numforge_alloc_stats_peak();
        int checked = check_value(c, out, original, status);
        if (checked < 0)
        {
            char *observed = NULL;
            if (status == BIGDECIMAL_OK) (void)bigdecimal_to_string(out, &observed);
            fprintf(stderr, "Reference mismatch: %s status=%d actual=%s\n", c->name, (int)status,
                observed ? observed : "ERROR");
            free(observed);
            goto cleanup;
        }
        if (begin < 0 || end < begin) goto cleanup;
        m->bounded |= checked;
        m->seconds += end - begin;
        m->iterations++;
    } while (!probe && m->seconds < target && m->iterations < 4096U);
    if (probe == 2)
    {
        if (!numforge_decimal_profile_complete()) goto cleanup;
        for (int phase = 0; phase < NUMFORGE_DECIMAL_PHASES; phase++)
            m->phases[phase] = numforge_decimal_profile_seconds((NumForgeDecimalPhase)phase);
    }
    success = true;
cleanup:
    bigdecimal_destroy(a); bigdecimal_destroy(b); bigdecimal_destroy(result); bigdecimal_destroy(original);
    if (probe == 1 && (!numforge_alloc_stats_complete() || numforge_alloc_stats_live() != 0U ||
                      !numforge_alloc_stats_track(false))) success = false;
    return success;
}

static bool run(const Case *c, bool quick, bool validate)
{
    Measurement memory, phases, timing;
    double samples[5];
    size_t count = validate ? 1U : quick ? 3U : 5U;
    int bounded = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (!measure(c, 0, validate ? 0.0 : quick ? 0.0002 : 0.001, &timing)) return false;
        samples[i] = timing.seconds * 1e9 / (double)timing.iterations;
        bounded |= timing.bounded;
    }
    for (size_t i = 1; i < count; i++) for (size_t j = i; j > 0 && samples[j] < samples[j - 1]; j--)
    { double temp = samples[j]; samples[j] = samples[j - 1]; samples[j - 1] = temp; }
    if (!measure(c, 1, 0.0, &memory) || !measure(c, 2, 0.0, &phases)) return false;
    bounded |= memory.bounded | phases.bounded;
    printf("%s,%s,%s,%lld,%d,%d,%s,%zu,%.3f,%.3f,%.3f,%zu,%zu,%zu,%zu,%zu,%llu,%.3f,%.3f,%.3f,%.3f\n",
        c->name, c->op, c->mode, (long long)c->parameter, (int)c->rounding, (int)c->expected_status,
        bounded ? "bounded_directed" : "exact", count, samples[0], samples[count / 2U], samples[count - 1],
        memory.calls, memory.bytes, memory.baseline, memory.live, memory.peak, numforge_profile_process_peak(),
        phases.phases[0] * 1e9, phases.phases[1] * 1e9, phases.phases[2] * 1e9, phases.phases[3] * 1e9);
    return true;
}

int main(int argc, char **argv)
{
    bool quick = false, validate = false, success = true, found = false;
    const char *selected = NULL;
    if (argc < 2) { fputs("Usage: decimal_math_benchmark fixtures.tsv [--quick] [--validate] [--case name]\n", stderr); return 1; }
    for (int i = 2; i < argc; i++)
    {
        if (!strcmp(argv[i], "--quick")) quick = true;
        else if (!strcmp(argv[i], "--validate")) validate = true;
        else if (!strcmp(argv[i], "--case") && i + 1 < argc) selected = argv[++i];
        else return 1;
    }
    FILE *input = fopen(argv[1], "r");
    size_t capacity = 262144U;
    char *line = malloc(capacity);
    if (!input || !line) { if (input) fclose(input); free(line); return 1; }
    puts("case,operation,mode,parameter,rounding,expected_status,validation,samples,min_ns_per_op,median_ns_per_op,max_ns_per_op,alloc_calls,requested_bytes,baseline_live_bytes,live_bytes,peak_live_bytes,process_peak_bytes,diagnostic_other_ns,diagnostic_alignment_ns,diagnostic_normalize_ns,diagnostic_core_ns");
    while (fgets(line, (int)capacity, input))
    {
        if (line[0] == '#') continue;
        if (!strchr(line, '\n')) { success = false; break; }
        char *fields[10], *token = strtok(line, "\t\r\n"); size_t n = 0;
        while (token && n < 10) { fields[n++] = token; token = strtok(NULL, "\t\r\n"); }
        if (n != 10 || token) { success = false; break; }
        if (selected && strcmp(selected, fields[0])) continue;
        found = true;
        Case c = {fields[0], fields[1], fields[2], fields[3], bigdecimal_create(), bigdecimal_create(),
            bigdecimal_create(), bigdecimal_create(), (int64_t)strtoll(fields[5], NULL, 10),
            (BigDecimalRoundingMode)atoi(fields[6]), (BigDecimalStatus)atoi(fields[7])};
        /* Root degree is stored in the second operand, not the input value. */
        if (!strcmp(c.op, "root")) c.input = fields[4];
        bool valid = c.a && c.b && c.expected && c.tolerance &&
            bigdecimal_set_string(c.a, fields[3]) == BIGDECIMAL_OK && bigdecimal_set_string(c.b, fields[4]) == BIGDECIMAL_OK &&
            bigdecimal_set_string(c.tolerance, fields[8]) == BIGDECIMAL_OK && bigdecimal_set_string(c.expected, fields[9]) == BIGDECIMAL_OK;
        valid = valid && (!strcmp(c.mode, "reuse") || !strcmp(c.mode, "fresh") || !strcmp(c.mode, "alias_a") || !strcmp(c.mode, "alias_b"));
        if (!strcmp(c.op, "normalize") && strcmp(c.mode, "alias_a")) valid = false;
        if (valid) valid = run(&c, quick, validate);
        if (!valid) fprintf(stderr, "Failed independent reference or measurement: %s\n", c.name);
        bigdecimal_destroy(c.a); bigdecimal_destroy(c.b); bigdecimal_destroy(c.expected); bigdecimal_destroy(c.tolerance);
        if (!valid) { success = false; break; }
    }
    if (ferror(input)) success = false;
    fclose(input); free(line);
    return success && found ? 0 : 1;
}
