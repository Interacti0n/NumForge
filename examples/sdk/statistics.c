#include <numforge/statistics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    BigInt *integer = bigint_create(); BigRational *zero = bigrational_create(), *one = bigrational_create();
    const BigRational *values[] = {zero, zero, one}; NumForgeStatisticsSummary *summary = NULL;
    BigRational *covariance = bigrational_create();
    NumForgeLinearRegression *fit = NULL; const BigRational *x[] = {zero, one, one};
    char *mean = NULL, *population = NULL, *sample = NULL, *paired = NULL, *slope = NULL, *intercept = NULL; int outcome = 1;
    if (!integer || !zero || !one || !covariance || bigint_set_string(integer, "1") != BIGINT_OK ||
        bigrational_from_bigint(one, integer) != BIGRATIONAL_OK ||
        numforge_statistics_describe(&summary, values, 3) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_statistics_summary_mean(summary), &mean) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_statistics_summary_variance_population(summary), &population) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_statistics_summary_variance_sample(summary), &sample) != BIGRATIONAL_OK ||
        numforge_statistics_covariance(covariance, values, values, 3, true) != BIGRATIONAL_OK ||
        bigrational_to_string(covariance, &paired) != BIGRATIONAL_OK ||
        numforge_statistics_linear_regression(&fit, x, values, 3) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_linear_regression_slope(fit), &slope) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_linear_regression_intercept(fit), &intercept) != BIGRATIONAL_OK ||
        strcmp(slope, "1/2") || strcmp(intercept, "0")) goto done;
    printf("count=%zu; mean=%s; population variance=%s; sample variance=%s (all exact)\n",
        numforge_statistics_summary_count(summary), mean, population, sample);
    printf("sample covariance with itself=%s (exact)\n", paired);
    printf("slope=%s; intercept=%s (exact least squares)\n", slope, intercept); outcome = 0;
done:
    free(mean); free(population); free(sample); free(paired); free(slope); free(intercept);
    numforge_linear_regression_destroy(fit); bigrational_destroy(covariance);
    bigint_destroy(integer); bigrational_destroy(zero); bigrational_destroy(one);
    numforge_statistics_summary_destroy(summary); return outcome;
}
