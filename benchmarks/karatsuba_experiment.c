#include "karatsuba_experiment.h"
#include "../src/internal/numforge_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Magnitudes only. All recursive destinations are disjoint from inputs.
 * z1=(a0+a1)(b0+b1)-z0-z2; z0 and z2 go directly into the final buffer.
 * Frame-local sums/cross product are arena slices, reused by child calls. */
static size_t trimmed(const uint64_t *a, size_t n)
{
    while (n && !a[n - 1U]) --n;
    return n;
}

static BigIntStatus scratch_size(size_t n, size_t threshold, size_t *out)
{
    size_t total = 0U;
    while (n >= threshold)
    {
        size_t h = n - n / 2U + 1U, frame;
        if (bigint_size_mul(h, 4U, &frame) != BIGINT_OK ||
            bigint_size_add(total, frame, &total) != BIGINT_OK)
            return BIGINT_VALUE_TOO_LARGE;
        n = h;
    }
    *out = total;
    return BIGINT_OK;
}

static BigIntStatus school(uint64_t *out, const uint64_t *a, size_t na,
                           const uint64_t *b, size_t nb)
{
    for (size_t i = 0U; i < nb; ++i)
    {
        if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
        uint64_t carry = 0U;
        for (size_t j = 0U; j < na; ++j)
        {
            if (j && !(j & 255U) && !numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
            uint64_t hi, lo;
            bigint_multiply_u64_u64(a[j], b[i], &hi, &lo);
            uint64_t sum = out[i + j] + lo, c1 = sum < lo;
            uint64_t next = sum + carry, c2 = next < sum;
            out[i + j] = next;
            carry = hi + c1 + c2;
        }
        out[i + na] = carry;
    }
    return BIGINT_OK;
}

static BigIntStatus sum_halves(uint64_t *out, const uint64_t *a, size_t na,
                               size_t k, size_t *length)
{
    size_t low = na < k ? na : k, high = na > k ? na - k : 0U;
    size_t n = low > high ? low : high;
    uint64_t carry = 0U;
    for (size_t i = 0U; i < n; ++i)
    {
        if (!(i & 255U) && !numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
        uint64_t x = i < low ? a[i] : 0U, y = i < high ? a[k + i] : 0U;
        uint64_t s = x + y, c1 = s < x;
        uint64_t t = s + carry, c2 = t < s;
        out[i] = t; carry = c1 | c2;
    }
    out[n] = carry;
    *length = n + (carry ? 1U : 0U);
    return BIGINT_OK;
}

static BigIntStatus subtract(uint64_t *a, size_t na, const uint64_t *b, size_t nb)
{
    uint64_t borrow = 0U;
    for (size_t i = 0U; i < na; ++i)
    {
        if (!(i & 255U) && !numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
        uint64_t x = a[i], y = i < nb ? b[i] : 0U;
        uint64_t s = x - y, c1 = x < y;
        uint64_t t = s - borrow, c2 = s < borrow;
        a[i] = t; borrow = c1 | c2;
    }
    return borrow || nb > na ? BIGINT_INVALID_ARGUMENT : BIGINT_OK;
}

static BigIntStatus add_shifted(uint64_t *a, size_t na, const uint64_t *b,
                                size_t nb, size_t offset)
{
    uint64_t carry = 0U;
    for (size_t i = 0U; i < nb || carry; ++i)
    {
        if (!(i & 255U) && !numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
        if (offset >= na || i >= na - offset) return BIGINT_INVALID_ARGUMENT;
        uint64_t x = a[offset + i], y = i < nb ? b[i] : 0U;
        uint64_t s = x + y, c1 = s < x;
        uint64_t t = s + carry, c2 = t < s;
        a[offset + i] = t; carry = c1 | c2;
    }
    return BIGINT_OK;
}

static BigIntStatus multiply(uint64_t *out, const uint64_t *a, size_t na,
                             const uint64_t *b, size_t nb, size_t threshold,
                             uint64_t *scratch, size_t available)
{
    if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
    memset(out, 0, (na + nb) * sizeof(*out));
    na = trimmed(a, na); nb = trimmed(b, nb);
    if (!na || !nb) return BIGINT_OK;
    size_t n = na > nb ? na : nb, smaller = na < nb ? na : nb;
    if (smaller < threshold || smaller <= n / 2U)
        return school(out, a, na, b, nb);
    size_t k = n / 2U, al = na < k ? na : k, bl = nb < k ? nb : k;
    size_t ah = na - al, bh = nb - bl;
    BigIntStatus status = multiply(out, a, al, b, bl, threshold, scratch, available);
    if (status == BIGINT_OK)
        status = multiply(out + 2U * k, a + al, ah, b + bl, bh,
                          threshold, scratch, available);
    if (status != BIGINT_OK) return status;
    size_t sa = (al > ah ? al : ah) + 1U, sb = (bl > bh ? bl : bh) + 1U;
    size_t cross = sa + sb, frame = 2U * cross;
    if (frame > available) return BIGINT_VALUE_TOO_LARGE;
    uint64_t *as = scratch, *bs = as + sa, *middle = bs + sb;
    size_t nas = 0U, nbs = 0U;
    status = sum_halves(as, a, na, k, &nas);
    if (status == BIGINT_OK) status = sum_halves(bs, b, nb, k, &nbs);
    if (status == BIGINT_OK)
        status = multiply(middle, as, nas, bs, nbs, threshold,
                          scratch + frame, available - frame);
    /* Only the initialized cross product is used, not spare arena capacity. */
    cross = status == BIGINT_OK ? nas + nbs : 0U;
    if (status == BIGINT_OK) status = subtract(middle, cross, out, trimmed(out, al + bl));
    if (status == BIGINT_OK)
        status = subtract(middle, cross, out + 2U * k, trimmed(out + 2U * k, ah + bh));
    if (status == BIGINT_OK)
        status = add_shifted(out, na + nb, middle, trimmed(middle, cross), k);
    return status;
}

BigIntStatus benchmark_karatsuba(BigInt *out, const BigInt *a, const BigInt *b,
                               size_t threshold)
{
    if (!out || !a || !b) return BIGINT_NULL_ARGUMENT;
    if (threshold < 8U || threshold > 1024U) return BIGINT_INVALID_ARGUMENT;
    if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
    if (!a->size || !b->size) return bigint_set_uint64(out, 0U);
    size_t length, count = 0U, bytes;
    if (bigint_size_add(a->size, b->size, &length) != BIGINT_OK || length > BIGINT_MAX_LIMBS)
        return BIGINT_VALUE_TOO_LARGE;
    size_t n = a->size > b->size ? a->size : b->size;
    size_t smaller = a->size < b->size ? a->size : b->size;
    if (smaller >= threshold && smaller > n / 2U)
    {
        BigIntStatus status = scratch_size(n, threshold, &count);
        if (status != BIGINT_OK) return status;
    }
    if (bigint_size_mul(count, sizeof(uint64_t), &bytes) != BIGINT_OK)
        return BIGINT_VALUE_TOO_LARGE;
    BigInt temporary = {0};
    temporary.limbs = numforge_calloc(length, sizeof(uint64_t));
    if (!temporary.limbs) return BIGINT_OUT_OF_MEMORY;
    uint64_t *scratch = count ? numforge_malloc(bytes) : NULL;
    if (count && !scratch) { free(temporary.limbs); return BIGINT_OUT_OF_MEMORY; }
    BigIntStatus status = multiply(temporary.limbs, a->limbs, a->size, b->limbs, b->size,
                                  threshold, scratch, count);
    if (status == BIGINT_OK && !numforge_budget_check()) status = BIGINT_OUT_OF_MEMORY;
    if (status == BIGINT_OK)
    {
        temporary.size = temporary.capacity = length;
        temporary.is_negative = a->is_negative != b->is_negative;
        bigint_normalize(&temporary);
        bigint_commit(out, &temporary);
    }
    free(scratch); free(temporary.limbs);
    return status;
}

#ifdef NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING
bool benchmark_karatsuba_fault_checks(void)
{
    size_t checks = 0U;
    if (!numforge_alloc_stats_track(true)) return false;
    bool good = true;
    /* Odd splits, carry growth, signs, each output alias, and same-input square. */
    for (unsigned mode = 0U; good && mode < 5U; ++mode)
    {
        BigInt fixture = {0}, other = {0}, a = {0}, b = {0};
        BigInt result = {0}, expected = {0}, original = {0};
        fixture.size = fixture.capacity = mode == 4U ? 513U : mode < 2U ? 65U : 257U;
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
                BigIntStatus status = benchmark_karatsuba(out, &a, mode == 3U ? &a : &b, threshold);
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
            BigIntStatus status = benchmark_karatsuba(out, &a, mode == 3U ? &a : &b, threshold);
            NumForgeBudgetFailure reason = numforge_budget_failure();
            numforge_budget_end(); ++checks;
            good = good && status == BIGINT_OUT_OF_MEMORY && reason == NUMFORGE_BUDGET_MEMORY &&
                bigint_compare(out, &original) == 0;
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
#endif
