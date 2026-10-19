#ifndef NUMFORGE_EXAMPLE_REFINEMENT_FUNCTION_H
#define NUMFORGE_EXAMPLE_REFINEMENT_FUNCTION_H
#include <numforge/numerical.h>
#include <string.h>

#define REFINEMENT_EXAMPLE_CAPACITY 257U
/* Caller-owned exact samples for f(x)=x*x. The callback compares the actual
 * midpoint value to the endpoint chord. It never evaluates in binary precision. */
typedef struct RefinementFunction {
    BigRational *points[REFINEMENT_EXAMPLE_CAPACITY];
    size_t count;
    BigRational *two, *tolerance, *chord, *value, *delta;
} RefinementFunction;
static void refinement_function_destroy(RefinementFunction *f)
{
    for (size_t i = 0; i < f->count; ++i) bigrational_destroy(f->points[i]);
    bigrational_destroy(f->two); bigrational_destroy(f->tolerance);
    bigrational_destroy(f->chord); bigrational_destroy(f->value); bigrational_destroy(f->delta);
    memset(f, 0, sizeof(*f));
}
static bool refinement_function_set(BigRational *value, BigDecimal *number, const char *text)
{
    return bigdecimal_set_string(number, text) == BIGDECIMAL_OK &&
        bigrational_from_bigdecimal(value, number) == BIGRATIONAL_OK;
}
static bool refinement_function_init(RefinementFunction *f, const char *lower,
    const char *upper, const char *tolerance)
{
    memset(f, 0, sizeof(*f)); f->count = 2;
    f->points[0] = bigrational_create(); f->points[1] = bigrational_create();
    f->two = bigrational_create(); f->tolerance = bigrational_create();
    f->chord = bigrational_create(); f->value = bigrational_create(); f->delta = bigrational_create();
    BigDecimal *number = bigdecimal_create();
    bool ok = f->points[0] && f->points[1] && f->two && f->tolerance && f->chord && f->value && f->delta && number &&
        refinement_function_set(f->points[0], number, lower) && refinement_function_set(f->points[1], number, upper) &&
        refinement_function_set(f->two, number, "2") && refinement_function_set(f->tolerance, number, tolerance);
    bigdecimal_destroy(number);
    if (!ok) refinement_function_destroy(f);
    return ok;
}
static NumForgeNumericalStatus refinement_function_probe(NumForgeRefinementDecision *decision,
    size_t left, size_t depth, void *user)
{
    RefinementFunction *f = user; (void)depth;
    if (f->count >= REFINEMENT_EXAMPLE_CAPACITY || left >= f->count - 1)
        return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    BigRational *middle = bigrational_create(); int comparison = 1;
    BigRationalStatus status = middle ? BIGRATIONAL_OK : BIGRATIONAL_OUT_OF_MEMORY;
    if (status == BIGRATIONAL_OK) status = bigrational_add(middle, f->points[left], f->points[left + 1]);
    if (status == BIGRATIONAL_OK) status = bigrational_div(middle, middle, f->two);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(f->value, middle, middle);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(f->chord, f->points[left], f->points[left]);
    if (status == BIGRATIONAL_OK) status = bigrational_mul(f->delta, f->points[left + 1], f->points[left + 1]);
    if (status == BIGRATIONAL_OK) status = bigrational_add(f->chord, f->chord, f->delta);
    if (status == BIGRATIONAL_OK) status = bigrational_div(f->chord, f->chord, f->two);
    /* The quadratic's midpoint lies below its chord, so this difference is >=0. */
    if (status == BIGRATIONAL_OK) status = bigrational_sub(f->delta, f->chord, f->value);
    if (status == BIGRATIONAL_OK) status = bigrational_compare(&comparison, f->delta, f->tolerance);
    if (status != BIGRATIONAL_OK) { bigrational_destroy(middle); return NUMFORGE_NUMERICAL_CALLBACK_FAILED; }
    memmove(f->points + left + 2, f->points + left + 1, (f->count - left - 1) * sizeof(*f->points));
    f->points[left + 1] = middle; ++f->count;
    *decision = comparison <= 0 ? NUMFORGE_REFINEMENT_ACCEPT : NUMFORGE_REFINEMENT_SPLIT;
    return NUMFORGE_NUMERICAL_OK;
}
#endif
