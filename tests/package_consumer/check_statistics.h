#ifndef NUMFORGE_CONSUMER_CHECK_STATISTICS_H
#define NUMFORGE_CONSUMER_CHECK_STATISTICS_H
#include <numforge/statistics.h>
#include <stdlib.h>
#include <string.h>

static int statistics_field(const BigRational *value, const char *expected)
{
    char *text = NULL; int ok = 0;
    if (value && bigrational_to_string(value, &text) == BIGRATIONAL_OK && strcmp(text, expected) == 0) ok = 1;
    free(text); return ok;
}
static int statistics_api_checks(void)
{
    BigInt *integer = bigint_create(); BigRational *zero = bigrational_create(), *one = bigrational_create();
    const BigRational *values[3] = {zero, zero, one}; NumForgeStatisticsSummary *summary = NULL, *singleton = NULL; int outcome = 1;
    if (!integer || !zero || !one || bigint_set_string(integer, "1") != BIGINT_OK ||
        bigrational_from_bigint(one, integer) != BIGRATIONAL_OK || numforge_statistics_describe(&summary, values, 3) != BIGRATIONAL_OK ||
        numforge_statistics_summary_count(summary) != 3 || !statistics_field(numforge_statistics_summary_sum(summary), "1") ||
        !statistics_field(numforge_statistics_summary_minimum(summary), "0") || !statistics_field(numforge_statistics_summary_maximum(summary), "1") ||
        !statistics_field(numforge_statistics_summary_mean(summary), "1/3") ||
        !statistics_field(numforge_statistics_summary_variance_population(summary), "2/9") ||
        !statistics_field(numforge_statistics_summary_variance_sample(summary), "1/3") ||
        numforge_statistics_describe(&singleton, values + 2, 1) != BIGRATIONAL_OK ||
        numforge_statistics_summary_variance_sample(singleton) != NULL) goto done;
    if (numforge_statistics_covariance(one, values, values, 3, false) != BIGRATIONAL_OK ||
        !statistics_field(one, "2/9")) goto done;
    outcome = 0;
done:
    bigint_destroy(integer); bigrational_destroy(zero); bigrational_destroy(one);
    numforge_statistics_summary_destroy(summary); numforge_statistics_summary_destroy(singleton); return outcome;
}
#endif
