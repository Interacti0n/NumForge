#include "src/application/equations/numeric_root.h"
#include "benchmarks/common/benchmark_clock.h"
#include "src/runtime/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Scene { const char *name, *input, *lower, *upper; size_t count; ApplicationRootScanStop stop; } Scene;
static const Scene scenes[] = {
    {"cubic", "x*(x-2)*(x+3)=0", "-4", "4", 3U, APPLICATION_SCAN_FINISHED},
    {"sine", "sin(x)=0", "-10", "10", 7U, APPLICATION_SCAN_FINISHED},
    {"cosine_fixed", "cos(x)=x", "-2", "2", 1U, APPLICATION_SCAN_FINISHED},
    {"partial_log_domain", "ln(x)=0", "-2", "2", 1U, APPLICATION_SCAN_FINISHED},
    {"sampled_pole", "1/x=0", "-1", "1", 0U, APPLICATION_SCAN_FINISHED},
    {"vanishing_jump", "x-0.1234+1E-30*(x-0.1234)/abs(x-0.1234)=0", "0", "1", 0U, APPLICATION_SCAN_FINISHED},
    {"scaled_pole", "1E-100/(x-0.1234)=0", "0", "1", 0U, APPLICATION_SCAN_FINISHED},
    {"unsampled_tangent", "(x-0.1234)^2=0", "0", "1", 1U, APPLICATION_SCAN_FINISHED},
    {"cluster_in_one_cell", "(x-0.01)*(x-0.02)=0", "0", "128", 2U, APPLICATION_SCAN_FINISHED},
    {"rational_tangent", "(x-1/3)^2=0", "0", "1", 1U, APPLICATION_SCAN_FINISHED},
    {"positive_minimum", "(x-0.1234)^2+1E-100=0", "0", "1", 0U, APPLICATION_SCAN_FINISHED},
    {"narrow_pair", "(x-0.1234)^2-1E-20=0", "0", "1", 2U, APPLICATION_SCAN_FINISHED},
    {"nonpolynomial_tangent", "(cos(x)-x)^2=0", "0", "1", 0U, APPLICATION_SCAN_FINISHED},
    {"root_limit", "x=x", "0", "1", 32U, APPLICATION_SCAN_ROOT_LIMIT},
    {"evaluation_limit", "2*mod(floor(x);2)-1=0", "0", "128", 0U, APPLICATION_SCAN_EVALUATION_LIMIT}
};
static bool bound(BigRational *value, const char *text)
{
    BigDecimal *decimal = bigdecimal_create();
    bool valid = decimal && bigdecimal_set_string(decimal,text) == BIGDECIMAL_OK && bigrational_from_bigdecimal(value,decimal) == BIGRATIONAL_OK;
    bigdecimal_destroy(decimal); return valid;
}
int main(int argc, char **argv)
{
    bool quick = argc == 2 && !strcmp(argv[1],"--quick");
    if (argc != 1 && !quick) return 2;
    const int precisions[] = {16,34,100}; bool failed = false;
    puts("case,precision,repeat,ms,status,stop,count,evaluations,iterations,segments,domain_gaps,rejected_brackets,allocation_bytes,checked,candidate_samples,exact_candidate_roots,candidate_gaps,xs");
    for (size_t p = quick ? 1U : 0U; p < (quick ? 2U : 3U); ++p)
        for (size_t i = 0U; i < sizeof(scenes)/sizeof(scenes[0]); ++i) for (int run = 1; run <= (quick ? 1 : 3); ++run) {
            const Scene *scene = &scenes[i]; CalculatorContext context; calculator_context_init(&context); context.division_scale = precisions[p];
            BigRational *low = bigrational_create(), *high = bigrational_create(); ApplicationRootScan *scan = NULL; CalculatorError error;
            if (!low || !high || !bound(low,scene->lower) || !bound(high,scene->upper)) return 2;
            numforge_alloc_stats_reset(); double start = benchmark_seconds(); if (start < 0) return 2;
            CalculatorStatus status = application_numeric_roots_scan(&scan,scene->input,low,high,&context,&error);
            double ms = (benchmark_seconds()-start)*1000; size_t bytes = numforge_alloc_stats_bytes();
            NumForgeRootScanStats scan_stats; numforge_root_scan_stats(scan, &scan_stats);
            bool valid = status == CALCULATOR_OK && scan && numforge_root_scan_count(scan) == scene->count && numforge_root_scan_stop(scan) == scene->stop &&
                scan_stats.evaluations <= APPLICATION_ROOT_SCAN_MAX_EVALUATIONS && scan_stats.scanned_segments <= APPLICATION_ROOT_SCAN_SEGMENTS;
            failed |= !valid;
            printf("%s,%d,%d,%.6f,%d,%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%d,%zu,%zu,%zu,",scene->name,precisions[p],run,ms,(int)status,
                scan ? application_numeric_roots_stop(numforge_root_scan_stop(scan)) : "error",scan ? numforge_root_scan_count(scan) : 0U,scan ? scan_stats.evaluations : 0U,
                scan ? scan_stats.iterations : 0U,scan ? scan_stats.scanned_segments : 0U,scan ? scan_stats.domain_gaps : 0U,scan ? scan_stats.rejected_brackets : 0U,bytes,valid ? 1 : 0,
                scan ? scan_stats.candidate_samples : 0U,scan ? scan_stats.candidate_roots : 0U,scan ? scan_stats.candidate_gaps : 0U);
            if (scan) for (size_t j = 0U; j < numforge_root_scan_count(scan); ++j) {
                char *text = NULL; if (bigrational_to_string(numforge_root_x(numforge_root_scan_root(scan, j)),&text) != BIGRATIONAL_OK) return 2;
                printf("%s%s",j ? ";" : "",text); free(text);
            }
            putchar('\n'); application_numeric_roots_destroy(scan); bigrational_destroy(low); bigrational_destroy(high);
        }
    return failed ? 1 : 0;
}
