#include "curve_function.h"
#include <stdio.h>
int main(void)
{
    CurveFunction f;
    if (!curve_function_init(&f)) return 1;
    NumForgeCurveAssessment assessment; int outcome = 1;
    if (numforge_curve_assess(&assessment, f.classifier, f.samples) != NUMFORGE_NUMERICAL_OK ||
        assessment.decision != NUMFORGE_REFINEMENT_ACCEPT) goto done;
    /* Identical sampled ordinates do not override an unsafe captured phase. */
    if (bigdecimal_set_string(f.values[12], "4") != BIGDECIMAL_OK ||
        numforge_curve_assess(&assessment, f.classifier, f.samples) != NUMFORGE_NUMERICAL_OK ||
        assessment.decision != NUMFORGE_REFINEMENT_SPLIT) goto done;
    puts("phase guard: accepted then split"); outcome = 0;
done:
    curve_function_destroy(&f); return outcome;
}
