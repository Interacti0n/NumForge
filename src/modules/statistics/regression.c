#include <numforge/statistics.h>
#include <numforge/runtime.h>
#include <stdint.h>
#include <stdlib.h>

struct NumForgeLinearRegression {
    size_t count;
    BigRational *slope, *intercept;
};

void numforge_linear_regression_destroy(NumForgeLinearRegression *fit)
{
    if (fit == NULL) return;
    bigrational_destroy(fit->slope); bigrational_destroy(fit->intercept); free(fit);
}
size_t numforge_linear_regression_count(const NumForgeLinearRegression *fit)
{ return fit ? fit->count : 0; }
const BigRational *numforge_linear_regression_slope(const NumForgeLinearRegression *fit)
{ return fit ? fit->slope : NULL; }
const BigRational *numforge_linear_regression_intercept(const NumForgeLinearRegression *fit)
{ return fit ? fit->intercept : NULL; }

BigRationalStatus numforge_statistics_linear_regression(NumForgeLinearRegression **result,
    const BigRational *const *x, const BigRational *const *y, size_t count)
{
    if (result == NULL || x == NULL || y == NULL) return BIGRATIONAL_NULL_ARGUMENT;
    if (count < 2) return BIGRATIONAL_INVALID_ARGUMENT;
    if (count > SIZE_MAX / sizeof(*x)) return BIGRATIONAL_VALUE_TOO_LARGE;
    for (size_t i = 0; i < count; ++i) {
        if (x[i] == NULL || y[i] == NULL) return BIGRATIONAL_NULL_ARGUMENT;
        if (!numforge_budget_check()) return BIGRATIONAL_OUT_OF_MEMORY;
    }
    NumForgeLinearRegression *fit = numforge_calloc(1, sizeof(*fit));
    if (fit == NULL) return BIGRATIONAL_OUT_OF_MEMORY;
    fit->count = count;
    fit->slope = bigrational_create(); fit->intercept = bigrational_create();
    BigRationalStatus status = fit->slope && fit->intercept ? BIGRATIONAL_OK : BIGRATIONAL_OUT_OF_MEMORY;
    NumForgeStatisticsSummary *xs = NULL, *ys = NULL;
    if (status == BIGRATIONAL_OK) status = numforge_statistics_describe(&xs, x, count);
    int comparison = 0;
    /* intercept is still zero; a zero x variance means the coefficients are not unique. */
    if (status == BIGRATIONAL_OK) status = bigrational_compare(&comparison,
        numforge_statistics_summary_variance_population(xs), fit->intercept);
    if (status == BIGRATIONAL_OK && comparison == 0) status = BIGRATIONAL_INVALID_ARGUMENT;
    if (status == BIGRATIONAL_OK) status = numforge_statistics_describe(&ys, y, count);
    if (status == BIGRATIONAL_OK) status = numforge_statistics_covariance(fit->slope, x, y, count, false);
    if (status == BIGRATIONAL_OK) status = bigrational_div(fit->slope, fit->slope,
        numforge_statistics_summary_variance_population(xs));
    if (status == BIGRATIONAL_OK) status = bigrational_mul(fit->intercept, fit->slope,
        numforge_statistics_summary_mean(xs));
    if (status == BIGRATIONAL_OK) status = bigrational_sub(fit->intercept,
        numforge_statistics_summary_mean(ys), fit->intercept);
    if (!numforge_budget_check()) status = BIGRATIONAL_OUT_OF_MEMORY;
    numforge_statistics_summary_destroy(xs); numforge_statistics_summary_destroy(ys);
    if (status == BIGRATIONAL_OK) *result = fit;
    else numforge_linear_regression_destroy(fit);
    return status;
}
