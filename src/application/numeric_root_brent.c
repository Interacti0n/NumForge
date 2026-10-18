#include "numeric_root_brent_internal.h"
#include "exact_functions.h"
#include <stdio.h>

/* Small helpers propagate sticky budget/resource failures; a failed arithmetic
 * operation must not silently become a bisection. */
#define RAT(operation) calculator_from_rational_status(operation)
#define NEXT(operation) do { status = RAT(operation); if (status != CALCULATOR_OK) goto cleanup; } while (0)
static CalculatorStatus compare(int *order, const BigRational *a, const BigRational *b)
{
    return RAT(bigrational_compare(order, a, b));
}
void numeric_root_brent_destroy(NumericRootBrent *work)
{
    bigrational_destroy(work->a); bigrational_destroy(work->b); bigrational_destroy(work->c); bigrational_destroy(work->d);
    bigrational_destroy(work->fa); bigrational_destroy(work->fb); bigrational_destroy(work->fc);
    for (size_t i = 0U; i < 12U; ++i) bigrational_destroy(work->scratch[i]);
    bigdecimal_destroy(work->decimal); bigdecimal_destroy(work->one);
}
CalculatorStatus numeric_root_brent_create(NumericRootBrent *work, int64_t digits)
{
    work->digits = digits; work->bisected = true;
    work->a = bigrational_create(); work->b = bigrational_create(); work->c = bigrational_create(); work->d = bigrational_create();
    work->fa = bigrational_create(); work->fb = bigrational_create(); work->fc = bigrational_create();
    work->decimal = bigdecimal_create(); work->one = bigdecimal_create();
    if (!work->a || !work->b || !work->c || !work->d || !work->fa || !work->fb || !work->fc || !work->decimal || !work->one)
        return CALCULATOR_OUT_OF_MEMORY;
    for (size_t i = 0U; i < 12U; ++i) {
        work->scratch[i] = bigrational_create(); if (!work->scratch[i]) return CALCULATOR_OUT_OF_MEMORY;
    }
    return calculator_from_decimal_status(bigdecimal_set_string(work->one, "1"));
}
static CalculatorStatus best(NumericRootBrent *work)
{
    CalculatorStatus status = CALCULATOR_OK; int order = 0;
    NEXT(bigrational_abs(work->scratch[0], work->fa));
    NEXT(bigrational_abs(work->scratch[1], work->fb));
    status = compare(&order, work->scratch[0], work->scratch[1]);
    if (status == CALCULATOR_OK && order < 0) {
        BigRational *temporary = work->a; work->a = work->b; work->b = temporary;
        temporary = work->fa; work->fa = work->fb; work->fb = temporary;
    }
cleanup: return status;
}
static CalculatorStatus ordinate(NumericRootBrent *work, BigRational *result, const BigDecimal *value)
{
    /* Exact polynomial samples can contain hundreds of incidental dyadic
     * digits. Keep the original sample for acceptance, but bound the size of
     * interpolation-only ordinates to the evaluator's working precision. */
    CalculatorStatus status = calculator_from_decimal_status(bigdecimal_div_significant(work->decimal, value,
        work->one, work->digits, BIGDECIMAL_ROUND_HALF_EVEN));
    if (status == CALCULATOR_OK) status = RAT(bigrational_from_bigdecimal(result, work->decimal));
    return status;
}
CalculatorStatus numeric_root_brent_reset(NumericRootBrent *work,
    const BigRational *lower, const BigRational *upper, const BigDecimal *left, const BigDecimal *right)
{
    CalculatorStatus status = CALCULATOR_OK;
    NEXT(bigrational_copy(work->a, lower)); NEXT(bigrational_copy(work->b, upper));
    status = ordinate(work, work->fa, left); if (status == CALCULATOR_OK) status = ordinate(work, work->fb, right);
    if (status != CALCULATOR_OK) goto cleanup;
    status = best(work); if (status != CALCULATOR_OK) goto cleanup;
    NEXT(bigrational_copy(work->c, work->a)); NEXT(bigrational_copy(work->fc, work->fa));
    NEXT(bigrational_copy(work->d, work->c)); work->bisected = true;
cleanup: return status;
}
/* Exact inverse-quadratic Lagrange interpolation. Repeated ordinates use a
 * secant instead; no near-equality threshold can erase a small exact value. */
