#include <numforge/statistics.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    BigInt *integer = bigint_create(); BigRational *zero = bigrational_create(), *one = bigrational_create();
    const BigRational *values[] = {zero, zero, one}; NumForgeStatisticsSummary *summary = NULL;
    char *mean = NULL, *population = NULL, *sample = NULL; int outcome = 1;
    if (!integer || !zero || !one || bigint_set_string(integer, "1") != BIGINT_OK ||
        bigrational_from_bigint(one, integer) != BIGRATIONAL_OK ||
        numforge_statistics_describe(&summary, values, 3) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_statistics_summary_mean(summary), &mean) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_statistics_summary_variance_population(summary), &population) != BIGRATIONAL_OK ||
        bigrational_to_string(numforge_statistics_summary_variance_sample(summary), &sample) != BIGRATIONAL_OK) goto done;
    printf("count=%zu; mean=%s; population variance=%s; sample variance=%s (all exact)\n",
        numforge_statistics_summary_count(summary), mean, population, sample); outcome = 0;
done:
    free(mean); free(population); free(sample); bigint_destroy(integer); bigrational_destroy(zero); bigrational_destroy(one);
    numforge_statistics_summary_destroy(summary); return outcome;
}
