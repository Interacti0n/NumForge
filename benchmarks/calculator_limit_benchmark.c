#include "calculator_internal.h"
#include "benchmark_clock.h"
#include "../src/internal/numforge_alloc.h"
#include "../src/internal/decimal_profile.h"
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

/* File expectations avoid environment-size limits for exact large fractions.
 * Check bytes after timing; never serialize reference contents into metrics. */
static bool matches_expected_file(const char *result, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    size_t position = 0U;
    int byte;
    bool matches = true;
    while ((byte = fgetc(file)) != EOF)
    {
        if (result[position] == '\0' || (unsigned char)result[position] != (unsigned char)byte)
        { matches = false; break; }
        position++;
    }
    matches = matches && !ferror(file) && result[position] == '\0';
    fclose(file);
    return matches;
}

int main(int argc, char **argv)
{
    CalculatorContext context;
    CalculatorError error;
    int64_t places, notation, deadline;
    char *result = NULL;
    char input[CALCULATOR_MAX_INPUT_BYTES + 2U];
    bool from_file = argc == 6 && !strcmp(argv[1], "--input-file");
    bool profile = argc == 6 && !strcmp(argv[5], "--phases-profile") && !from_file;
    bool phases = argc == 6 && (!strcmp(argv[5], "--phases") || profile) && !from_file;
    int settings = from_file ? 3 : 2;
    if ((!from_file && !phases && argc != 5) || !integer_argument(argv[settings], &places) ||
        !integer_argument(argv[settings + 1], &notation) || !integer_argument(argv[settings + 2], &deadline) ||
        notation < CALCULATOR_NOTATION_AUTO || notation > CALCULATOR_NOTATION_FRACTION || deadline < 0)
    {
        fputs("Usage: calculator_limit_benchmark expression places notation time-limit-ms [--phases|--phases-profile]\n"
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
    if (profile) numforge_decimal_profile_reset();
    double begin = benchmark_seconds();
    /* Own the same production scope so its reason remains observable after
     * calculator_compute returns. Nested compute/format stages reuse it. */
    if (!numforge_budget_begin((uint64_t)deadline,
            CALCULATOR_ALLOCATION_BUDGET, CALCULATOR_SINGLE_ALLOCATION)) return EXIT_FAILURE;
    double evaluated = begin, formatted = begin;
    CalculatorStatus status;
    if (phases)
    {
        /* Mirror compute's value/format pipeline inside one shared scope.
         * Evaluation includes parsing and value projection, not only BigInt
         * factorial. Destruction is included in total, outside both phases. */
        CalculatorValue value = {0};
        status = calculator_compute_value(expression, &context, &value, &error);
        evaluated = benchmark_seconds();
        if (status == CALCULATOR_OK)
        {
            status = calculator_format_value(&value, &context, &result);
            status = calculator_budget_status(status);
            if (status != CALCULATOR_OK) { free(result); result = NULL; }
        }
        formatted = benchmark_seconds();
        calculator_value_destroy(&value);
    }
    else status = calculator_compute(expression, &context, &result, &error);
    double end = benchmark_seconds();
    if (profile) numforge_decimal_profile_stop();
    NumForgeBudgetFailure budget_failure = numforge_budget_failure();
    numforge_budget_end();
    if (begin < 0.0 || end < begin ||
        (phases && (evaluated < begin || formatted < evaluated || end < formatted)) ||
        (profile && !numforge_decimal_profile_complete()) ||
        (status == CALCULATOR_OK && !result) ||
        (status != CALCULATOR_OK && result != NULL)) { free(result); return EXIT_FAILURE; }
    printf("status,status_code,elapsed_ms,output_bytes,alloc_calls,requested_bytes,budget_reason%s%s\n",
        phases ? ",evaluate_ms,format_ms" : "",
        profile ? ",normalize_ms,decimal_core_ms,profile_complete" : "");
    printf("%s,%d,%.6f,%zu,%zu,%zu,%s",
        calculator_status_to_string(status), (int)status, (end - begin) * 1000.0,
        result ? strlen(result) : 0U, numforge_alloc_stats_calls(), numforge_alloc_stats_bytes(),
        budget_failure == NUMFORGE_BUDGET_TIME ? "time" :
        budget_failure == NUMFORGE_BUDGET_MEMORY ? "allocation" : "none");
    if (phases) printf(",%.6f,%.6f", (evaluated - begin) * 1000.0,
        (formatted - evaluated) * 1000.0);
    if (profile) printf(",%.6f,%.6f,%d",
        numforge_decimal_profile_seconds(NUMFORGE_DECIMAL_NORMALIZE) * 1000.0,
        numforge_decimal_profile_seconds(NUMFORGE_DECIMAL_CORE) * 1000.0,
        numforge_decimal_profile_complete());
    putchar('\n');
    /* Optional independently known result for smoke tests; never dump huge or
     * user-provided expressions/results into CSV. */
    const char *expected = getenv("NUMFORGE_BENCH_EXPECTED");
    bool matches = expected == NULL || (status == CALCULATOR_OK && !strcmp(result, expected));
    const char *expected_file = getenv("NUMFORGE_BENCH_EXPECTED_FILE");
    if (expected_file != NULL)
        matches = matches && status == CALCULATOR_OK && matches_expected_file(result, expected_file);
    free(result);
    return matches ? EXIT_SUCCESS : EXIT_FAILURE;
}
