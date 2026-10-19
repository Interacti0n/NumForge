#ifndef NUMFORGE_EXAMPLE_NUMERICAL_FUNCTION_H
#define NUMFORGE_EXAMPLE_NUMERICAL_FUNCTION_H
#include <numforge/numerical.h>
/* Example callback for x*x-2, using exact rational intermediates. Scratch belongs
 * to this call's user state, so independent searches do not share mutable values. */
typedef struct NumericalFunction { BigRational *value, *two, *zero; } NumericalFunction;
static void numerical_function_destroy(NumericalFunction *f)
{ bigrational_destroy(f->value); bigrational_destroy(f->two); bigrational_destroy(f->zero); }
static int numerical_function_init(NumericalFunction *f)
{
    f->value = bigrational_create(); f->two = bigrational_create(); f->zero = bigrational_create();
    BigInt *two = bigint_create();
    int ok = f->value && f->two && f->zero && two && bigint_set_string(two, "2") == BIGINT_OK &&
        bigrational_from_bigint(f->two, two) == BIGRATIONAL_OK;
    bigint_destroy(two); return ok;
}
static NumForgeNumericalStatus numerical_function_evaluate(BigDecimal *value,
    NumForgeRootSample *sample, const BigRational *x, int64_t digits, void *user)
{
    NumericalFunction *f = (NumericalFunction *)user;
    BigRationalStatus status = bigrational_mul(f->value, x, x); int sign = 0;
    if (status == BIGRATIONAL_OK) status = bigrational_sub(f->value, f->value, f->two);
    if (status == BIGRATIONAL_OK) status = bigrational_compare(&sign, f->value, f->zero);
    if (status == BIGRATIONAL_OK) status = bigrational_to_bigdecimal(value, f->value, digits, BIGDECIMAL_ROUND_HALF_EVEN);
    sample->real = true; sample->exact_zero = sign == 0;
    switch (status) {
        case BIGRATIONAL_OK: return NUMFORGE_NUMERICAL_OK;
        case BIGRATIONAL_OUT_OF_MEMORY: return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        case BIGRATIONAL_VALUE_TOO_LARGE: return NUMFORGE_NUMERICAL_VALUE_TOO_LARGE;
        case BIGRATIONAL_SCALE_OVERFLOW: return NUMFORGE_NUMERICAL_SCALE_OVERFLOW;
        default: return NUMFORGE_NUMERICAL_CALLBACK_FAILED;
    }
}
static NumForgeNumericalStatus numerical_function_domain(bool *verified,
    const BigRational *lower, const BigRational *upper, void *user)
{
    (void)lower; (void)upper; (void)user;
    /* A real polynomial is continuous on every finite real interval. */
    *verified = true; return NUMFORGE_NUMERICAL_OK;
}
static inline NumForgeNumericalStatus numerical_function_scan_evaluate(BigDecimal *value,
    BigRational *proposal, NumForgeRootSample *sample, const BigRational *x, int64_t digits, void *user)
{
    NumForgeNumericalStatus status = numerical_function_evaluate(value, sample, x, digits, user);
    if (status == NUMFORGE_NUMERICAL_OK && proposal &&
        bigrational_copy(proposal, ((NumericalFunction *)user)->value) != BIGRATIONAL_OK)
        status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    return status;
}
#endif
