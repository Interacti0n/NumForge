#include <numforge/phase.h>
#include "src/modules/numerical/numerical_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>

enum { PERIOD, TOLERANCE, OFFSET, TARGET, INDEX, DELTA, RATIO, A, B, VALUE, ONE, DECIMALS };
enum { LOWER, UPPER, FRACTION, POSITION, RATIONALS };
struct NumForgePhaseLocator {
    NumForgePhaseOptions options;
    BigDecimal *decimals[DECIMALS];
    BigRational *rationals[RATIONALS];
    bool found;
};
static NumForgeNumericalStatus checked(NumForgeNumericalStatus status)
{ return numforge_budget_check() ? status : NUMFORGE_NUMERICAL_OUT_OF_MEMORY; }
void numforge_phase_options_init(NumForgePhaseOptions *options)
{
    if (options) *options = (NumForgePhaseOptions){NULL, NULL, 34, 8, 0, 0};
}
void numforge_phase_locator_destroy(NumForgePhaseLocator *locator)
{
    if (!locator) return;
    for (size_t i = 0; i < DECIMALS; ++i) bigdecimal_destroy(locator->decimals[i]);
    for (size_t i = 0; i < RATIONALS; ++i) bigrational_destroy(locator->rationals[i]);
    free(locator);
}
NumForgeNumericalStatus numforge_phase_locator_create(NumForgePhaseLocator **result,
    const NumForgePhaseOptions *options)
{
    if (!result || !options || !options->period) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    if (options->significant_digits < 1 || !options->max_evaluations) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    NumForgePhaseLocator *locator = numforge_calloc(1, sizeof(*locator));
    if (!locator) return checked(NUMFORGE_NUMERICAL_OUT_OF_MEMORY);
    locator->options = *options;
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    for (size_t i = 0; status == NUMFORGE_NUMERICAL_OK && i < DECIMALS; ++i) {
        locator->decimals[i] = bigdecimal_create();
        if (!locator->decimals[i]) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    for (size_t i = 0; status == NUMFORGE_NUMERICAL_OK && i < RATIONALS; ++i) {
        locator->rationals[i] = bigrational_create();
        if (!locator->rationals[i]) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    BigDecimal **d = locator->decimals;
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_copy(d[PERIOD], options->period));
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(options->tolerance ? bigdecimal_copy(d[TOLERANCE], options->tolerance) : bigdecimal_set_string(d[TOLERANCE], "1E-6"));
    if (status == NUMFORGE_NUMERICAL_OK) status = decimal(bigdecimal_set_string(d[ONE], "1"));
    for (size_t i = PERIOD; status == NUMFORGE_NUMERICAL_OK && i <= TOLERANCE; ++i) {
        int sign = 0; status = decimal(bigdecimal_sign(&sign, d[i]));
        if (status == NUMFORGE_NUMERICAL_OK && sign <= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    }
    locator->options.period = d[PERIOD]; locator->options.tolerance = d[TOLERANCE];
    status = checked(status);
    if (status == NUMFORGE_NUMERICAL_OK) *result = locator;
    else numforge_phase_locator_destroy(locator);
    return status;
}
const BigRational *numforge_phase_locator_x(const NumForgePhaseLocator *locator)
{ return locator && locator->found ? locator->rationals[POSITION] : NULL; }
const BigDecimal *numforge_phase_locator_target(const NumForgePhaseLocator *locator)
{ return locator && locator->found ? locator->decimals[TARGET] : NULL; }

#define DEC(op) do { status = decimal(op); if (status != NUMFORGE_NUMERICAL_OK) goto done; } while (0)
#define RAT(op) do { status = rational(op); if (status != NUMFORGE_NUMERICAL_OK) goto done; } while (0)
NumForgeNumericalStatus numforge_phase_locate(NumForgePhaseReport *result,
    NumForgePhaseLocator *locator, const BigRational *lower, const BigRational *upper,
    const BigDecimal *phase_lower, const BigDecimal *phase_upper, const BigDecimal *offset,
    NumForgePhaseEvaluate evaluate, void *user)
{
    if (locator) locator->found = false;
    if (!result || !locator || !lower || !upper || !phase_lower || !phase_upper || !evaluate)
        return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    NumForgeNumericalStatus status = NUMFORGE_NUMERICAL_OK;
    NumForgePhaseReport report = {NUMFORGE_PHASE_NO_CANDIDATE, 0};
    BigDecimal **d = locator->decimals; BigRational **r = locator->rationals;
    int comparison = 0, direction = 0;
    RAT(bigrational_compare(&comparison, lower, upper));
    if (comparison >= 0) { status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT; goto done; }
    RAT(bigrational_copy(r[LOWER], lower)); RAT(bigrational_copy(r[UPPER], upper));
    DEC(bigdecimal_copy(d[A], phase_lower)); DEC(bigdecimal_copy(d[B], phase_upper));
    DEC(bigdecimal_compare(&direction, d[A], d[B]));
    if (!direction) goto done;
    DEC(offset ? bigdecimal_copy(d[OFFSET], offset) : bigdecimal_set_string(d[OFFSET], "0"));
    DEC(bigdecimal_sub(d[DELTA], direction < 0 ? d[A] : d[B], d[OFFSET]));
    DEC(bigdecimal_div_significant(d[INDEX], d[DELTA], d[PERIOD], locator->options.significant_digits, BIGDECIMAL_ROUND_HALF_EVEN));
    DEC(bigdecimal_floor(d[INDEX], d[INDEX])); DEC(bigdecimal_add(d[INDEX], d[INDEX], d[ONE]));
    DEC(bigdecimal_mul(d[TARGET], d[INDEX], d[PERIOD])); DEC(bigdecimal_add(d[TARGET], d[TARGET], d[OFFSET]));
    DEC(bigdecimal_compare(&comparison, d[TARGET], direction < 0 ? d[B] : d[A]));
    if (comparison >= 0) goto done;
    DEC(bigdecimal_compare(&comparison, d[TARGET], direction < 0 ? d[A] : d[B]));
    if (comparison < 0) { report.kind = NUMFORGE_PHASE_PRECISION_LIMIT; goto done; }
    DEC(bigdecimal_sub(d[DELTA], d[TARGET], d[A])); DEC(bigdecimal_abs(d[DELTA], d[DELTA]));
    DEC(bigdecimal_compare(&comparison, d[DELTA], d[TOLERANCE])); if (comparison <= 0) goto done;
    DEC(bigdecimal_sub(d[DELTA], d[TARGET], d[B])); DEC(bigdecimal_abs(d[DELTA], d[DELTA]));
    DEC(bigdecimal_compare(&comparison, d[DELTA], d[TOLERANCE])); if (comparison <= 0) goto done;
    report.kind = NUMFORGE_PHASE_EVALUATION_LIMIT;
    while (report.evaluations < locator->options.max_evaluations) {
        if (!numforge_budget_check()) { status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY; goto done; }
        if (numforge_budget_available_bytes() < locator->options.reserve_bytes ||
            numforge_budget_available_ms() < locator->options.reserve_ms) {
            report.kind = NUMFORGE_PHASE_RESOURCE_LIMIT; break;
        }
        DEC(bigdecimal_sub(d[DELTA], d[B], d[A])); DEC(bigdecimal_sub(d[RATIO], d[TARGET], d[A]));
        DEC(bigdecimal_div_significant(d[RATIO], d[RATIO], d[DELTA], locator->options.significant_digits, BIGDECIMAL_ROUND_HALF_EVEN));
        RAT(bigrational_from_bigdecimal(r[FRACTION], d[RATIO])); RAT(bigrational_sub(r[POSITION], r[UPPER], r[LOWER]));
        RAT(bigrational_mul(r[POSITION], r[POSITION], r[FRACTION])); RAT(bigrational_add(r[POSITION], r[POSITION], r[LOWER]));
        int left = 0, right = 0;
        RAT(bigrational_compare(&left, r[POSITION], r[LOWER])); RAT(bigrational_compare(&right, r[POSITION], r[UPPER]));
        if (left <= 0 || right >= 0) { report.kind = NUMFORGE_PHASE_PRECISION_LIMIT; break; }
        bool usable = false; ++report.evaluations;
        status = evaluate(d[VALUE], &usable, r[POSITION], user);
        status = checked(status); if (status != NUMFORGE_NUMERICAL_OK) goto done;
        if (!usable) { report.kind = NUMFORGE_PHASE_UNUSABLE; break; }
        DEC(bigdecimal_sub(d[DELTA], d[VALUE], d[TARGET]));
        int side = 0; DEC(bigdecimal_compare(&side, d[VALUE], d[TARGET]));
        DEC(bigdecimal_abs(d[DELTA], d[DELTA])); DEC(bigdecimal_compare(&comparison, d[DELTA], d[TOLERANCE]));
        if (comparison <= 0) { report.kind = NUMFORGE_PHASE_FOUND; break; }
        bool replace_lower = direction < 0 ? side < 0 : side > 0;
        DEC(bigdecimal_copy(replace_lower ? d[A] : d[B], d[VALUE]));
        RAT(bigrational_copy(replace_lower ? r[LOWER] : r[UPPER], r[POSITION]));
    }
done:
    status = checked(status);
    if (status == NUMFORGE_NUMERICAL_OK) { *result = report; locator->found = report.kind == NUMFORGE_PHASE_FOUND; }
    return status;
}
#undef DEC
#undef RAT
