#include "benchmark_clock.h"
#include "../src/bigint/bigint_internal.h"
#include "../src/internal/numforge_alloc.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Direct BigInt multiplication benchmark. Private representation access is
    limited to exact-size fixtures and untimed result checks, not arithmetic.
    Each batch reuses the destination object; allocation/free inside bigint_mul
    remains measured. Parsing, fixture construction, checks and CSV are excluded.

    Fixed data, warm-up, calibrated batches, independent samples and median.
    Checks are sanity checks, not a substitute for the correctness test suite.
------------------------------------------------------------------------------------------------------------------------------
*/

#define BENCHMARK_MAX_SAMPLES 7U
#define BENCHMARK_MAX_ITERATIONS 262144U

static volatile uint64_t benchmark_sink;

static uint64_t next_random(uint64_t *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 7;
    *state ^= *state << 17;

    return *state;
}

static BigInt *make_operand(size_t limbs, unsigned pattern, uint64_t seed)
{
    BigInt *value = bigint_create();

    if (value == NULL)
    {
        return NULL;
    }

    value->limbs = malloc(limbs * sizeof(*value->limbs));

    if (value->limbs == NULL)
    {
        bigint_destroy(value);
        return NULL;
    }

    value->size = value->capacity = limbs;

    for (size_t index = 0U; index < limbs; index++)
    {
        switch (pattern)
        {
            case 1U:
                value->limbs[index] = UINT64_MAX;
                break;
            case 2U:
                value->limbs[index] = index == 0U ? 1U : 0U;
                break;
            case 3U:
                value->limbs[index] = index % 2U == 0U
                    ? UINT64_C(0xaaaaaaaaaaaaaaaa) : UINT64_C(0x5555555555555555);
                break;
            default:
                value->limbs[index] = next_random(&seed);
                break;
        }
    }

    /* Always use all declared bits, including sparse operands. */
    value->limbs[limbs - 1U] |= UINT64_C(1) << 63;
    return value;
}

static uint64_t residue(const BigInt *value, uint64_t modulus)
{
    uint64_t result = 0U;
    uint64_t base = (UINT64_MAX % modulus + 1U) % modulus;

    for (size_t index = value->size; index > 0U; index--)
    {
        result = (result * base + value->limbs[index - 1U] % modulus) % modulus;
    }

    return result;
}

static bool check_product(const BigInt *a, const BigInt *b, const BigInt *product)
{
    static const uint64_t moduli[] = {97U, 65521U};

    if (product->size == 0U || product->size > a->size + b->size ||
        product->limbs[product->size - 1U] == 0U || product->is_negative)
    {
        return false;
    }

    for (size_t index = 0U; index < sizeof(moduli) / sizeof(moduli[0]); index++)
    {
        uint64_t modulus = moduli[index];

        if (residue(product, modulus) != residue(a, modulus) * residue(b, modulus) % modulus)
        {
            return false;
        }
    }

    benchmark_sink ^= product->limbs[0] ^ product->limbs[product->size - 1U];
    return true;
}

