#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include "src/core/bigdecimal/bigdecimal_internal.h"
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/* Factorial construction and exact round-trip validation are outside timing.
 * Measures the existing public integer-to-decimal bridge without calculator
 * budgets; this is not an end-to-end request or an independent factorial oracle. */
int main(int argc, char **argv)
{
    BigInt *n = bigint_create(), *source = bigint_create(), *restored = bigint_create();
    BigDecimal *destination = bigdecimal_create(), *direct = NULL;
    int exit_code = EXIT_FAILURE;
    if (argc != 2 || !n || !source || !restored || !destination ||
        bigint_set_string(n, argv[1]) != BIGINT_OK || bigint_factorial(source, n) != BIGINT_OK)
        goto cleanup;
    puts("sample,materialize_ms,alloc_calls,requested_bytes,direct_copy_normalize_ms,direct_alloc_calls,direct_requested_bytes");
    for (unsigned sample = 1; sample <= 3; sample++)
    {
        numforge_alloc_stats_reset();
        double begin = benchmark_seconds();
        BigDecimalStatus status = bigdecimal_from_bigint(destination, source);
        double end = benchmark_seconds();
        uint64_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
        if (status != BIGDECIMAL_OK || begin < 0.0 || end < begin ||
            bigdecimal_to_bigint(restored, destination) != BIGDECIMAL_OK ||
            bigint_compare(source, restored) != 0)
            goto cleanup;
        /* Private experiment, not a replacement public implementation: prepare
         * a fresh object outside timing, copy limbs and normalize. The atomic
         * public commit/ownership path and failure-injection work remain future
         * work. Validate the exact integer round trip separately. */
        bigdecimal_destroy(direct);
        direct = bigdecimal_create();
        if (direct == NULL) goto cleanup;
        numforge_alloc_stats_reset();
        double direct_begin = benchmark_seconds();
        BigIntStatus copied = bigint_copy(direct->coefficient, source);
        direct->scale = 0;
        BigDecimalStatus normalized = copied == BIGINT_OK ? bigdecimal_normalize(direct) : BIGDECIMAL_OUT_OF_MEMORY;
        double direct_end = benchmark_seconds();
        uint64_t direct_calls = numforge_alloc_stats_calls(), direct_bytes = numforge_alloc_stats_bytes();
        if (normalized != BIGDECIMAL_OK || direct_begin < 0.0 || direct_end < direct_begin ||
            bigdecimal_to_bigint(restored, direct) != BIGDECIMAL_OK || bigint_compare(source, restored) != 0)
            goto cleanup;
        printf("%u,%.6f,%" PRIu64 ",%" PRIu64 ",%.6f,%" PRIu64 ",%" PRIu64 "\n",
            sample, (end - begin) * 1000.0, calls, bytes, (direct_end - direct_begin) * 1000.0,
            direct_calls, direct_bytes);
    }
    exit_code = EXIT_SUCCESS;
cleanup:
    bigint_destroy(n); bigint_destroy(source); bigint_destroy(restored);
    bigdecimal_destroy(destination); bigdecimal_destroy(direct);
    return exit_code;
}