static CalculatorStatus interpolate(NumericRootBrent *work, BigRational *x, bool *quadratic)
{
    CalculatorStatus status = CALCULATOR_OK; int ac = 0, bc = 0;
    status = compare(&ac, work->fa, work->fc);
    if (status == CALCULATOR_OK) status = compare(&bc, work->fb, work->fc);
    if (status != CALCULATOR_OK) return status;
    BigRational **t = work->scratch;
    *quadratic = ac && bc;
    if (!*quadratic) {
        NEXT(bigrational_sub(t[0], work->b, work->a));
        NEXT(bigrational_mul(t[0], t[0], work->fb));
        NEXT(bigrational_sub(t[1], work->fb, work->fa));
        NEXT(bigrational_div(t[0], t[0], t[1]));
        NEXT(bigrational_sub(x, work->b, t[0]));
    } else {
        const BigRational *positions[] = {work->a, work->b, work->c};
        const BigRational *values[] = {work->fa, work->fb, work->fc};
        for (size_t i = 0U; i < 3U; ++i) {
            size_t j = (i+1U)%3U, k = (i+2U)%3U;
            NEXT(bigrational_mul(t[0], values[j], values[k]));
            NEXT(bigrational_mul(t[0], t[0], positions[i]));
            NEXT(bigrational_sub(t[1], values[i], values[j]));
            NEXT(bigrational_sub(t[2], values[i], values[k]));
            NEXT(bigrational_mul(t[1], t[1], t[2]));
            NEXT(bigrational_div(t[0], t[0], t[1]));
            if (!i) { NEXT(bigrational_copy(x, t[0])); }
            else { NEXT(bigrational_add(x, x, t[0])); }
        }
    }
cleanup: return status;
}
CalculatorStatus numeric_root_brent_propose(NumericRootBrent *work, BigRational *x, ApplicationNumericRootStats *stats)
{
    CalculatorStatus status = CALCULATOR_OK; BigRational **t = work->scratch;
    bool quadratic = false, fallback = false, nudged = false; int order = 0, orientation = 0;
    status = compare(&order, work->bisected ? work->b : work->c, work->bisected ? work->c : work->d);
    if (status != CALCULATOR_OK) return status;
    if (!order) {
        /* Brent's half-displacement safeguard cannot accept any proposal when
         * that displacement is zero; avoid constructing doomed IQI fractions. */
        NEXT(bigrational_from_bigdecimal(t[11], work->one)); NEXT(bigrational_add(t[11], t[11], t[11]));
        NEXT(bigrational_add(x, work->a, work->b)); NEXT(bigrational_div(x, x, t[11]));
        ++stats->bisections; ++stats->rejections; work->bisected = true; goto cleanup;
    }
    status = interpolate(work, x, &quadratic); if (status != CALCULATOR_OK) return status;
    /* s must lie strictly between b and (3a+b)/4, and improve on half
     * the previous accepted displacement (or preceding bisection width). */
    NEXT(bigrational_add(t[0], work->a, work->a)); NEXT(bigrational_add(t[0], t[0], work->a));
    NEXT(bigrational_add(t[0], t[0], work->b));
    NEXT(bigrational_from_bigdecimal(t[10], work->one));
    NEXT(bigrational_add(t[11], t[10], t[10])); /* two */
    NEXT(bigrational_add(t[1], t[11], t[11])); NEXT(bigrational_div(t[0], t[0], t[1]));
    status = compare(&orientation, work->a, work->b); if (status != CALCULATOR_OK) goto cleanup;
    status = compare(&order, x, t[0]); if (status != CALCULATOR_OK) goto cleanup;
    fallback = orientation < 0 ? order <= 0 : order >= 0;
    status = compare(&order, x, work->b); if (status != CALCULATOR_OK) goto cleanup;
    fallback |= orientation < 0 ? order >= 0 : order <= 0;
    NEXT(bigrational_sub(t[0], x, work->b)); NEXT(bigrational_abs(t[0], t[0]));
    NEXT(bigrational_sub(t[1], work->bisected ? work->b : work->c, work->bisected ? work->c : work->d));
    NEXT(bigrational_abs(t[1], t[1])); NEXT(bigrational_div(t[1], t[1], t[11]));
    status = compare(&order, t[0], t[1]); if (status != CALCULATOR_OK) goto cleanup;
    fallback |= order >= 0;
    if (!fallback) {
        /* A minimum relative displacement prevents a rounded proposal from
         * repeatedly sampling b. Near zero use an absolute working-grid step.
         * This affects proposals only, never the acceptance tolerances. */
        char epsilon[32]; snprintf(epsilon, sizeof(epsilon), "1E-%lld", (long long)(work->digits-2));
        status = calculator_from_decimal_status(bigdecimal_set_string(work->decimal, epsilon));
        if (status != CALCULATOR_OK) goto cleanup;
        NEXT(bigrational_from_bigdecimal(t[2], work->decimal));
        NEXT(bigrational_abs(t[3], work->b));
        /* Reset reused scratch explicitly before using it as zero. */
        NEXT(bigrational_sub(t[4], work->b, work->b));
        status = compare(&order, t[3], t[4]); if (status != CALCULATOR_OK) goto cleanup;
        if (order) { NEXT(bigrational_mul(t[2], t[2], t[3])); }
        status = compare(&order, t[0], t[2]); if (status != CALCULATOR_OK) goto cleanup;
        if (order < 0) {
            status = RAT(orientation < 0 ? bigrational_sub(x, work->b, t[2]) : bigrational_add(x, work->b, t[2]));
            if (status != CALCULATOR_OK) goto cleanup;
            nudged = true;
        }
        /* Cap rational denominator growth by rounding the proposed coordinate
         * to working significant digits, then store that finite decimal exactly. */
        status = RAT(bigrational_to_bigdecimal(work->decimal, x, work->digits, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status == CALCULATOR_OK) status = calculator_from_decimal_status(bigdecimal_div_significant(work->decimal, work->decimal, work->one, work->digits, BIGDECIMAL_ROUND_HALF_EVEN));
        if (status != CALCULATOR_OK) goto cleanup;
        NEXT(bigrational_from_bigdecimal(x, work->decimal));
        status = compare(&order, x, work->a); if (status != CALCULATOR_OK) goto cleanup;
        fallback = orientation < 0 ? order <= 0 : order >= 0;
        status = compare(&order, x, work->b); if (status != CALCULATOR_OK) goto cleanup;
        fallback |= orientation < 0 ? order >= 0 : order <= 0;
    }
    if (fallback) {
        NEXT(bigrational_add(x, work->a, work->b)); NEXT(bigrational_div(x, x, t[11]));
        ++stats->bisections; ++stats->rejections;
    } else {
        ++stats->interpolations; if (quadratic) ++stats->quadratics; else ++stats->secants;
        if (nudged) ++stats->nudges;
    }
    work->bisected = fallback;
cleanup: return status;
}
CalculatorStatus numeric_root_brent_accept(NumericRootBrent *work, const BigRational *x, const BigDecimal *value)
{
    CalculatorStatus status = CALCULATOR_OK; int sign_a = 0, sign_x = 0;
    BigRational **t = work->scratch;
    NEXT(bigrational_copy(work->d, work->c)); NEXT(bigrational_copy(work->c, work->b)); NEXT(bigrational_copy(work->fc, work->fb));
    status = ordinate(work, t[0], value); if (status != CALCULATOR_OK) goto cleanup;
    NEXT(bigrational_sub(t[1], x, x));
    status = compare(&sign_a, work->fa, t[1]); if (status == CALCULATOR_OK) status = compare(&sign_x, t[0], t[1]);
    if (status != CALCULATOR_OK) goto cleanup;
    if ((sign_a < 0) != (sign_x < 0)) { NEXT(bigrational_copy(work->b, x)); NEXT(bigrational_copy(work->fb, t[0])); }
    else { NEXT(bigrational_copy(work->a, x)); NEXT(bigrational_copy(work->fa, t[0])); }
    status = best(work);
cleanup: return status;
}
#undef NEXT
#undef RAT