static double measure_batch(BigInt *product, const BigInt *a, const BigInt *b, size_t iterations)
{
    double start;
    double end;

    numforge_alloc_stats_reset();
    start = benchmark_seconds();

    if (start < 0.0)
    {
        return -1.0;
    }

    for (size_t index = 0U; index < iterations; index++)
    {
        if (bigint_mul(product, a, b) != BIGINT_OK)
        {
            return -1.0;
        }
    }

    end = benchmark_seconds();

    if (end < start || !check_product(a, b, product))
    {
        return -1.0;
    }

    return end - start;
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

/*
------------------------------------------------------------------------------------------------------------------------------
    Run one shape/pattern. Squaring passes the same operand twice, with a
    separate output: no input restore or growing chained products in timing.
------------------------------------------------------------------------------------------------------------------------------
*/
static bool run_case(const char *shape, size_t a_size, size_t b_size,
                     unsigned pattern, bool square, bool quick)
{
    static const char *const patterns[] = {"random", "max", "sparse", "alternating"};
    BigInt *a = make_operand(a_size, pattern, UINT64_C(0x123456789abcdef));
    BigInt *b = square ? a : make_operand(b_size, pattern, UINT64_C(0xfedcba987654321));
    BigInt *product = bigint_create();
    double samples[BENCHMARK_MAX_SAMPLES];
    size_t sample_count = quick ? 3U : BENCHMARK_MAX_SAMPLES;
    size_t iterations = 1U;
    double target = quick ? 0.002 : 0.01;
    bool success = false;
    size_t allocation_calls;
    size_t allocation_bytes;

    if (a == NULL || b == NULL || product == NULL)
    {
        goto cleanup;
    }

    if (strcmp(shape, "factorial_factor") == 0)
    {
        b->limbs[0] = 10000U;
    }

    for (size_t warmup = 0U; warmup < 3U; warmup++)
    {
        if (measure_batch(product, a, b, 1U) < 0.0)
        {
            goto cleanup;
        }
    }

    for (;;)
    {
        double duration = measure_batch(product, a, b, iterations);

        if (duration < 0.0)
        {
            goto cleanup;
        }

        if (duration >= target || iterations >= BENCHMARK_MAX_ITERATIONS)
        {
            break;
        }

        iterations *= 2U;
    }

    for (size_t sample = 0U; sample < sample_count; sample++)
    {
        double duration = measure_batch(product, a, b, iterations);

        if (duration <= 0.0)
        {
            goto cleanup;
        }

        samples[sample] = duration * 1000000000.0 / (double)iterations;
    }

    /* Count one steady-state call separately; avoid overflowing cumulative
     * counters on 32-bit builds during large batches. */
    numforge_alloc_stats_reset();

    if (bigint_mul(product, a, b) != BIGINT_OK || !check_product(a, b, product))
    {
        goto cleanup;
    }

    allocation_calls = numforge_alloc_stats_calls();
    allocation_bytes = numforge_alloc_stats_bytes();
    sort_samples(samples, sample_count);

    printf("%s,%s,%zu,%zu,%zu,%zu,%zu,%zu,%.3f,%.3f,%.3f,%zu,%zu\n",
           shape, patterns[pattern], a_size, b_size, bigint_bit_length(a), bigint_bit_length(b),
           iterations, sample_count, samples[0], samples[sample_count / 2U],
           samples[sample_count - 1U], allocation_calls, allocation_bytes);
    success = true;

cleanup:
    if (!square)
    {
        bigint_destroy(b);
    }

    bigint_destroy(a);
    bigint_destroy(product);

    if (!success)
    {
        fprintf(stderr, "Benchmark failed: %s/%s (%zu x %zu limbs)\n",
                shape, patterns[pattern], a_size, b_size);
    }

    return success;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Default: 1..1024 limbs (64..65536 bits). --quick is a short harness check,
    not sufficient for selecting an algorithm crossover or claiming a speedup.
------------------------------------------------------------------------------------------------------------------------------
*/
int main(int argc, char **argv)
{
    bool quick = argc == 2 && strcmp(argv[1], "--quick") == 0;
    size_t maximum = quick ? 64U : 1024U;

    if (argc > 2 || (argc == 2 && !quick))
    {
        fputs("Usage: bigint_multiply_benchmark [--quick]\n", stderr);
        return EXIT_FAILURE;
    }

#ifdef _MSC_VER
    fprintf(stderr, "compiler=MSVC-%d ", _MSC_VER);
#elif defined(__VERSION__)
    fprintf(stderr, "compiler=%s ", __VERSION__);
#endif
    fprintf(stderr, "pointer_bits=%zu limb_bits=64 alloc_stats=on mode=%s\n",
            sizeof(void *) * 8U, quick ? "quick" : "full");

    puts("shape,pattern,a_limbs,b_limbs,a_bits,b_bits,iterations,samples,"
         "min_ns_per_op,median_ns_per_op,max_ns_per_op,alloc_calls_per_op,requested_bytes_per_op");

    for (size_t size = 1U; size <= maximum; size *= quick ? 8U : 2U)
    {
        for (unsigned pattern = 0U; pattern < 4U; pattern++)
        {
            if (!run_case("balanced", size, size, pattern, false, quick) ||
                !run_case("square", size, size, pattern, true, quick))
            {
                return EXIT_FAILURE;
            }
        }

        if (size > 1U)
        {
            size_t small = size / 8U > 1U ? size / 8U : 1U;

            if (!run_case("large_small", size, 1U, 0U, false, quick) ||
                !run_case("small_large", 1U, size, 0U, false, quick) ||
                !run_case("factorial_factor", size, 1U, 0U, false, quick) ||
                (small > 1U &&
                 (!run_case("ratio_8_1", size, small, 0U, false, quick) ||
                  !run_case("ratio_1_8", small, size, 0U, false, quick))))
            {
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}
