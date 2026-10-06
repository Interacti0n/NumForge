#include "benchmark_clock.h"
#include "../src/internal/benchmark_profile.h"
#include "../src/internal/numforge_alloc.h"

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Direct decimal conversion and formatting benchmark. Fixture construction
    and result validation are outside each timed call. Output allocation and
    destruction remain included because they are part of the public API path.
------------------------------------------------------------------------------------------------------------------------------
*/

#define FORMAT_BENCHMARK_MAX_SAMPLES 7U
#define FORMAT_BENCHMARK_MAX_ITERATIONS 262144U

typedef enum FormatOperation
{
    FORMAT_BIGINT_FULL,
    FORMAT_DECIMAL_SCIENTIFIC_10,
    FORMAT_DECIMAL_SCIENTIFIC_100,
    FORMAT_DECIMAL_SCIENTIFIC_FULL,
    FORMAT_DECIMAL_AUTO_10,
    FORMAT_DECIMAL_AUTO_FULL,
    FORMAT_DECIMAL_MODE_SCIENTIFIC_10,
    FORMAT_DECIMAL_MODE_SCIENTIFIC_FULL,
    FORMAT_DECIMAL_FIXED_10
} FormatOperation;

static volatile unsigned char format_sink;

/* String-only reference for the deterministic nonzero fixture pattern. It
 * does not call the formatter or use floating point as a numerical oracle. */
static bool validate_fixture_output(const char *output, const char *fixture,
                                    size_t digits, FormatOperation operation)
{
    char expected[16432];
    char significant[16386];
    bool scientific = operation != FORMAT_BIGINT_FULL &&
        operation != FORMAT_DECIMAL_FIXED_10 &&
        (digits > 10U || operation == FORMAT_DECIMAL_MODE_SCIENTIFIC_10 ||
         operation == FORMAT_DECIMAL_MODE_SCIENTIFIC_FULL);
    size_t kept = digits;
    size_t exponent = digits - 1U;
    if (operation == FORMAT_DECIMAL_FIXED_10)
    {
        kept = digits < 10U ? digits : 10U;
        memcpy(significant, fixture + 2U, kept);
        if (digits > kept && (fixture[2U + kept] > '5' ||
            (fixture[2U + kept] == '5' && (digits > kept + 1U ||
             (significant[kept - 1U] - '0') % 2 != 0))))
        {
            size_t i = kept;
            while (i > 0U && significant[i - 1U] == '9') significant[--i] = '0';
            if (i == 0U) return false; /* this fixture pattern never carries to 1 */
            significant[i - 1U]++;
        }
        while (kept > 0U && significant[kept - 1U] == '0') kept--;
        significant[kept] = '\0';
        (void)snprintf(expected, sizeof(expected), "0.%s", significant);
    }
    else if (scientific)
    {
        size_t wanted = operation == FORMAT_DECIMAL_SCIENTIFIC_100 ? 101U : 11U;
        bool full = operation == FORMAT_DECIMAL_SCIENTIFIC_FULL ||
            operation == FORMAT_DECIMAL_AUTO_FULL || operation == FORMAT_DECIMAL_MODE_SCIENTIFIC_FULL;
        if (!full && digits > 10U && kept > wanted) kept = wanted;
        memcpy(significant, fixture, kept);
        if (digits > kept && (fixture[kept] > '5' ||
            (fixture[kept] == '5' && (digits > kept + 1U ||
             (significant[kept - 1U] - '0') % 2 != 0))))
        {
            size_t i = kept;
            while (i > 0U && significant[i - 1U] == '9') significant[--i] = '0';
            if (i == 0U) { significant[0] = '1'; exponent++; }
            else significant[i - 1U]++;
        }
        while (kept > 1U && significant[kept - 1U] == '0') kept--;
        significant[kept] = '\0';
        if (kept > 1U)
            (void)snprintf(expected, sizeof(expected), "%c.%sE+%zu", significant[0], significant + 1U, exponent);
        else (void)snprintf(expected, sizeof(expected), "%cE+%zu", significant[0], exponent);
    }
    else
    {
        (void)snprintf(expected, sizeof(expected), "%s", fixture);
    }
    return strcmp(output, expected) == 0;
}

