#ifndef NUMFORGE_EXAMPLE_CURVE_FUNCTION_H
#define NUMFORGE_EXAMPLE_CURVE_FUNCTION_H
#include <numforge/curve.h>
#include <string.h>

typedef struct CurveFunction {
    BigDecimal *values[13];
    const BigDecimal *channels[3][1];
    NumForgeCurveSample samples[3];
    NumForgeCurveClassifier *classifier;
} CurveFunction;
static void curve_function_destroy(CurveFunction *f)
{
    numforge_curve_classifier_destroy(f->classifier);
    for (size_t i = 0; i < 13; ++i) bigdecimal_destroy(f->values[i]);
    memset(f, 0, sizeof(*f));
}
static bool curve_function_policy(CurveFunction *f)
{
    NumForgeCurveChannelKind kind = NUMFORGE_CURVE_PHASE;
    NumForgeCurveOptions options = {f->values[0], f->values[1], f->values[2], f->values[3], false, NULL, &kind, 1};
    numforge_curve_classifier_destroy(f->classifier); f->classifier = NULL;
    return numforge_curve_classifier_create(&f->classifier, &options) == NUMFORGE_NUMERICAL_OK;
}
static bool curve_function_init(CurveFunction *f)
{
    memset(f, 0, sizeof(*f));
    const char *texts[] = {"0.01", "3", "0.5", "0.25", "0", "0.125", "0.25",
        "0", "0.125", "0.25", "0", "0.25", "0.5"};
    for (size_t i = 0; i < 13; ++i) {
        f->values[i] = bigdecimal_create();
        if (!f->values[i] || bigdecimal_set_string(f->values[i], texts[i]) != BIGDECIMAL_OK) {
            curve_function_destroy(f); return false;
        }
    }
    for (size_t i = 0; i < 3; ++i) {
        f->channels[i][0] = f->values[10 + i];
        f->samples[i] = (NumForgeCurveSample){f->values[4 + i], f->values[7 + i], true, f->channels[i], 1, false};
    }
    if (!curve_function_policy(f)) { curve_function_destroy(f); return false; }
    return true;
}
#endif
