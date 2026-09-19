#include "benchmark_clock.h"
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
    FORMAT_DECIMAL_FIXED_10
} FormatOperation;

static volatile unsigned char format_sink;

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

static bool run_case(const char *name, size_t digits, FormatOperation operation, bool quick)
{
    bool fractional = operation == FORMAT_DECIMAL_FIXED_10;
    char *fixture = make_digits(digits, fractional);
    BigInt *integer = NULL;
    BigDecimal *decimal = NULL;
    double samples[FORMAT_BENCHMARK_MAX_SAMPLES];
    size_t sample_count = quick ? 3U : FORMAT_BENCHMARK_MAX_SAMPLES;
    size_t iterations = 1U;
    double target = quick ? 0.002 : 0.01;
    size_t allocation_calls;
    size_t allocation_bytes;
    size_t output_bytes;
    bool success = false;

    if (fixture == NULL)
    {
        goto cleanup;
    }

    if (operation == FORMAT_BIGINT_FULL)
    {
        integer = bigint_create();
        if (integer == NULL || bigint_set_string(integer, fixture) != BIGINT_OK)
        {
            goto cleanup;
        }
    }
    else
    {
        decimal = bigdecimal_create();
        if (decimal == NULL || bigdecimal_set_string(decimal, fixture) != BIGDECIMAL_OK)
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

        numforge_alloc_stats_reset();
        if (!invoke_operation(operation, integer, decimal, &output))
        {
            goto cleanup;
        }

        allocation_calls = numforge_alloc_stats_calls();
        allocation_bytes = numforge_alloc_stats_bytes();
        output_bytes = strlen(output);
        format_sink ^= (unsigned char)output[output_bytes - 1U];
        free(output);
    }

    sort_samples(samples, sample_count);
    printf("%s,%zu,%zu,%zu,%zu,%.3f,%.3f,%.3f,%zu,%zu\n",
           name, digits, iterations, sample_count, output_bytes,
           samples[0], samples[sample_count / 2U], samples[sample_count - 1U],
           allocation_calls, allocation_bytes);
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

int main(int argc, char **argv)
{
    static const size_t full_sizes[] = {16U, 64U, 256U, 1024U, 4096U, 16384U};
    static const size_t quick_sizes[] = {16U, 256U, 1024U};
    const size_t *sizes;
    size_t size_count;
    bool quick = argc == 2 && strcmp(argv[1], "--quick") == 0;

    if (argc > 2 || (argc == 2 && !quick))
    {
        fputs("Usage: decimal_format_benchmark [--quick]\n", stderr);
        return EXIT_FAILURE;
    }

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
         "median_ns_per_op,max_ns_per_op,alloc_calls_per_op,requested_bytes_per_op");

    for (size_t index = 0U; index < size_count; index++)
    {
        size_t digits = sizes[index];

        if (!run_case("bigint_full", digits, FORMAT_BIGINT_FULL, quick) ||
            !run_case("scientific_10", digits, FORMAT_DECIMAL_SCIENTIFIC_10, quick) ||
            !run_case("scientific_100", digits, FORMAT_DECIMAL_SCIENTIFIC_100, quick) ||
            !run_case("scientific_full", digits, FORMAT_DECIMAL_SCIENTIFIC_FULL, quick) ||
            !run_case("fixed_10", digits, FORMAT_DECIMAL_FIXED_10, quick))
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