static char *make_digits(size_t count, bool fractional)
{
    size_t prefix = fractional ? 2U : 0U;
    char *text = malloc(prefix + count + 1U);

    if (text == NULL)
    {
        return NULL;
    }

    if (fractional)
    {
        text[0] = '0';
        text[1] = '.';
    }

    for (size_t index = 0U; index < count; index++)
    {
        text[prefix + index] = (char)('1' + (index * 7U + 3U) % 9U);
    }

    text[prefix + count] = '\0';
    return text;
}

static bool invoke_operation(
    FormatOperation operation,
    const BigInt *integer,
    const BigDecimal *decimal,
    char **output
)
{
    switch (operation)
    {
        case FORMAT_BIGINT_FULL:
            *output = bigint_to_string(integer);
            return *output != NULL;
        case FORMAT_DECIMAL_SCIENTIFIC_10:
            return bigdecimal_format(
                decimal, 10, BIGDECIMAL_ROUND_HALF_EVEN, output) == BIGDECIMAL_OK;
        case FORMAT_DECIMAL_SCIENTIFIC_100:
            return bigdecimal_format(
                decimal, 100, BIGDECIMAL_ROUND_HALF_EVEN, output) == BIGDECIMAL_OK;
        case FORMAT_DECIMAL_SCIENTIFIC_FULL:
            return bigdecimal_format(
                decimal, -1, BIGDECIMAL_ROUND_HALF_EVEN, output) == BIGDECIMAL_OK;
        case FORMAT_DECIMAL_AUTO_10:
        case FORMAT_DECIMAL_AUTO_FULL:
            return bigdecimal_format_mode(decimal,
                operation == FORMAT_DECIMAL_AUTO_FULL ? -1 : 10,
                BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, 65536U, output) == BIGDECIMAL_OK;
        case FORMAT_DECIMAL_MODE_SCIENTIFIC_10:
        case FORMAT_DECIMAL_MODE_SCIENTIFIC_FULL:
            return bigdecimal_format_mode(decimal,
                operation == FORMAT_DECIMAL_MODE_SCIENTIFIC_FULL ? -1 : 10,
                BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_SCIENTIFIC, 65536U, output) == BIGDECIMAL_OK;
        case FORMAT_DECIMAL_FIXED_10:
            return bigdecimal_format(
                decimal, 10, BIGDECIMAL_ROUND_HALF_EVEN, output) == BIGDECIMAL_OK;
        default:
            return false;
    }
}

static double measure_batch(
    FormatOperation operation,
    const BigInt *integer,
    const BigDecimal *decimal,
    size_t iterations
)
{
    double start = benchmark_seconds();
    double end;

    if (start < 0.0)
    {
        return -1.0;
    }

    for (size_t index = 0U; index < iterations; index++)
    {
        char *output = NULL;

        if (!invoke_operation(operation, integer, decimal, &output))
        {
            return -1.0;
        }

        format_sink ^= (unsigned char)output[0];
        free(output);
    }

    end = benchmark_seconds();
    return end < start ? -1.0 : end - start;
}

static void sort_samples(double *values, size_t count)
{
    for (size_t index = 1U; index < count; index++)
    {
        double current = values[index];
        size_t position = index;

        while (position > 0U && values[position - 1U] > current)
        {
            values[position] = values[position - 1U];
            position--;
        }

        values[position] = current;
    }
}

