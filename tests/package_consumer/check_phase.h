#ifndef NUMFORGE_CONSUMER_CHECK_PHASE_H
#define NUMFORGE_CONSUMER_CHECK_PHASE_H
#include <numforge/phase.h>
static NumForgeNumericalStatus consumer_phase(BigDecimal *value, bool *usable, const BigRational *x, void *user)
{
    (void)user;
    if (bigrational_to_bigdecimal(value, x, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK)
        return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    *usable = true; return NUMFORGE_NUMERICAL_OK;
}
static int phase_api_checks(void)
{
    BigDecimal *zero = bigdecimal_create(), *one = bigdecimal_create(), *half = bigdecimal_create();
    BigRational *lower = bigrational_create(), *upper = bigrational_create(), *expected = bigrational_create();
    NumForgePhaseLocator *locator = NULL; NumForgePhaseReport report; NumForgePhaseOptions options; int outcome = 1, comparison = 1;
    if (!zero || !one || !half || !lower || !upper || !expected || bigdecimal_set_string(one, "1") != BIGDECIMAL_OK ||
        bigdecimal_set_string(half, "0.5") != BIGDECIMAL_OK || bigrational_from_bigdecimal(upper, one) != BIGRATIONAL_OK ||
        bigrational_from_bigdecimal(expected, half) != BIGRATIONAL_OK) goto done;
    numforge_phase_options_init(&options); options.period = one;
    if (numforge_phase_locator_create(&locator, &options) != NUMFORGE_NUMERICAL_OK ||
        numforge_phase_locate(&report, locator, lower, upper, zero, one, half, consumer_phase, NULL) != NUMFORGE_NUMERICAL_OK ||
        report.kind != NUMFORGE_PHASE_FOUND || report.evaluations != 1 ||
        bigrational_compare(&comparison, expected, numforge_phase_locator_x(locator)) != BIGRATIONAL_OK || comparison ||
        bigdecimal_compare(&comparison, half, numforge_phase_locator_target(locator)) != BIGDECIMAL_OK || comparison) goto done;
    if (numforge_phase_locate(&report, locator, lower, upper, zero, zero, NULL, consumer_phase, NULL) != NUMFORGE_NUMERICAL_OK ||
        report.kind != NUMFORGE_PHASE_NO_CANDIDATE || report.evaluations || numforge_phase_locator_x(locator) || numforge_phase_locator_target(locator)) goto done;
    outcome = 0;
done:
    numforge_phase_locator_destroy(locator); bigdecimal_destroy(zero); bigdecimal_destroy(one); bigdecimal_destroy(half);
    bigrational_destroy(lower); bigrational_destroy(upper); bigrational_destroy(expected); return outcome;
}
#endif
