#include "numeric_root.h"
#include "numeric_root_cases.h"
#include "benchmark_clock.h"
#include "../src/internal/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool bound(BigRational *target, const char *text)
{
    BigDecimal *value = bigdecimal_create();
    bool ok = value && bigdecimal_set_string(value, text) == BIGDECIMAL_OK &&
        bigrational_from_bigdecimal(target, value) == BIGRATIONAL_OK;
    bigdecimal_destroy(value); return ok;
}
static bool verify(const ApplicationNumericRoot *root, const NumericRootCase *scene)
{
    if (!root || root->kind != scene->outcome || root->evaluations > 192U || root->iterations > 96U) return false;
    if (!scene->reference) return !root->x && !root->residual;
    BigDecimal *actual = bigdecimal_create(), *expected = bigdecimal_create(), *tolerance = bigdecimal_create();
    int comparison = 1;
    bool ok = actual && expected && tolerance && root->x && root->residual &&
        bigrational_to_bigdecimal(actual, root->x, 140, BIGDECIMAL_ROUND_HALF_EVEN) == BIGRATIONAL_OK &&
        bigdecimal_set_string(expected, scene->reference) == BIGDECIMAL_OK &&
        bigdecimal_sub(actual, actual, expected) == BIGDECIMAL_OK &&
        bigdecimal_abs(actual, actual) == BIGDECIMAL_OK &&
        bigdecimal_set_string(tolerance, "1E-12") == BIGDECIMAL_OK &&
        bigdecimal_compare(&comparison, actual, tolerance) == BIGDECIMAL_OK && comparison <= 0 &&
        bigdecimal_compare(&comparison, root->residual, tolerance) == BIGDECIMAL_OK && comparison <= 0;
    bigdecimal_destroy(actual); bigdecimal_destroy(expected); bigdecimal_destroy(tolerance); return ok;
}
int main(int argc, char **argv)
{
    bool quick = argc == 2 && !strcmp(argv[1], "--quick");
    bool adversarial = argc == 2 && !strcmp(argv[1], "--adversarial");
    if (argc > 2 || (argc == 2 && !quick && !adversarial)) return 2;
    const int precisions[] = {16, 34, 100}; bool failed = false;
    puts("case,precision,repeat,ms,status,outcome,evaluations,iterations,allocation_calls,allocation_bytes,verified,x,residual");
    NumericRootCase gap = {"vanishing_jump", "x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0", "0", "1", NULL, APPLICATION_ROOT_ITERATION_LIMIT};
    size_t count = adversarial ? 1U : sizeof(numeric_root_cases)/sizeof(numeric_root_cases[0]);
    for (size_t p = quick ? 1U : 0U; p < (quick ? 2U : 3U); ++p)
        for (size_t i = 0U; i < count; ++i) for (int run = 1; run <= (quick || adversarial ? 1 : 3); ++run) {
            const NumericRootCase *scene = adversarial ? &gap : &numeric_root_cases[i];
            CalculatorContext context; calculator_context_init(&context); context.division_scale = precisions[p];
            CalculatorError error; ApplicationNumericRoot *root = NULL;
            BigRational *lower = bigrational_create(), *upper = bigrational_create();
            if (!lower || !upper || !bound(lower, scene->lower) || !bound(upper, scene->upper)) return 2;
            numforge_alloc_stats_reset(); double start = benchmark_seconds();
            if (start < 0) return 2;
            CalculatorStatus status = application_numeric_root_solve(&root, scene->equation, lower, upper, &context, &error);
            double ms = (benchmark_seconds()-start)*1000;
            size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
            bool valid = status == CALCULATOR_OK && verify(root, scene); failed |= !valid;
            char *x = NULL, *residual = NULL;
            if (root && root->x && (bigrational_to_string(root->x, &x) != BIGRATIONAL_OK ||
                bigdecimal_to_string(root->residual, &residual) != BIGDECIMAL_OK)) return 2;
            printf("%s,%d,%d,%.6f,%d,%s,%zu,%zu,%zu,%zu,%d,%s,%s\n", scene->name, precisions[p], run, ms,
                (int)status, root ? application_numeric_root_kind(root->kind) : "error", root ? root->evaluations : 0U,
                root ? root->iterations : 0U, calls, bytes, valid ? 1 : 0, x ? x : "", residual ? residual : "");
            free(x); free(residual);
            application_numeric_root_destroy(root); bigrational_destroy(lower); bigrational_destroy(upper);
        }
    return failed ? 1 : 0;
}
