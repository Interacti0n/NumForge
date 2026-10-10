#include "calculator_internal.h"
#include "benchmark_clock.h"
#include "../src/internal/numforge_alloc.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One complete production calculator pipeline, with its shared deadline and
 * allocation/output budgets. Failures are diagnostic outcomes, not timings
 * of a successful calculation. No live-allocation ledger distorts the timer. */
static bool integer_argument(const char *text, int64_t *value)
{
    char *end;
    errno = 0;
    long long parsed = strtoll(text, &end, 10);
    if (errno || !*text || *end) return false;
    *value = (int64_t)parsed;
    return true;
}

int main(int argc, char **argv)
{
    CalculatorContext context;
    CalculatorError error;
    int64_t places, notation, deadline;
    char *result = NULL;
    char input[CALCULATOR_MAX_INPUT_BYTES + 2U];
    bool from_file = argc == 6 && !strcmp(argv[1], "--input-file");
    int settings = from_file ? 3 : 2;
    if ((!from_file && argc != 5) || !integer_argument(argv[settings], &places) ||
        !integer_argument(argv[settings + 1], &notation) || !integer_argument(argv[settings + 2], &deadline) ||
        notation < CALCULATOR_NOTATION_AUTO || notation > CALCULATOR_NOTATION_FRACTION || deadline < 0)
    {
        fputs("Usage: calculator_limit_benchmark expression places notation time-limit-ms\n"
              "   or: calculator_limit_benchmark --input-file UTF8-file places notation time-limit-ms\n", stderr);
        return EXIT_FAILURE;
    }
    const char *expression = argv[1];
    if (from_file)
    {
        FILE *file = fopen(argv[2], "rb");
        if (file == NULL) return EXIT_FAILURE;
        size_t length = fread(input, 1U, sizeof(input) - 1U, file);
        bool failed = ferror(file) != 0;
        fclose(file);
        if (failed) return EXIT_FAILURE;
        input[length] = '\0';
        expression = input;
    }
    calculator_context_init(&context);
    if (calculator_context_set_output_scale(&context, places) != CALCULATOR_OK) return EXIT_FAILURE;
    context.notation = (CalculatorNotation)notation;
    context.time_limit_ms = deadline;
    numforge_alloc_stats_reset();
    double begin = benchmark_seconds();
    CalculatorStatus status = calculator_compute(expression, &context, &result, &error);
    double end = benchmark_seconds();
    if (begin < 0.0 || end < begin || (status == CALCULATOR_OK && !result) ||
        (status != CALCULATOR_OK && result != NULL)) { free(result); return EXIT_FAILURE; }
    printf("status,status_code,elapsed_ms,output_bytes,alloc_calls,requested_bytes\n");
    printf("%s,%d,%.6f,%zu,%" PRIu64 ",%" PRIu64 "\n",
        calculator_status_to_string(status), (int)status, (end - begin) * 1000.0,
        result ? strlen(result) : 0U, numforge_alloc_stats_calls(), numforge_alloc_stats_bytes());
    /* Optional independently known result for smoke tests; never dump huge or
     * user-provided expressions/results into CSV. */
    const char *expected = getenv("NUMFORGE_BENCH_EXPECTED");
    bool matches = expected == NULL || (status == CALCULATOR_OK && !strcmp(result, expected));
    free(result);
    return matches ? EXIT_SUCCESS : EXIT_FAILURE;
}
