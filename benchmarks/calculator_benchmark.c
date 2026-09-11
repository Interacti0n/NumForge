#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "evaluator.h"
#include "formatter.h"
#include "../src/internal/numforge_alloc.h"

/* Phase totals exclude object destruction and result creation. Allocation
 * counts measure requests, including realloc's full size, not peak live bytes.
 * Never use these timings as absolute pass/fail thresholds in ordinary CI. */
static int run_case(const char *label, const char *input, unsigned iterations)
{
    CalculatorContext context;
    double elapsed[3] = { 0, 0, 0 };
    uint64_t calls[3] = { 0, 0, 0 }, bytes[3] = { 0, 0, 0 };
    calculator_context_init(&context);
    for (unsigned iteration = 0; iteration < iterations; iteration++)
    {
        CalculatorExpression *expression = NULL;
        CalculatorError error;
        BigDecimal *value = bigdecimal_create();
        char *text = NULL;
        clock_t times[4];
        if (value == NULL) return EXIT_FAILURE;
        numforge_alloc_stats_reset();
        times[0] = clock();
        CalculatorStatus status = calculator_parse(input, &expression, &error);
        times[1] = clock();
        calls[0] += numforge_alloc_stats_calls();
        bytes[0] += numforge_alloc_stats_bytes();
        numforge_alloc_stats_reset();
        if (status == CALCULATOR_OK)
            status = calculator_evaluate(value, expression, &context, &error);
        times[2] = clock();
        calls[1] += numforge_alloc_stats_calls();
        bytes[1] += numforge_alloc_stats_bytes();
        numforge_alloc_stats_reset();
        if (status == CALCULATOR_OK)
            status = calculator_format_result(value, &context, &text);
        times[3] = clock();
        calls[2] += numforge_alloc_stats_calls();
        bytes[2] += numforge_alloc_stats_bytes();
        free(text);
        bigdecimal_destroy(value);
        calculator_expression_destroy(expression);
        if (status != CALCULATOR_OK)
        {
            fprintf(stderr, "Benchmark failed: %s (%s)\n", label, calculator_status_to_string(status));
            return EXIT_FAILURE;
        }
        for (size_t phase = 0; phase < 3; phase++)
        {
            if (times[phase] == (clock_t)-1 || times[phase + 1] == (clock_t)-1) return EXIT_FAILURE;
            elapsed[phase] += (double)(times[phase + 1] - times[phase]);
        }
    }
    printf("%s,%u", label, iterations);
    for (size_t phase = 0; phase < 3; phase++)
        printf(",%.3f,%" PRIu64 ",%" PRIu64, elapsed[phase] * 1000 / CLOCKS_PER_SEC, calls[phase], bytes[phase]);
    putchar('\n');
    return EXIT_SUCCESS;
}

int main(void)
{
    static const char *const inputs[] = { "1E-40/3", "2^1024", "250!", "π*e+φ" };
    static const size_t sizes[] = { 16, 64, 256, 1024 };
    puts("case,iterations,parse_ms,parse_calls,parse_bytes,evaluate_ms,evaluate_calls,evaluate_bytes,format_ms,format_calls,format_bytes");
    for (size_t sample = 0; sample < sizeof(inputs) / sizeof(inputs[0]); sample++)
        if (run_case(inputs[sample], inputs[sample], 1000) != EXIT_SUCCESS) return EXIT_FAILURE;
    for (size_t sample = 0; sample < sizeof(sizes) / sizeof(sizes[0]); sample++)
    {
        size_t digits = sizes[sample];
        char input[2050], label[32];
        memset(input, '8', digits);
        input[digits] = '*';
        memset(input + digits + 1, '3', digits);
        input[2 * digits + 1] = '\0';
        (void)snprintf(label, sizeof(label), "multiply_%zu_digits", digits);
        if (run_case(label, input, 100) != EXIT_SUCCESS) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
