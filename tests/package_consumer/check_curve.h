#ifndef NUMFORGE_CONSUMER_CHECK_CURVE_H
#define NUMFORGE_CONSUMER_CHECK_CURVE_H
#include <numforge/curve.h>
static int curve_api_checks(void)
{
    BigDecimal *zero = bigdecimal_create(), *one = bigdecimal_create(), *half = bigdecimal_create();
    NumForgeCurveClassifier *classifier = NULL; NumForgeCurveAssessment result; int outcome = 1;
    if (!zero || !one || !half || bigdecimal_set_string(one, "1") != BIGDECIMAL_OK ||
        bigdecimal_set_string(half, "0.5") != BIGDECIMAL_OK) goto done;
    {
        NumForgeCurveChannelKind kind = NUMFORGE_CURVE_STEP;
        NumForgeCurveOptions options = {zero, one, half, zero, false, NULL, &kind, 1};
        const BigDecimal *a[] = {zero}, *b[] = {zero}, *c[] = {zero};
        NumForgeCurveSample samples[] = {{zero, zero, true, a, 1, false},
            {half, half, true, b, 1, false}, {one, one, true, c, 1, false}};
        if (numforge_curve_classifier_create(&classifier, &options) != NUMFORGE_NUMERICAL_OK ||
            numforge_curve_assess(&result, classifier, samples) != NUMFORGE_NUMERICAL_OK ||
            result.decision != NUMFORGE_REFINEMENT_ACCEPT || result.screen_clipped) goto done;
        b[0] = one;
        if (numforge_curve_assess(&result, classifier, samples) != NUMFORGE_NUMERICAL_OK ||
            result.decision != NUMFORGE_REFINEMENT_SPLIT || result.screen_clipped) goto done;
        outcome = 0;
    }
done:
    numforge_curve_classifier_destroy(classifier); bigdecimal_destroy(zero); bigdecimal_destroy(one); bigdecimal_destroy(half);
    return outcome;
}
#endif