static bool run_fixture(const char *name, size_t digits, FormatOperation operation, bool quick,
                        const char *literal, const char *expected)
{
    bool fractional = operation == FORMAT_DECIMAL_FIXED_10;
    char *fixture = literal == NULL ? make_digits(digits, fractional) : NULL;
    const char *source = literal == NULL ? fixture : literal;
    BigInt *integer = NULL;
    BigDecimal *decimal = NULL;
    double samples[FORMAT_BENCHMARK_MAX_SAMPLES];
    size_t sample_count = quick ? 3U : FORMAT_BENCHMARK_MAX_SAMPLES;
    size_t iterations = 1U;
    double target = quick ? 0.002 : 0.01;
    size_t allocation_calls;
    size_t allocation_bytes;
    size_t output_bytes;
    size_t peak_bytes = 0U;
    double phases[NUMFORGE_PHASE_COUNT] = {0};
    bool success = false;

    if (source == NULL)
    {
        goto cleanup;
    }

    if (operation == FORMAT_BIGINT_FULL)
    {
        integer = bigint_create();
        if (integer == NULL || bigint_set_string(integer, source) != BIGINT_OK)
        {
            goto cleanup;
        }
    }
    else
    {
        decimal = bigdecimal_create();
        if (decimal == NULL || bigdecimal_set_string(decimal, source) != BIGDECIMAL_OK)
        {
            goto cleanup;
        }
    }

    for (size_t warmup = 0U; warmup < 3U; warmup++)
    {
        if (measure_batch(operation, integer, decimal, 1U) < 0.0)
        {
            goto cleanup;
        }
    }

    for (;;)
    {
        double duration = measure_batch(operation, integer, decimal, iterations);

        if (duration < 0.0)
        {
            goto cleanup;
        }
        if (duration >= target || iterations >= FORMAT_BENCHMARK_MAX_ITERATIONS)
        {
            break;
        }
        iterations *= 2U;
    }

    for (size_t sample = 0U; sample < sample_count; sample++)
    {
        double duration = measure_batch(operation, integer, decimal, iterations);

        if (duration <= 0.0)
        {
            goto cleanup;
        }

        samples[sample] = duration * 1000000000.0 / (double)iterations;
    }

    {
        char *output = NULL;

        numforge_profile_reset();
        if (!invoke_operation(operation, integer, decimal, &output))
        {
            goto cleanup;
        }

        numforge_profile_stop();
        if (!numforge_profile_complete()) goto cleanup;
        for (size_t phase = 0; phase < NUMFORGE_PHASE_COUNT; phase++)
            phases[phase] = numforge_profile_seconds((NumForgeProfilePhase)phase);
        if ((expected != NULL && strcmp(output, expected) != 0) ||
            (literal == NULL && !validate_fixture_output(output, source, digits, operation)))
        {
            fprintf(stderr, "Incorrect output: %s -> %s\n", name, output);
            free(output);
            goto cleanup;
        }
        free(output);
        output = NULL;
        if (!numforge_alloc_stats_track(true)) goto cleanup;
        numforge_alloc_stats_reset();
        if (!invoke_operation(operation, integer, decimal, &output)) goto cleanup;
        peak_bytes = numforge_alloc_stats_peak();
        allocation_calls = numforge_alloc_stats_calls();
        allocation_bytes = numforge_alloc_stats_bytes();
        output_bytes = strlen(output);
        format_sink ^= (unsigned char)output[output_bytes - 1U];
        free(output);
        if (numforge_alloc_stats_live() != 0U || !numforge_alloc_stats_complete() ||
            !numforge_alloc_stats_track(false)) goto cleanup;
    }

    sort_samples(samples, sample_count);
    printf("%s,%zu,%zu,%zu,%zu,%.3f,%.3f,%.3f,%zu,%zu,%zu,%llu,%.3f,%.3f,%.3f,%.3f\n",
           name, digits, iterations, sample_count, output_bytes,
           samples[0], samples[sample_count / 2U], samples[sample_count - 1U],
           allocation_calls, allocation_bytes, peak_bytes, numforge_profile_process_peak(),
           phases[NUMFORGE_PHASE_CONVERT] * 1e9, phases[NUMFORGE_PHASE_ROUND] * 1e9,
           phases[NUMFORGE_PHASE_COMPOSE] * 1e9, phases[NUMFORGE_PHASE_OTHER] * 1e9);
    success = true;

cleanup:
    free(fixture);
    bigint_destroy(integer);
    bigdecimal_destroy(decimal);

    if (!success)
    {
        fprintf(stderr, "Benchmark failed: %s/%zu digits\n", name, digits);
    }

    return success;
}

static bool run_case(const char *name, size_t digits, FormatOperation operation, bool quick)
{
    return run_fixture(name, digits, operation, quick, NULL, NULL);
}

