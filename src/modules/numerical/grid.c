#include "src/modules/numerical/numerical_internal.h"
#include <numforge/runtime.h>
#include <stdio.h>

NumForgeNumericalStatus numforge_grid_visit(const BigRational *lower,
    const BigRational *upper, size_t count, NumForgeGridVisit visit, void *user)
{
    if (!lower || !upper || !visit) return NUMFORGE_NUMERICAL_NULL_ARGUMENT;
    if (count < 2) return NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    int comparison = 0;
    NumForgeNumericalStatus status = rational(bigrational_compare(&comparison, lower, upper));
    if (status == NUMFORGE_NUMERICAL_OK && comparison >= 0) status = NUMFORGE_NUMERICAL_INVALID_ARGUMENT;
    BigRational *step = NULL, *divisor = NULL, *cursor = NULL;
    BigDecimal *intervals = NULL;
    if (status == NUMFORGE_NUMERICAL_OK) {
        step = bigrational_create(); divisor = bigrational_create(); cursor = bigrational_create();
        intervals = bigdecimal_create();
        if (!step || !divisor || !cursor || !intervals) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
    }
    if (status == NUMFORGE_NUMERICAL_OK) {
        char text[3 * sizeof(size_t) + 1];
        (void)snprintf(text, sizeof(text), "%zu", count - 1);
        status = decimal(bigdecimal_set_string(intervals, text));
    }
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_from_bigdecimal(divisor, intervals));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_sub(step, upper, lower));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_div(step, step, divisor));
    if (status == NUMFORGE_NUMERICAL_OK) status = rational(bigrational_copy(cursor, lower));
    for (size_t i = 0; status == NUMFORGE_NUMERICAL_OK && i < count; ++i) {
        if (!numforge_budget_check()) { status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY; break; }
        status = visit(cursor, i, user);
        if (!numforge_budget_check()) status = NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
        if (status == NUMFORGE_NUMERICAL_OK && i + 1 < count)
            status = rational(bigrational_add(cursor, cursor, step));
    }
    bigrational_destroy(step); bigrational_destroy(divisor); bigrational_destroy(cursor);
    bigdecimal_destroy(intervals);
    return numforge_budget_check() ? status : NUMFORGE_NUMERICAL_OUT_OF_MEMORY;
}
