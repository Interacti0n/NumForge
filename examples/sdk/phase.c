#include "phase_function.h"
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    PhaseFunction f; if (!phase_function_init(&f)) return 1;
    NumForgePhaseOptions options; numforge_phase_options_init(&options); options.period = f.period;
    NumForgePhaseLocator *locator = NULL; NumForgePhaseReport report; char *x = NULL, *target = NULL; int outcome = 1;
    if (numforge_phase_locator_create(&locator, &options) != NUMFORGE_NUMERICAL_OK ||
        numforge_phase_locate(&report, locator, f.lower, f.upper, f.phase_lower, f.phase_upper, f.offset, phase_function_evaluate, &f) != NUMFORGE_NUMERICAL_OK ||
        report.kind != NUMFORGE_PHASE_FOUND || report.evaluations != 1 ||
        bigrational_to_string(numforge_phase_locator_x(locator), &x) != BIGRATIONAL_OK ||
        bigdecimal_to_string(numforge_phase_locator_target(locator), &target) != BIGDECIMAL_OK) goto done;
    printf("x=%s; phase target=%s; evaluations=%zu\n", x, target, report.evaluations); outcome = 0;
done:
    free(x); free(target); numforge_phase_locator_destroy(locator); phase_function_destroy(&f); return outcome;
}
