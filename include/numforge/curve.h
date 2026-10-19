#ifndef NUMFORGE_CURVE_H
#define NUMFORGE_CURVE_H
#include <numforge/numerical.h>
#ifdef __cplusplus
extern "C" {
#endif
/** @file curve.h
 * Sampled curve acceptance heuristics in NumForge::numforge, independent of
 * expressions, sessions and rendering. All arithmetic is exact on the supplied
 * finite decimals; those decimals need not be exact function values. No decision
 * proves continuity, absence of hidden features or a global interpolation error.
 */
typedef struct NumForgeCurveClassifier NumForgeCurveClassifier;
typedef enum NumForgeCurveChannelKind {
    NUMFORGE_CURVE_PHASE = 0, /**< Pairwise phase distances use the supplied spans. */
    NUMFORGE_CURVE_STEP       /**< All three captured step values must be equal. */
} NumForgeCurveChannelKind;
typedef struct NumForgeCurveCamera {
    const BigDecimal *minimum, *maximum; /**< Expanded clipping bounds, minimum<maximum. */
    const BigDecimal *tolerance;         /**< Nonnegative camera chord tolerance. */
    const BigDecimal *pixel_width;       /**< Nonnegative horizontal acceptance width. */
} NumForgeCurveCamera;
typedef struct NumForgeCurveOptions {
    const BigDecimal *tolerance;         /**< Nonnegative absolute midpoint chord tolerance. */
    const BigDecimal *phase_span;        /**< Positive broad pairwise phase limit. */
    const BigDecimal *strict_phase_span; /**< Positive tight limit, <=phase_span. */
    const BigDecimal *coarse_width;      /**< Nonnegative width for coarse/deferred intervals. */
    bool smooth;                        /**< Caller permits coarser display approximation. */
    const NumForgeCurveCamera *camera;   /**< NULL selects legacy width-only coarse acceptance. */
    const NumForgeCurveChannelKind *channels; /**< May be NULL when channel_count=0. */
    size_t channel_count;
} NumForgeCurveOptions;
typedef struct NumForgeCurveSample {
    const BigDecimal *x, *y; /**< Borrowed display coordinates; required only for real triples. */
    bool real;              /**< False for undefined/non-real/unit-valued samples. */
    const BigDecimal *const *channels; /**< Borrowed corresponding phase/step values. */
    size_t channel_count;
    bool channels_unknown; /**< Unknown/overflowed metadata prevents acceptance. */
} NumForgeCurveSample;
typedef struct NumForgeCurveAssessment {
    NumForgeRefinementDecision decision; /**< ACCEPT, SPLIT or SKIP for both child edges. */
    bool screen_clipped;                /**< SKIP arose from offscreen deferral. */
} NumForgeCurveAssessment;
/** Copy and validate options, all decimal values and channel kinds into a new
 * owned classifier. Caller inputs may be released/changed afterwards. The handle
 * owns reusable scratch objects and is thread-confined; separate handles may run
 * independently. Failures preserve *result; a preexisting handle is not freed.
 * @return NULL_ARGUMENT for missing mandatory pointers; INVALID_ARGUMENT for
 * invalid thresholds/camera/channel kinds or channel-size overflow; resource and
 * representation statuses as in numerical.h. No implicit budget is installed.
 */
NumForgeNumericalStatus numforge_curve_classifier_create(NumForgeCurveClassifier **result,
    const NumForgeCurveOptions *options);
/** NULL is accepted. */
void numforge_curve_classifier_destroy(NumForgeCurveClassifier *classifier);
/** Assess left, midpoint and right samples without retaining/mutating inputs.
 * Caller supplies the actual parameter midpoint; projected x coordinates may
 * coincide after rounding, but must be nondecreasing. All-non-real triples SKIP;
 * mixed triples SPLIT. Missing/unknown/mismatched channel data prevents acceptance.
 * Channel positions must describe the same operations across all samples, with
 * phase values and spans in consistent units. STEP equality and PHASE spans are
 * sampled safeguards, not domain certificates.
 * The broad phase limit gates every acceptance/deferral; tight phases are needed
 * for strict chord acceptance and camera width relaxation. STEP changes prevent
 * acceptance, but can defer a monotone offscreen triple without connecting it.
 * Smooth non-camera triples may use coarse_width; smooth camera triples require
 * both pixel_width and camera tolerance. Offscreen deferral requires a narrow,
 * monotone triple strictly beyond the same expanded camera edge and never ACCEPTs.
 *
 * OK overwrites *result; failures preserve it. Sample outputs belong to the caller.
 * Inherit budgets and check them during channel traversal; never begin/end a scope.
 * @return NULL_ARGUMENT for missing result/handle/sample array or real x/y;
 * INVALID_ARGUMENT for unordered real x coordinates; other resource/representation
 * statuses as in numerical.h. Exhaustion is OUT_OF_MEMORY; runtime.h distinguishes
 * its cause. The handle remains reusable after a failed call once the caller's
 * failed budget/fault scope is ended.
 */
NumForgeNumericalStatus numforge_curve_assess(NumForgeCurveAssessment *result,
    NumForgeCurveClassifier *classifier, const NumForgeCurveSample samples[3]);
#ifdef __cplusplus
}
#endif
#endif
