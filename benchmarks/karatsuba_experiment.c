#include "karatsuba_experiment.h"
#include "../src/internal/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>

BigIntStatus benchmark_karatsuba(BigInt *out, const BigInt *a, const BigInt *b, size_t threshold)
{
    return bigint_mul_karatsuba(out, a, b, threshold);
}

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
static bool fault_checks(bool production)
{
    size_t checks = 0U;
    if (!numforge_alloc_stats_track(true)) return false;
    bool good = true;
    /* Odd splits, carry growth, signs, each output alias, and same-input square. */
    for (unsigned mode = 0U; good && mode < 5U; ++mode)
    {
        BigInt fixture = {0}, other = {0}, a = {0}, b = {0};
        BigInt result = {0}, expected = {0}, original = {0};
        fixture.size = fixture.capacity = mode == 4U ? 513U : mode < 2U ? 129U : 257U;
        other.size = other.capacity = mode == 4U ? 1U : fixture.size - 2U;
        fixture.limbs = numforge_malloc(fixture.size * sizeof(uint64_t));
        other.limbs = numforge_malloc(other.size * sizeof(uint64_t));
        good = fixture.limbs && other.limbs;
        if (good)
        {
            for (size_t i = 0U; i < fixture.size; ++i) fixture.limbs[i] = UINT64_MAX;
            for (size_t i = 0U; i < other.size; ++i)
                other.limbs[i] = i & 1U ? UINT64_MAX : UINT64_C(0xaaaaaaaaaaaaaaaa);
            fixture.is_negative = mode != 3U;
            good = bigint_mul(&expected, &fixture, mode == 3U ? &fixture : &other) == BIGINT_OK;
        }
        BigInt *out = mode == 1U || mode == 3U ? &a : mode == 2U ? &b : &result;
        size_t threshold = mode < 2U ? 16U : 64U;
        for (unsigned kind = 0U; good && kind < 3U; ++kind)
        {
            bool succeeded = false;
            for (size_t fail = kind == 0U ? 0U : 1U; good && fail < 20000U; ++fail)
            {
                good = bigint_copy(&a, &fixture) == BIGINT_OK &&
                    bigint_copy(&b, &other) == BIGINT_OK && bigint_set_uint64(&result, 77U) == BIGINT_OK &&
                    bigint_copy(&original, out) == BIGINT_OK;
                if (!good) break;
                size_t baseline = numforge_alloc_stats_live();
                bool owner = numforge_budget_begin(UINT64_MAX, SIZE_MAX, SIZE_MAX);
                if (!owner) { good = false; break; }
                numforge_test_allocator_begin(kind == 1U ? fail : 0U);
                if (kind == 2U) numforge_test_budget_expire_after(fail);
                BigIntStatus status = production ? bigint_mul(out, &a, mode == 3U ? &a : &b) :
                    benchmark_karatsuba(out, &a, mode == 3U ? &a : &b, threshold);
                bool allocation_failed = numforge_test_allocator_did_fail();
                NumForgeBudgetFailure reason = numforge_budget_failure();
                numforge_test_allocator_end(); numforge_budget_end();
                ++checks;
                if (status == BIGINT_OK)
                {
                    good = bigint_compare(out, &expected) == 0;
                    succeeded = true; break;
                }
                good = status == BIGINT_OUT_OF_MEMORY && bigint_compare(out, &original) == 0 &&
                    numforge_alloc_stats_live() == baseline &&
                    ((kind == 1U && allocation_failed) ||
                     (kind == 2U && reason == NUMFORGE_BUDGET_TIME));
            }
            if (!succeeded) good = false;
        }
        /* Both cumulative and single-allocation limits must preserve aliases. */
        for (unsigned limit = 0U; good && limit < 2U; ++limit)
        {
            good = bigint_copy(&a, &fixture) == BIGINT_OK && bigint_copy(&b, &other) == BIGINT_OK &&
                bigint_set_uint64(&result, 77U) == BIGINT_OK && bigint_copy(&original, out) == BIGINT_OK;
            bool owner = numforge_budget_begin(UINT64_MAX, limit ? SIZE_MAX : 1U,
                                              limit ? 1U : SIZE_MAX);
            if (!owner) { good = false; break; }
            BigIntStatus status = production ? bigint_mul(out, &a, mode == 3U ? &a : &b) :
                benchmark_karatsuba(out, &a, mode == 3U ? &a : &b, threshold);
            NumForgeBudgetFailure reason = numforge_budget_failure();
            numforge_budget_end(); ++checks;
            good = good && status == BIGINT_OUT_OF_MEMORY && reason == NUMFORGE_BUDGET_MEMORY &&
                bigint_compare(out, &original) == 0;
        }
        /* An optional arena that does not fit must not poison the scope.
         * Existing result-only budgets still succeed, including all aliases. */
        for (unsigned limit = 0U; production && good && limit < 3U; ++limit)
        {
            good = bigint_copy(&a, &fixture) == BIGINT_OK && bigint_copy(&b, &other) == BIGINT_OK &&
                bigint_set_uint64(&result, 77U) == BIGINT_OK;
            size_t bytes = (fixture.size + (mode == 3U ? fixture.size : other.size)) * sizeof(uint64_t);
            bool owner = numforge_budget_begin(UINT64_MAX, limit == 0U ? bytes : limit == 2U ? bytes + 1U : SIZE_MAX,
                                              limit == 1U ? bytes : SIZE_MAX);
            if (!owner) { good = false; break; }
            /* Test the remaining budget after an earlier charged allocation. */
            void *spent = limit == 2U ? numforge_malloc(1U) : NULL;
            numforge_test_allocator_begin(0U);
            BigIntStatus status = bigint_mul(out, &a, mode == 3U ? &a : &b);
            size_t calls = numforge_test_allocator_call_count();
            NumForgeBudgetFailure reason = numforge_budget_failure();
            numforge_test_allocator_end(); numforge_budget_end(); free(spent); ++checks;
            good = good && status == BIGINT_OK && reason == NUMFORGE_BUDGET_OK &&
                bigint_compare(out, &expected) == 0 &&
                calls == 1U;
        }
        free(fixture.limbs); free(other.limbs); free(a.limbs); free(b.limbs);
        free(result.limbs); free(expected.limbs); free(original.limbs);
    }
    if (numforge_alloc_stats_live() || !numforge_alloc_stats_complete()) good = false;
    if (!numforge_alloc_stats_track(false)) good = false;
    if (good) printf("Karatsuba allocation/deadline/alias checks passed: %zu\n", checks);
    else fputs("Karatsuba fault contract failed\n", stderr);
    return good;
}

bool benchmark_karatsuba_fault_checks(void) { return fault_checks(false); }
bool benchmark_multiplication_fault_checks(void) { return fault_checks(true); }
#endif
