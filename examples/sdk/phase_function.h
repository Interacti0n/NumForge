#ifndef NUMFORGE_EXAMPLE_PHASE_FUNCTION_H
#define NUMFORGE_EXAMPLE_PHASE_FUNCTION_H
#include <numforge/phase.h>
#include <string.h>

/* A caller-supplied phase: base +/- factor*((x-lower)/span), optionally squared
 * before scaling. Exact rational intermediates remain independent of any engine. */
typedef struct PhaseFunction {
    BigRational *lower, *upper, *span, *base, *factor, *temporary, *value, *last;
    BigDecimal *period, *offset, *phase_lower, *phase_upper, *tolerance, *scratch;
    int64_t decimal_places;
    size_t calls;
    bool quadratic, decreasing;
} PhaseFunction;
static void phase_function_destroy(PhaseFunction *f)
{
    bigrational_destroy(f->lower); bigrational_destroy(f->upper); bigrational_destroy(f->span);
    bigrational_destroy(f->base); bigrational_destroy(f->factor); bigrational_destroy(f->temporary);
    bigrational_destroy(f->value); bigrational_destroy(f->last);
    bigdecimal_destroy(f->period); bigdecimal_destroy(f->offset); bigdecimal_destroy(f->phase_lower);
    bigdecimal_destroy(f->phase_upper); bigdecimal_destroy(f->tolerance); bigdecimal_destroy(f->scratch);
    memset(f, 0, sizeof(*f));
}
static bool phase_function_set(PhaseFunction *f, BigRational *value, const char *text)
{
    return bigdecimal_set_string(f->scratch, text) == BIGDECIMAL_OK &&
        bigrational_from_bigdecimal(value, f->scratch) == BIGRATIONAL_OK;
}
static bool phase_function_range(PhaseFunction *f, const char *lower, const char *span,
    const char *base, const char *factor)
{
    return phase_function_set(f, f->lower, lower) && phase_function_set(f, f->span, span) &&
        phase_function_set(f, f->base, base) && phase_function_set(f, f->factor, factor) &&
        bigrational_add(f->upper, f->lower, f->span) == BIGRATIONAL_OK &&
        bigrational_to_bigdecimal(f->phase_lower, f->base, f->decimal_places, BIGDECIMAL_ROUND_HALF_EVEN) == BIGRATIONAL_OK &&
        (f->decreasing ? bigrational_sub(f->value, f->base, f->factor) : bigrational_add(f->value, f->base, f->factor)) == BIGRATIONAL_OK &&
        bigrational_to_bigdecimal(f->phase_upper, f->value, f->decimal_places, BIGDECIMAL_ROUND_HALF_EVEN) == BIGRATIONAL_OK;
}
static bool phase_function_init(PhaseFunction *f)
{
    memset(f, 0, sizeof(*f)); f->decimal_places = 34;
    f->lower = bigrational_create(); f->upper = bigrational_create(); f->span = bigrational_create(); f->base = bigrational_create();
    f->factor = bigrational_create(); f->temporary = bigrational_create(); f->value = bigrational_create(); f->last = bigrational_create();
    f->period = bigdecimal_create(); f->offset = bigdecimal_create(); f->phase_lower = bigdecimal_create();
    f->phase_upper = bigdecimal_create(); f->tolerance = bigdecimal_create(); f->scratch = bigdecimal_create();
    bool ok = f->lower && f->upper && f->span && f->base && f->factor && f->temporary && f->value && f->last &&
        f->period && f->offset && f->phase_lower && f->phase_upper && f->tolerance && f->scratch &&
        bigdecimal_set_string(f->period, "1") == BIGDECIMAL_OK && bigdecimal_set_string(f->offset, "0.5") == BIGDECIMAL_OK &&
        bigdecimal_set_string(f->tolerance, "1E-6") == BIGDECIMAL_OK && phase_function_range(f, "0", "1", "0", "1");
    if (!ok) phase_function_destroy(f);
    return ok;
}
static NumForgeNumericalStatus phase_function_evaluate(BigDecimal *value, bool *usable, const BigRational *x, void *user)
{
    PhaseFunction *f = user; ++f->calls;
    BigRationalStatus status = bigrational_sub(f->temporary, x, f->lower);
    if (status == BIGRATIONAL_OK) status = bigrational_div(f->temporary, f->temporary, f->span);
    if (status == BIGRATIONAL_OK && f->quadratic) status = bigrational_mul(f->temporary, f->temporary, f->temporary);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(f->value, f->temporary, f->factor);
    if (status == BIGRATIONAL_OK) status = f->decreasing ? bigrational_sub(f->value, f->base, f->value) : bigrational_add(f->value, f->base, f->value);
    if (status == BIGRATIONAL_OK) status = bigrational_to_bigdecimal(value, f->value, f->decimal_places, BIGDECIMAL_ROUND_HALF_EVEN);
    if (status == BIGRATIONAL_OK) status = bigrational_copy(f->last, x);
    *usable = status == BIGRATIONAL_OK;
    switch (status) {
        case BIGRATIONAL_OK: return NUMFORGE_NUMERICAL_OK;
        case BIGRATIONAL_OUT_OF_MEMORY: return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        case BIGRATIONAL_SCALE_OVERFLOW: return NUMFORGE_NUMERICAL_SCALE_OVERFLOW;
        case BIGRATIONAL_VALUE_TOO_LARGE: return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
        default: return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    }
}
#endif
