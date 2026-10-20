#include "src/modules/numerical/numerical_internal.h"
#include <numforge/runtime.h>
#include <stdlib.h>

struct NumForgeIntegral {
    BigRational *value, *error;
    NumForgeIntegralKind kind;
    size_t evaluations, refinements;
};
void numforge_integral_options_init(NumForgeIntegralOptions *options)
{
    if (options) {
        options->decimal_places = 34; options->max_refinements = 12;
        options->max_evaluations = 4097; options->absolute_tolerance = NULL;
    }
}
void numforge_integral_destroy(NumForgeIntegral *result)
{
    if (!result) return;
    bigrational_destroy(result->value); bigrational_destroy(result->error); free(result);
}
const BigRational *numforge_integral_value(const NumForgeIntegral *result)
{ return result && result->kind != NUMFORGE_INTEGRAL_DOMAIN_GAP ? result->value : NULL; }
const BigRational *numforge_integral_error(const NumForgeIntegral *result)
{ return result && result->kind != NUMFORGE_INTEGRAL_DOMAIN_GAP ? result->error : NULL; }
NumForgeIntegralKind numforge_integral_kind(const NumForgeIntegral *result)
{ return result ? result->kind : NUMFORGE_INTEGRAL_EVALUATION_LIMIT; }
size_t numforge_integral_evaluations(const NumForgeIntegral *result)
{ return result ? result->evaluations : 0; }
size_t numforge_integral_refinements(const NumForgeIntegral *result)
{ return result ? result->refinements : 0; }

static NumForgeNumericalStatus sample(BigRational *value, BigDecimal *decimal_value,
    NumForgeIntegral *output, NumForgeRootEvaluate evaluate, void *user,
    const BigRational *x, int64_t places)
{
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    NumForgeRootSample flags = {false, false};
    ++output->evaluations;
    NumForgeNumericalStatus status = evaluate(decimal_value, &flags, x, places, user);
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status != NUMFORGE_NUMERICAL_OK) return status;
    int sign = 0;
    if (flags.real) status = decimal(bigdecimal_sign(&sign, decimal_value));
    if (flags.exact_zero && (!flags.real || sign)) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (!flags.real) { output->kind = NUMFORGE_INTEGRAL_DOMAIN_GAP; return status; }
    return status == NUMFORGE_NUMERICAL_OK ? rational(bigrational_from_bigdecimal(value, decimal_value)) : status;
}

