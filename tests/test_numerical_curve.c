#include <numforge/curve.h>
#include <numforge/runtime.h>
#include "src/runtime/numforge_alloc.h"
#include <unity.h>

enum { TOL, SPAN, TIGHT, WIDTH, LOW, HIGH, CAMERA_TOL, PIXEL, PARAMS };
static BigDecimal *parameters[PARAMS], *x[3], *y[3], *values[3][2];
static const BigDecimal *channels[3][2];
static NumForgeCurveChannelKind kinds[2];
static NumForgeCurveSample samples[3];
static NumForgeCurveOptions options;
static NumForgeCurveCamera camera;
static NumForgeCurveClassifier *classifier;
static NumForgeCurveAssessment assessment;
static void set(BigDecimal *value, const char *text)
{ TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text)); }
void setUp(void)
{
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(true));
#endif
    const char *texts[] = {"0.01", "3", "0.5", "0.25", "-1", "1", "0.1", "0.1"};
    for (size_t i = 0; i < PARAMS; ++i) { parameters[i] = bigdecimal_create(); TEST_ASSERT_NOT_NULL(parameters[i]); set(parameters[i], texts[i]); }
    const char *coordinates[] = {"0", "0.125", "0.25"};
    for (size_t i = 0; i < 3; ++i) {
        x[i] = bigdecimal_create(); y[i] = bigdecimal_create(); TEST_ASSERT_NOT_NULL(x[i]); TEST_ASSERT_NOT_NULL(y[i]);
        set(x[i], coordinates[i]); set(y[i], coordinates[i]);
        for (size_t j = 0; j < 2; ++j) {
            values[i][j] = bigdecimal_create(); TEST_ASSERT_NOT_NULL(values[i][j]); channels[i][j] = values[i][j];
        }
        samples[i] = (NumForgeCurveSample){x[i], y[i], true, channels[i], 0, false};
    }
    kinds[0] = NUMFORGE_CURVE_PHASE; kinds[1] = NUMFORGE_CURVE_STEP;
    camera = (NumForgeCurveCamera){parameters[LOW], parameters[HIGH], parameters[CAMERA_TOL], parameters[PIXEL]};
    options = (NumForgeCurveOptions){parameters[TOL], parameters[SPAN], parameters[TIGHT], parameters[WIDTH], false, NULL, kinds, 0};
    assessment = (NumForgeCurveAssessment){(NumForgeRefinementDecision)99, true};
}
void tearDown(void)
{
    numforge_test_allocator_end(); numforge_test_budget_expire_after(0); numforge_budget_end();
    numforge_curve_classifier_destroy(classifier); classifier = NULL;
    for (size_t i = 0; i < PARAMS; ++i) bigdecimal_destroy(parameters[i]);
    for (size_t i = 0; i < 3; ++i) {
        bigdecimal_destroy(x[i]); bigdecimal_destroy(y[i]);
        for (size_t j = 0; j < 2; ++j) bigdecimal_destroy(values[i][j]);
    }
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
    TEST_ASSERT_TRUE(numforge_alloc_stats_complete()); TEST_ASSERT_EQUAL_UINT64(0, numforge_alloc_stats_live());
    TEST_ASSERT_TRUE(numforge_alloc_stats_track(false));
#endif
}
static void create(void)
{
    numforge_curve_classifier_destroy(classifier); classifier = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_curve_classifier_create(&classifier, &options));
}
static void expect(NumForgeRefinementDecision decision, bool clipped)
{
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_curve_assess(&assessment, classifier, samples));
    TEST_ASSERT_EQUAL(decision, assessment.decision); TEST_ASSERT_EQUAL(clipped, assessment.screen_clipped);
}
static void channel_count(size_t count)
{ options.channel_count = count; for (size_t i = 0; i < 3; ++i) samples[i].channel_count = count; }
static void preserved(void)
{ TEST_ASSERT_EQUAL_INT(99, assessment.decision); TEST_ASSERT_TRUE(assessment.screen_clipped); }
static void test_chord_boundary_and_sample_classification(void)
{
    create(); expect(NUMFORGE_REFINEMENT_ACCEPT, false);
    set(y[1], "0.135"); expect(NUMFORGE_REFINEMENT_ACCEPT, false);
    set(y[1], "0.135000000000000000000000000000000001"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    samples[1].real = false; samples[1].y = NULL; expect(NUMFORGE_REFINEMENT_SPLIT, false);
    samples[0].real = samples[2].real = false; expect(NUMFORGE_REFINEMENT_SKIP, false);
}
static void test_periodic_and_step_guards_and_missing_metadata(void)
{
    channel_count(2); set(values[1][0], "0.25"); set(values[2][0], "0.5"); create();
    expect(NUMFORGE_REFINEMENT_ACCEPT, false);
    set(values[2][0], "0.5000000000000000000000000000000001"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    set(values[2][0], "0.5"); set(values[1][1], "1"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    set(values[1][1], "0"); samples[1].channels_unknown = true; expect(NUMFORGE_REFINEMENT_SPLIT, false);
    samples[1].channels_unknown = false; samples[1].channel_count = 1; expect(NUMFORGE_REFINEMENT_SPLIT, false);
    samples[1].channel_count = 2; channels[1][1] = NULL; expect(NUMFORGE_REFINEMENT_SPLIT, false);
    channels[1][1] = values[1][1]; samples[1].channels = NULL; expect(NUMFORGE_REFINEMENT_SPLIT, false);
    samples[1].channels = channels[1]; expect(NUMFORGE_REFINEMENT_ACCEPT, false);
}
static void test_legacy_width_relaxation_respects_broad_phase_and_steps(void)
{
    options.smooth = true; channel_count(2); set(y[1], "1");
    set(values[1][0], "0.5"); set(values[2][0], "1"); create(); expect(NUMFORGE_REFINEMENT_ACCEPT, false);
    set(values[2][0], "3.0001"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    set(values[2][0], "1"); set(values[1][1], "1"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    set(values[1][1], "0"); set(x[2], "0.2501"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
}
static void test_camera_requires_width_error_and_tight_phases(void)
{
    options.camera = &camera; options.smooth = true; channel_count(1);
    set(x[1], "0.025"); set(x[2], "0.05"); set(y[0], "0"); set(y[1], "0.05"); set(y[2], "0");
    create(); expect(NUMFORGE_REFINEMENT_ACCEPT, false);
    set(x[2], "0.2"); expect(NUMFORGE_REFINEMENT_SPLIT, false); set(x[2], "0.05");
    set(y[1], "0.100001"); expect(NUMFORGE_REFINEMENT_SPLIT, false); set(y[1], "0.05");
    set(values[2][0], "0.6"); expect(NUMFORGE_REFINEMENT_SPLIT, false); set(values[2][0], "0");
    options.smooth = false; create(); expect(NUMFORGE_REFINEMENT_SPLIT, false);
}
static void test_offscreen_deferral_never_connects_and_checks_monotonicity(void)
{
    options.camera = &camera; channel_count(1); kinds[0] = NUMFORGE_CURVE_STEP;
    set(values[1][0], "1"); set(y[0], "2"); set(y[1], "3"); set(y[2], "4"); create();
    expect(NUMFORGE_REFINEMENT_SKIP, true);
    set(y[0], "1"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    set(y[0], "3"); set(y[1], "2"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    set(y[0], "2"); set(y[1], "3"); set(x[2], "0.251"); expect(NUMFORGE_REFINEMENT_SPLIT, false);
    set(x[2], "0.25"); set(y[0], "-2"); set(y[1], "-3"); set(y[2], "-4"); expect(NUMFORGE_REFINEMENT_SKIP, true);
}
static void test_classifier_owns_policy_and_channels(void)
{
    channel_count(1); set(values[1][0], "0.25"); set(values[2][0], "0.5"); create();
    set(parameters[TOL], "0"); kinds[0] = NUMFORGE_CURVE_STEP; options.channel_count = 0;
    set(y[1], "0.135"); expect(NUMFORGE_REFINEMENT_ACCEPT, false);
}
static void test_thousand_digit_cancellation_and_collapsed_projection(void)
{
    set(y[0], "1E1000"); set(y[2], "1E1000"); set(values[0][0], "1E-1000");
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_add(y[1], y[0], values[0][0]));
    set(parameters[TOL], "1E-1000"); set(x[0], "1"); set(x[1], "1"); set(x[2], "1");
    create(); expect(NUMFORGE_REFINEMENT_ACCEPT, false);
    set(parameters[TOL], "1E-1001"); create(); expect(NUMFORGE_REFINEMENT_SPLIT, false);
}
static void test_invalid_arguments_and_order_preserve_outputs(void)
{
    create(); NumForgeCurveClassifier *old = classifier;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_classifier_create(NULL, &options));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_classifier_create(&classifier, NULL));
    options.tolerance = NULL; TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    options.tolerance = parameters[TOL]; set(parameters[SPAN], "0");
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    set(parameters[SPAN], "0.1"); TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    set(parameters[SPAN], "3"); set(parameters[TOL], "-1"); TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    set(parameters[TOL], "0.01"); options.camera = &camera; camera.minimum = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    camera.minimum = parameters[HIGH]; TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    camera.minimum = parameters[LOW]; options.channel_count = SIZE_MAX;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    channel_count(1); kinds[0] = (NumForgeCurveChannelKind)99;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_curve_classifier_create(&classifier, &options));
    TEST_ASSERT_EQUAL_PTR(old, classifier);
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_assess(NULL, classifier, samples));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_assess(&assessment, NULL, samples));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_assess(&assessment, classifier, NULL));
    samples[0].y = NULL; TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_NULL_ARGUMENT, numforge_curve_assess(&assessment, classifier, samples)); preserved();
    samples[0].y = y[0]; set(x[1], "-1");
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_INVALID_ARGUMENT, numforge_curve_assess(&assessment, classifier, samples)); preserved();
    numforge_curve_classifier_destroy(NULL);
}
static void test_every_constructor_allocation_failure_preserves_handle(void)
{
    options.camera = &camera; channel_count(2); create();
    numforge_test_allocator_begin(0); NumForgeCurveClassifier *attempt = NULL;
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OK, numforge_curve_classifier_create(&attempt, &options));
    size_t count = numforge_test_allocator_call_count(); numforge_test_allocator_end(); numforge_curve_classifier_destroy(attempt);
    for (size_t i = 1; i <= count; ++i) {
        attempt = classifier; numforge_test_allocator_begin(i);
        NumForgeNumericalStatus status = numforge_curve_classifier_create(&attempt, &options); numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status); TEST_ASSERT_EQUAL_PTR(classifier, attempt);
    }
}
static void test_every_assessment_allocation_failure_and_reuse(void)
{
    options.camera = &camera; channel_count(1); set(y[0], "2"); set(y[1], "3.1"); set(y[2], "4"); create();
    numforge_test_allocator_begin(0); expect(NUMFORGE_REFINEMENT_SKIP, true);
    size_t count = numforge_test_allocator_call_count(); numforge_test_allocator_end();
    TEST_ASSERT_GREATER_THAN(0, count);
    for (size_t i = 1; i <= count; ++i) {
        create(); assessment = (NumForgeCurveAssessment){(NumForgeRefinementDecision)99, true};
        numforge_test_allocator_begin(i); NumForgeNumericalStatus status = numforge_curve_assess(&assessment, classifier, samples); numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, status); preserved(); expect(NUMFORGE_REFINEMENT_SKIP, true);
    }
}
static void test_inherited_budget_failure_and_reuse(void)
{
    channel_count(2); create();
    TEST_ASSERT_TRUE(numforge_budget_begin(0, SIZE_MAX, SIZE_MAX));
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_curve_assess(&assessment, classifier, samples)); preserved();
    TEST_ASSERT_EQUAL(NUMFORGE_BUDGET_TIME, numforge_budget_failure()); numforge_budget_end();
    TEST_ASSERT_TRUE(numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX)); numforge_test_budget_expire_after(20);
    TEST_ASSERT_EQUAL(NUMFORGE_NUMERICAL_OUT_OF_MEMORY, numforge_curve_assess(&assessment, classifier, samples)); preserved();
    numforge_test_budget_expire_after(0); numforge_budget_end(); expect(NUMFORGE_REFINEMENT_ACCEPT, false);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_chord_boundary_and_sample_classification);
    RUN_TEST(test_periodic_and_step_guards_and_missing_metadata);
    RUN_TEST(test_legacy_width_relaxation_respects_broad_phase_and_steps);
    RUN_TEST(test_camera_requires_width_error_and_tight_phases);
    RUN_TEST(test_offscreen_deferral_never_connects_and_checks_monotonicity);
    RUN_TEST(test_classifier_owns_policy_and_channels);
    RUN_TEST(test_thousand_digit_cancellation_and_collapsed_projection);
    RUN_TEST(test_invalid_arguments_and_order_preserve_outputs);
    RUN_TEST(test_every_constructor_allocation_failure_preserves_handle);
    RUN_TEST(test_every_assessment_allocation_failure_and_reuse);
    RUN_TEST(test_inherited_budget_failure_and_reuse);
    return UNITY_END();
}