int main(int argc, char **argv)
{
    static const size_t full_sizes[] = {16U, 64U, 256U, 1024U, 4096U, 16384U};
    static const size_t quick_sizes[] = {16U, 256U, 1024U};
    const size_t *sizes;
    size_t size_count;
    bool quick = false;
    const char *selected = NULL;
    size_t selected_digits = 0U;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--quick") == 0) quick = true;
        else if (strcmp(argv[i], "--case") == 0 && i + 1 < argc) selected = argv[++i];
        else if (strcmp(argv[i], "--digits") == 0 && i + 1 < argc)
        {
            char *end;
            unsigned long parsed = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || parsed == 0 || parsed > 16384U) return EXIT_FAILURE;
            selected_digits = (size_t)parsed;
        }
        else { fputs("Usage: decimal_format_benchmark [--quick] [--case name] [--digits 1..16384]\n", stderr); return EXIT_FAILURE; }
    }
    bool matched = false;

    sizes = quick ? quick_sizes : full_sizes;
    size_count = quick ? sizeof(quick_sizes) / sizeof(quick_sizes[0])
                       : sizeof(full_sizes) / sizeof(full_sizes[0]);

#ifdef _MSC_VER
    fprintf(stderr, "compiler=MSVC-%d ", _MSC_VER);
#elif defined(__VERSION__)
    fprintf(stderr, "compiler=%s ", __VERSION__);
#endif
    fprintf(stderr, "pointer_bits=%zu alloc_stats=on mode=%s\n",
            sizeof(void *) * 8U, quick ? "quick" : "full");

    puts("operation,input_digits,iterations,samples,output_bytes,min_ns_per_op,"
         "median_ns_per_op,max_ns_per_op,alloc_calls_per_op,requested_bytes_per_op,peak_tracked_payload_bytes,process_peak_bytes,profile_convert_ns,"
         "profile_round_ns,profile_compose_ns,profile_other_ns");

    static const struct { const char *name; FormatOperation operation; } operations[] = {
        {"bigint_full", FORMAT_BIGINT_FULL},
        {"scientific_10", FORMAT_DECIMAL_SCIENTIFIC_10},
        {"scientific_100", FORMAT_DECIMAL_SCIENTIFIC_100},
        {"scientific_full", FORMAT_DECIMAL_SCIENTIFIC_FULL},
        {"fixed_10", FORMAT_DECIMAL_FIXED_10},
        {"auto_10", FORMAT_DECIMAL_AUTO_10},
        {"auto_full", FORMAT_DECIMAL_AUTO_FULL},
        {"mode_scientific_10", FORMAT_DECIMAL_MODE_SCIENTIFIC_10},
        {"mode_scientific_full", FORMAT_DECIMAL_MODE_SCIENTIFIC_FULL}
    };
    for (size_t index = 0; index < (selected_digits != 0U ? 1U : size_count); index++)
    {
        size_t digits = selected_digits != 0U ? selected_digits : sizes[index];
        for (size_t operation = 0; operation < sizeof(operations) / sizeof(operations[0]); operation++)
        {
            if (selected != NULL && strcmp(selected, operations[operation].name) != 0) continue;
            matched = true;
            if (!run_case(operations[operation].name, digits, operations[operation].operation, quick))
                return EXIT_FAILURE;
        }
    }

    static const struct { const char *label; const char *input; const char *expected; } edges[] = {
        {"zero", "0", "0"}, {"negative", "-1.25", "-1.25"},
        {"scale_positive", "1E100000", "1E+100000"},
        {"scale_negative", "1E-100000", "1E-100000"},
        {"carry", "9.99999999996E20", "1E+21"},
        {"tie_even", "1.23456789005E20", "1.23456789E+20"},
        {"tie_odd", "1.23456789015E20", "1.2345678902E+20"},
        {"sticky", "1.23456789005000000000001E20", "1.2345678901E+20"}
    };
    for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
    {
        if (selected != NULL && strcmp(selected, edges[i].label) != 0) continue;
        if (selected_digits != 0U) continue;
        matched = true;
        if (!run_fixture(edges[i].label, strlen(edges[i].input), FORMAT_DECIMAL_AUTO_10,
                         quick, edges[i].input, edges[i].expected)) return EXIT_FAILURE;
    }
    return matched ? EXIT_SUCCESS : EXIT_FAILURE;
}
