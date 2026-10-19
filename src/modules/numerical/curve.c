#include <numforge/curve.h>
#include "src/modules/numerical/numerical_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>

struct NumForgeCurveClassifier {
    NumForgeCurveOptions policy;
    NumForgeCurveCamera camera;
    NumForgeCurveChannelKind *channels;
    BigDecimal *parameters[8], *half, *sum, *delta;
};
static NumForgeNumericalStatus checked(NumForgeNumericalStatus status)
{ return numforge_budget_check() ? status : NUMFORGE_NUMERICAL_OUT_OF_MEMORY; }
void numforge_curve_classifier_destroy(NumForgeCurveClassifier *classifier)
{
    if (!classifier) return;
    for (size_t i = 0; i < 8; ++i) bigdecimal_destroy(classifier->parameters[i]);
    bigdecimal_destroy(classifier->half); bigdecimal_destroy(classifier->sum); bigdecimal_destroy(classifier->delta);
    free(classifier->channels); free(classifier);
}
NumForgeNumericalStatus numforge_curve_classifier_create(NumForgeCurveClassifier **result,
    const NumForgeCurveOptions *options)
{
    if (!result || !options || !options->tolerance || !options->phase_span ||
        !options->strict_phase_span || !options->coarse_width || (options->channel_count && !options->channels))
        return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    if (options->channel_count > SIZE_MAX / sizeof(*options->channels)) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    const NumForgeCurveCamera *camera = options->camera;
    if (camera && (!camera->minimum || !camera->maximum || !camera->tolerance || !camera->pixel_width))
        return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    NumForgeCurveClassifier *c = numforge_calloc(1, sizeof(*c));
    if (!c) return checked(NUMFORGE_NUMERICAL_OUT_OF_MEMORY);
    c->policy = *options;
    c->half = bigdecimal_create(); c->sum = bigdecimal_create(); c->delta = bigdecimal_create();
    NumForgeNumericalStatus status = c->half && c->sum && c->delta ? NUMFORGE_NUMERICAL_OK : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    const BigDecimal *inputs[] = {options->tolerance, options->phase_span, options->strict_phase_span,
        options->coarse_width, camera ? camera->minimum : NULL, camera ? camera->maximum : NULL,
        camera ? camera->tolerance : NULL, camera ? camera->pixel_width : NULL};
    for (size_t i = 0; status == NUMFORGE_NUMERICAL_OK && i < (camera ? 8U : 4U); ++i) {
        c->parameters[i] = bigdecimal_create();
        status = c->parameters[i] ? decimal(bigdecimal_copy(c->parameters[i], inputs[i])) : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        if (status == NUMFORGE_NUMERICAL_OK && i != 4 && i != 5) {
            int sign = 0; status = decimal(bigdecimal_sign(&sign, c->parameters[i]));
            if (status == NUMFORGE_NUMERICAL_OK && (sign < 0 || (!sign && (i == 1 || i == 2))))
                status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
        }
    }
    int comparison = 0;
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_compare(&comparison, c->parameters[2], c->parameters[1]));
    if (status == NUMFORGE_NUMERICAL_OK && comparison > 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (status == NUMFORGE_NUMERICAL_OK && camera) {
        status = decimal(bigdecimal_compare(&comparison, c->parameters[4], c->parameters[5]));
        if (status == NUMFORGE_NUMERICAL_OK && comparison >= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_set_string(c->half, "0.5"));
    if (status == NUMFORGE_NUMERICAL_OK && options->channel_count) {
        c->channels = numforge_calloc(options->channel_count, sizeof(*c->channels));
        if (!c->channels) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    for (size_t i = 0; status == NUMFORGE_NUMERICAL_OK && i < options->channel_count; ++i) {
        if (!numforge_budget_check()) { status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY; break; }
        c->channels[i] = options->channels[i];
        if (c->channels[i] != NUMFORGE_CURVE_PHASE && c->channels[i] != NUMFORGE_CURVE_STEP)
            status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
    c->policy.tolerance = c->parameters[0]; c->policy.phase_span = c->parameters[1];
    c->policy.strict_phase_span = c->parameters[2]; c->policy.coarse_width = c->parameters[3];
    c->policy.channels = c->channels;
    if (camera) {
        c->camera = (NumForgeCurveCamera){c->parameters[4], c->parameters[5], c->parameters[6], c->parameters[7]};
        c->policy.camera = &c->camera;
    }
    status = checked(status);
    if (status == NUMFORGE_NUMERICAL_OK) *result = c;
    else numforge_curve_classifier_destroy(c);
    return status;
}

#define TRY(operation) do { status = decimal(operation); if (status != NUMFORGE_NUMERICAL_OK) goto done; } while (0)
NumForgeNumericalStatus numforge_curve_assess(NumForgeCurveAssessment *result,
    NumForgeCurveClassifier *c, const NumForgeCurveSample samples[3])
{
    if (!result || !c || !samples) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    NumForgeCurveAssessment assessment = {NUMFORGE_REFINEMENT_SPLIT, false};
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    if (!samples[0].real && !samples[1].real && !samples[2].real) { assessment.decision = NUMFORGE_REFINEMENT_SKIP; goto done; }
    if (!samples[0].real || !samples[1].real || !samples[2].real) goto done;
    for (size_t i = 0; i < 3; ++i) if (!samples[i].x || !samples[i].y) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    int order = 0;
    for (size_t i = 0; i < 2; ++i) {
        TRY(bigdecimal_compare(&order, samples[i].x, samples[i + 1].x));
        if (order > 0) { status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT; goto done; }
    }
    const NumForgeCurveOptions *p = &c->policy;
    TRY(bigdecimal_add(c->sum, samples[0].y, samples[2].y));
    TRY(bigdecimal_mul(c->sum, c->sum, c->half));
    TRY(bigdecimal_sub(c->delta, samples[1].y, c->sum));
    TRY(bigdecimal_abs(c->delta, c->delta));
    int comparison = 0, camera_comparison = 1;
    TRY(bigdecimal_compare(&comparison, c->delta, p->tolerance));
    if (p->camera) TRY(bigdecimal_compare(&camera_comparison, c->delta, p->camera->tolerance));
    bool phases_safe = true, phases_tight = true, steps_safe = true;
    for (size_t i = 0; i < 3; ++i)
        if (samples[i].channels_unknown || samples[i].channel_count != p->channel_count ||
            (p->channel_count && !samples[i].channels)) phases_safe = false;
    for (size_t channel = 0; phases_safe && channel < p->channel_count; ++channel) {
        if (!numforge_budget_check()) { status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY; goto done; }
        for (size_t i = 0; phases_safe && i < 2; ++i)
            for (size_t j = i + 1; phases_safe && j < 3; ++j) {
                const BigDecimal *a = samples[i].channels[channel], *b = samples[j].channels[channel];
                if (!a || !b) { phases_safe = false; break; }
                if (p->channels[channel] == NUMFORGE_CURVE_STEP) {
                    TRY(bigdecimal_compare(&order, a, b));
                    if (order) steps_safe = false;
                    continue;
                }
                TRY(bigdecimal_sub(c->delta, a, b)); TRY(bigdecimal_abs(c->delta, c->delta));
                TRY(bigdecimal_compare(&order, c->delta, p->phase_span));
                if (order > 0) phases_safe = false;
                TRY(bigdecimal_compare(&order, c->delta, p->strict_phase_span));
                if (order > 0) phases_tight = false;
            }
    }
    bool coarse = false;
    if (p->camera && (comparison > 0 || !steps_safe) && phases_safe && phases_tight) {
        TRY(bigdecimal_sub(c->delta, samples[2].x, samples[0].x));
        TRY(bigdecimal_compare(&order, c->delta, p->coarse_width));
        if (order <= 0) {
            int ab = 0, bc = 0;
            TRY(bigdecimal_compare(&ab, samples[0].y, samples[1].y));
            TRY(bigdecimal_compare(&bc, samples[1].y, samples[2].y));
            if ((ab <= 0 && bc <= 0) || (ab >= 0 && bc >= 0)) {
                bool below = true, above = true;
                for (size_t i = 0; i < 3; ++i) {
                    int low = 0, high = 0;
                    TRY(bigdecimal_compare(&low, samples[i].y, p->camera->minimum));
                    TRY(bigdecimal_compare(&high, samples[i].y, p->camera->maximum));
                    below = below && low < 0; above = above && high > 0;
                }
                if (below || above) { assessment.decision = NUMFORGE_REFINEMENT_SKIP; assessment.screen_clipped = true; goto done; }
            }
        }
    }
    if (!p->camera && p->smooth && phases_safe && (comparison > 0 || !phases_tight)) {
        TRY(bigdecimal_sub(c->delta, samples[2].x, samples[0].x));
        TRY(bigdecimal_compare(&order, c->delta, p->coarse_width)); coarse = order <= 0;
    }
    if (p->camera && p->smooth && phases_safe && phases_tight && camera_comparison <= 0) {
        TRY(bigdecimal_sub(c->delta, samples[2].x, samples[0].x));
        TRY(bigdecimal_compare(&order, c->delta, p->camera->pixel_width)); coarse = order <= 0;
    }
    if (((comparison <= 0 && phases_tight) || coarse) && phases_safe && steps_safe)
        assessment.decision = NUMFORGE_REFINEMENT_ACCEPT;
done:
    status = checked(status);
    if (status == NUMFORGE_NUMERICAL_OK) *result = assessment;
    return status;
}
#undef TRY
