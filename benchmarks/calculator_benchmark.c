#include "evaluator.h"
#include "formatter.h"
#include "benchmark_clock.h"
#include "../src/internal/numforge_alloc.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Calculator phase benchmark.

    Phase totals exclude object destruction and result creation. Allocation
    counts measure requests, including realloc's full size, not peak live
    bytes. These timings are diagnostic data and are not CI pass/fail limits.
------------------------------------------------------------------------------------------------------------------------------
*/

/*
------------------------------------------------------------------------------------------------------------------------------
    Execute one benchmark scenario and print a CSV result row.
------------------------------------------------------------------------------------------------------------------------------
*/
static int run_case(
    const char *label,
    const char *input,
    unsigned iterations,
    const char *expected
)
{
    CalculatorContext context;
    double elapsed[3] = { 0.0, 0.0, 0.0 };
    uint64_t calls[3] = { 0, 0, 0 };
    uint64_t bytes[3] = { 0, 0, 0 };

    calculator_context_init(&context);
    /* Large diagnostics measure the phases to completion rather than making
     * hardware-dependent five-second timings a benchmark failure. The memory
     * and output budgets remain active. Production defaults are unchanged. */
    if (expected != NULL) context.time_limit_ms = INT64_MAX;

    for (unsigned iteration = 0; iteration < iterations; iteration++)
    {
        CalculatorExpression *expression = NULL;
        CalculatorError error;
        BigDecimal *value = bigdecimal_create();
        char *text = NULL;
        double times[4];

        if (value == NULL)
        {
            return EXIT_FAILURE;
        }

        numforge_alloc_stats_reset();
        times[0] = benchmark_seconds();

        CalculatorStatus status = calculator_parse(
            input,
            &expression,
            &error
        );

        times[1] = benchmark_seconds();
        calls[0] += numforge_alloc_stats_calls();
        bytes[0] += numforge_alloc_stats_bytes();

        numforge_alloc_stats_reset();

        if (status == CALCULATOR_OK)
        {
            status = calculator_evaluate(
                value,
                expression,
                &context,
                &error
            );
        }

        times[2] = benchmark_seconds();
        calls[1] += numforge_alloc_stats_calls();
        bytes[1] += numforge_alloc_stats_bytes();

        numforge_alloc_stats_reset();

        if (status == CALCULATOR_OK)
        {
            status = calculator_format_result(
                value,
                &context,
                &text
            );
        }

        times[3] = benchmark_seconds();
        calls[2] += numforge_alloc_stats_calls();
        bytes[2] += numforge_alloc_stats_bytes();

        if (status == CALCULATOR_OK && expected != NULL && strcmp(text, expected) != 0)
        {
            fprintf(stderr, "Unexpected formatted factorial: %s => %s\n", input, text);
            status = CALCULATOR_INVALID_ARGUMENT;
        }
        free(text);
        bigdecimal_destroy(value);
        calculator_expression_destroy(expression);

        if (status != CALCULATOR_OK)
        {
            fprintf(
                stderr,
                "Benchmark failed: %s (%s)\n",
                label,
                calculator_status_to_string(status)
            );

            return EXIT_FAILURE;
        }

        for (size_t phase = 0; phase < 3; phase++)
        {
            if (times[phase] < 0.0 || times[phase + 1] < times[phase])
            {
                return EXIT_FAILURE;
            }

            elapsed[phase] +=
                (double)(times[phase + 1] - times[phase]);
        }
    }

    printf("%s,%u", label, iterations);

    for (size_t phase = 0; phase < 3; phase++)
    {
        printf(
            ",%.3f,%" PRIu64 ",%" PRIu64,
            elapsed[phase] * 1000.0,
            calls[phase],
            bytes[phase]
        );
    }

    putchar('\n');

    return EXIT_SUCCESS;
}

/*
------------------------------------------------------------------------------------------------------------------------------
    Benchmark entry point.
------------------------------------------------------------------------------------------------------------------------------
*/
int main(int argc, char **argv)
{
    bool factorial_only = argc == 2 && strcmp(argv[1], "--factorial-only") == 0;
    if (argc != 1 && !factorial_only)
    {
        fputs("Usage: calculator_benchmark [--factorial-only]\n", stderr);
        return EXIT_FAILURE;
    }
    static const char *const inputs[] = {
        "1E-40/3",
        "2^1024",
        "250!",
        "π*e+φ"
    };

    static const size_t sizes[] = {
        16,
        64,
        256,
        1024
    };

    puts(
        "case,iterations,parse_ms,parse_calls,parse_bytes,"
        "evaluate_ms,evaluate_calls,evaluate_bytes,"
        "format_ms,format_calls,format_bytes"
    );

    if (factorial_only)
    {
        /* Literal scientific references from the independent Node BigInt
         * fixtures, rounded to the default ten decimal places. One iteration
         * keeps large diagnostic runs practical; repeat processes for ranges.
         * Large diagnostics disable the phase time limits explicitly. */
        const char *cases[][2] = {
            {"10000!", "2.8462596809E+35659"},
            {"20000!", "1.8192063202E+77337"},
            {"50000!", "3.3473205096E+213236"},
            {"100000!", "2.824229408E+456573"}
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
            if (run_case(cases[i][0], cases[i][0], 1, cases[i][1]) != EXIT_SUCCESS)
                return EXIT_FAILURE;
        return EXIT_SUCCESS;
    }

    for (size_t sample = 0;
         sample < sizeof(inputs) / sizeof(inputs[0]);
         sample++)
    {
        if (run_case(inputs[sample], inputs[sample], 1000, NULL) != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
    }

    for (size_t sample = 0;
         sample < sizeof(sizes) / sizeof(sizes[0]);
         sample++)
    {
        size_t digits = sizes[sample];
        char input[2050];
        char label[32];

        memset(input, '8', digits);
        input[digits] = '*';
        memset(input + digits + 1, '3', digits);
        input[2 * digits + 1] = '\0';

        (void)snprintf(
            label,
            sizeof(label),
            "multiply_%zu_digits",
            digits
        );

        if (run_case(label, input, 100, NULL) != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
