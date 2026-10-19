#include "src/application/equations/numeric_root.h"
#include "src/application/equations/numeric_root_experiment.h"
#include "benchmarks/numeric_root_cases.h"
#include "benchmarks/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
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
static bool verify(const ApplicationNumericRoot *root, const NumericRootCase *scene, bool experiment)
{
    if (!root || root->evaluations > 192U || root->iterations > 96U) return false;
    if (root->kind != APPLICATION_ROOT_FOUND) return scene->outcome != APPLICATION_ROOT_FOUND &&
        (experiment || root->kind == scene->outcome) && !root->x && !root->residual;
    if (!scene->reference || (!experiment && scene->outcome != APPLICATION_ROOT_FOUND)) return false;
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
    bool quick = false, adversarial = false, comparison = false, hybrid = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--quick") && !quick) quick = true;
        else if (!strcmp(argv[i], "--adversarial") && !adversarial) adversarial = true;
        else if (!strcmp(argv[i], "--compare") && !comparison) comparison = true;
        else if (!strcmp(argv[i], "--hybrid") && !hybrid) hybrid = true;
        else return 2;
    }
    const int precisions[] = {16, 34, 100}; bool failed = false;
    puts("case,precision,repeat,ms,status,outcome,evaluations,iterations,allocation_calls,allocation_bytes,verified,x,residual,algorithm,bisections,interpolations,secants,quadratics,nudges,rejections,fallback_steps,switch_offs");
    NumericRootCase gap = {"vanishing_jump", "x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0", "0", "1", NULL, APPLICATION_ROOT_DOMAIN_UNVERIFIED};
    size_t count = adversarial ? 1U : sizeof(numeric_root_cases)/sizeof(numeric_root_cases[0]);
    for (size_t p = quick ? 1U : 0U; p < (quick ? 2U : 3U); ++p)
        for (size_t i = 0U; i < count; ++i) for (int run = 1; run <= (quick || adversarial ? 1 : 3); ++run)
            for (int algorithm = 0; algorithm <= (hybrid ? 2 : comparison ? 1 : 0); ++algorithm) {
            const NumericRootCase *scene = adversarial ? &gap : &numeric_root_cases[i];
            CalculatorContext context; calculator_context_init(&context); context.division_scale = precisions[p];
            CalculatorError error; ApplicationNumericRoot *root = NULL; ApplicationNumericRootStats stats = {0};
            BigRational *lower = bigrational_create(), *upper = bigrational_create();
            if (!lower || !upper || !bound(lower, scene->lower) || !bound(upper, scene->upper)) return 2;
            numforge_alloc_stats_reset(); double start = benchmark_seconds();
            if (start < 0) return 2;
            CalculatorStatus status = application_numeric_root_method(&root, scene->equation, lower, upper, &context, &error, (ApplicationNumericRootMethod)algorithm, &stats);
            double ms = (benchmark_seconds()-start)*1000;
            size_t calls = numforge_alloc_stats_calls(), bytes = numforge_alloc_stats_bytes();
            bool valid = status == CALCULATOR_OK && verify(root, scene, algorithm != 0) &&
                stats.bisections+stats.interpolations == root->iterations &&
                stats.secants+stats.quadratics == stats.interpolations && stats.nudges <= stats.interpolations &&
                (algorithm ? stats.rejections+stats.fallback_steps == stats.bisections : stats.rejections == 0U) &&
                stats.switch_offs <= 1U && (!stats.fallback_steps || stats.switch_offs == 1U);
            failed |= !valid;
            char *x = NULL, *residual = NULL;
            if (root && root->x && (bigrational_to_string(root->x, &x) != BIGRATIONAL_OK ||
                bigdecimal_to_string(root->residual, &residual) != BIGDECIMAL_OK)) return 2;
            printf("%s,%d,%d,%.6f,%d,%s,%zu,%zu,%zu,%zu,%d,%s,%s,%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", scene->name, precisions[p], run, ms,
                (int)status, root ? application_numeric_root_kind(root->kind) : "error", root ? root->evaluations : 0U,
                root ? root->iterations : 0U, calls, bytes, valid ? 1 : 0, x ? x : "", residual ? residual : "", algorithm == 2 ? "hybrid" : algorithm ? "brent" : "bisection",
                stats.bisections, stats.interpolations, stats.secants, stats.quadratics, stats.nudges, stats.rejections, stats.fallback_steps, stats.switch_offs);
            free(x); free(residual);
            application_numeric_root_destroy(root); bigrational_destroy(lower); bigrational_destroy(upper);
        }
    return failed ? 1 : 0;
}