NumForgeNumericalStatus numforge_integrate(NumForgeIntegral **result,
    NumForgeRootEvaluate evaluate, void *user, const BigRational *lower,
    const BigRational *upper, const NumForgeIntegralOptions *options)
{
    if (!result || !evaluate || !lower || !upper) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    NumForgeIntegralOptions defaults; numforge_integral_options_init(&defaults);
    if (!options) options = &defaults;
    if (options->decimal_places < 1 || options->max_refinements < 2 || options->max_evaluations < 5)
        return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    int order = 0;
    NumForgeNumericalStatus status = rational(bigrational_compare(&order, lower, upper));
    if (status != NUMFORGE_NUMERICAL_OK) return status;
    if (order >= 0) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    enum { ZERO, TWO, THREE, FOUR, FIFTEEN, TOL, WIDTH, STEP, STRIDE, X, Y, SUM, TRAP, NEXT, SIMPSON, COUNT };
    BigRational *w[COUNT] = {0};
    BigDecimal *value = bigdecimal_create();
    NumForgeIntegral *out = numforge_calloc(1, sizeof(*out));
    if (!value || !out) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) {
        out->kind = NUMFORGE_INTEGRAL_REFINEMENT_LIMIT;
        out->value = bigrational_create(); out->error = bigrational_create();
        if (!out->value || !out->error) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < COUNT && status == NUMFORGE_NUMERICAL_OK; ++i) {
        w[i] = bigrational_create(); if (!w[i]) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    /* One exit keeps every allocation and callback failure transactional. */
#define R(call) do { status = rational(call); if (status != NUMFORGE_NUMERICAL_OK) goto done; } while (0)
#define D(call) do { status = decimal(call); if (status != NUMFORGE_NUMERICAL_OK) goto done; } while (0)
#define SAMPLE(at) do { status = sample(w[Y], value, out, evaluate, user, (at), options->decimal_places); \
    if (status != NUMFORGE_NUMERICAL_OK || out->kind == NUMFORGE_INTEGRAL_DOMAIN_GAP) goto done; } while (0)
    if (status != NUMFORGE_NUMERICAL_OK) goto done;
    const char *constants[] = {"2", "3", "4", "15", "1E-12"};
    for (size_t i = 0; i < sizeof(constants) / sizeof(constants[0]); ++i) {
        D(bigdecimal_set_string(value, constants[i])); R(bigrational_from_bigdecimal(w[TWO + i], value));
    }
    if (options->absolute_tolerance) R(bigrational_copy(w[TOL], options->absolute_tolerance));
    R(bigrational_compare(&order, w[TOL], w[ZERO]));
    if (order <= 0) { status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT; goto done; }
    R(bigrational_sub(w[WIDTH], upper, lower));
    R(bigrational_copy(w[STEP], w[WIDTH]));
    SAMPLE(lower); R(bigrational_copy(w[SUM], w[Y]));
    SAMPLE(upper); R(bigrational_add(w[SUM], w[SUM], w[Y]));
    R(bigrational_mul(w[TRAP], w[SUM], w[WIDTH]));
    R(bigrational_div(w[TRAP], w[TRAP], w[TWO]));
    size_t added = 1;
    for (size_t level = 0; level < options->max_refinements; ++level) {
        if (added > options->max_evaluations - out->evaluations) {
            out->kind = NUMFORGE_INTEGRAL_EVALUATION_LIMIT; break;
        }
        R(bigrational_copy(w[STRIDE], w[STEP]));
        R(bigrational_div(w[STEP], w[STEP], w[TWO]));
        R(bigrational_add(w[X], lower, w[STEP]));
        R(bigrational_copy(w[SUM], w[ZERO]));
        for (size_t j = 0; j < added; ++j) {
            SAMPLE(w[X]); R(bigrational_add(w[SUM], w[SUM], w[Y]));
            if (j + 1 < added) R(bigrational_add(w[X], w[X], w[STRIDE]));
        }
        R(bigrational_mul(w[NEXT], w[STEP], w[SUM]));
        R(bigrational_div(w[SUM], w[TRAP], w[TWO]));
        R(bigrational_add(w[NEXT], w[NEXT], w[SUM]));
        R(bigrational_mul(w[SIMPSON], w[FOUR], w[NEXT]));
        R(bigrational_sub(w[SIMPSON], w[SIMPSON], w[TRAP]));
        R(bigrational_div(w[SIMPSON], w[SIMPSON], w[THREE]));
        if (level) {
            R(bigrational_sub(out->error, w[SIMPSON], out->value));
            R(bigrational_abs(out->error, out->error));
            R(bigrational_div(out->error, out->error, w[FIFTEEN]));
        }
        R(bigrational_copy(out->value, w[SIMPSON]));
        R(bigrational_copy(w[TRAP], w[NEXT]));
        ++out->refinements;
        if (level) {
            R(bigrational_compare(&order, out->error, w[TOL]));
            if (order <= 0) { out->kind = NUMFORGE_INTEGRAL_ESTIMATED; break; }
        }
        /* A full next grid cannot fit even SIZE_MAX evaluations after overflow. */
        if (added > SIZE_MAX / 2) { out->kind = NUMFORGE_INTEGRAL_EVALUATION_LIMIT; break; }
        added *= 2;
    }
done:
    for (size_t i = 0; i < COUNT; ++i) bigrational_destroy(w[i]);
    bigdecimal_destroy(value);
    if (!numforge_budget_check()) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    if (status == NUMFORGE_NUMERICAL_OK) *result = out; else numforge_integral_destroy(out);
    return status;
#undef R
#undef D
#undef SAMPLE
}
