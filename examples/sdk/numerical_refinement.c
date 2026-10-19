#include "refinement_function.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    RefinementFunction f;
    if (!refinement_function_init(&f, "0", "1", "0.015625")) return 1;
    NumForgeRefinementOptions options; numforge_refinement_options_init(&options);
    options.max_points = REFINEMENT_EXAMPLE_CAPACITY;
    NumForgeRefinementStats stats;
    NumForgeNumericalStatus status = numforge_refine_intervals(&stats, f.count, &options, refinement_function_probe, &f);
    int outcome = 1;
    if (status == NUMFORGE_NUMERICAL_OK) {
        printf("points=%zu; probes=%zu; limits=%u\n", stats.points, stats.probes, stats.limits);
        outcome = stats.points == 9 && stats.probes == 7 && stats.limits == 0 ? 0 : 1;
        for (size_t i = 0; i < f.count && !outcome; ++i) {
            char *x = NULL, *y = NULL;
            if (bigrational_mul(f.value, f.points[i], f.points[i]) != BIGRATIONAL_OK ||
                bigrational_to_string(f.points[i], &x) != BIGRATIONAL_OK ||
                bigrational_to_string(f.value, &y) != BIGRATIONAL_OK) outcome = 1;
            else printf("x=%s; x*x=%s (exact)\n", x, y);
            free(x); free(y);
        }
    }
    refinement_function_destroy(&f); return outcome;
}
