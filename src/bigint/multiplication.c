#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
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

bool bigint_karatsuba_fits(size_t na, size_t nb, size_t threshold)
{
    size_t count, length, result_bytes, scratch_bytes;
    if (scratch_size(na > nb ? na : nb, threshold, &count) != BIGINT_OK ||
        bigint_size_add(na, nb, &length) != BIGINT_OK || length > BIGINT_MAX_LIMBS ||
        bigint_size_mul(length, sizeof(uint64_t), &result_bytes) != BIGINT_OK ||
        bigint_size_mul(count, sizeof(uint64_t), &scratch_bytes) != BIGINT_OK)
        return false;
    return numforge_budget_can_allocate_pair(result_bytes, scratch_bytes);
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
                             uint64_t *scratch, size_t available,
                             BigIntMultiplicationWorkspace *workspace, size_t depth)
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
    BigIntStatus status = multiply(out, a, al, b, bl, threshold, scratch, available, workspace, depth + 1U);
    if (status == BIGINT_OK)
        status = multiply(out + 2U * k, a + al, ah, b + bl, bh,
                          threshold, scratch, available, workspace, depth + 1U);
    if (status != BIGINT_OK) return status;
    size_t sa = (al > ah ? al : ah) + 1U, sb = (bl > bh ? bl : bh) + 1U;
    size_t cross = sa + sb, frame = 2U * cross;
    if (workspace)
    {
        if (depth >= 64U) return BIGINT_VALUE_TOO_LARGE;
        scratch = workspace->frames[depth];
        available = workspace->capacities[depth];
    }
    if (frame > available) return BIGINT_VALUE_TOO_LARGE;
    uint64_t *as = scratch, *bs = as + sa;
    uint64_t *middle = workspace && workspace->split ? workspace->cross_frames[depth] : bs + sb;
    size_t nas = 0U, nbs = 0U;
    status = sum_halves(as, a, na, k, &nas);
    if (status == BIGINT_OK) status = sum_halves(bs, b, nb, k, &nbs);
    if (status == BIGINT_OK)
        status = multiply(middle, as, nas, bs, nbs, threshold,
                          workspace ? NULL : scratch + frame, workspace ? 0U : available - frame,
                          workspace, depth + 1U);
    /* Only the initialized cross product is used, not spare arena capacity. */
    cross = status == BIGINT_OK ? nas + nbs : 0U;
    if (status == BIGINT_OK) status = subtract(middle, cross, out, trimmed(out, al + bl));
    if (status == BIGINT_OK)
        status = subtract(middle, cross, out + 2U * k, trimmed(out + 2U * k, ah + bh));
    if (status == BIGINT_OK)
        status = add_shifted(out, na + nb, middle, trimmed(middle, cross), k);
    return status;
}

BigIntStatus bigint_mul_karatsuba(BigInt *out, const BigInt *a, const BigInt *b,
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
                                  threshold, scratch, count, NULL, 0U);
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

static BigIntStatus workspace_frame(size_t n, size_t *next, size_t *capacity, size_t *bytes)
{
    size_t needed;
    *next = n - n / 2U + 1U;
    if (bigint_size_mul(*next, 4U, &needed) != BIGINT_OK) return BIGINT_VALUE_TOO_LARGE;
    *capacity = 32U;
    while (*capacity < needed)
        if (bigint_size_mul(*capacity, 2U, capacity) != BIGINT_OK) return BIGINT_VALUE_TOO_LARGE;
    return bigint_size_mul(*capacity, sizeof(uint64_t), bytes);
}

bool bigint_multiplication_workspace_plan(size_t n, size_t *total, size_t *largest)
{
    if (!total || !largest) return false;
    *total = *largest = 0U;
    for (size_t depth = 0U; n >= 32U; ++depth)
    {
        size_t next, capacity, bytes, requests;
        if (depth >= 64U || workspace_frame(n, &next, &capacity, &bytes) != BIGINT_OK ||
            bigint_size_mul(bytes, 2U, &requests) != BIGINT_OK ||
            bigint_size_add(*total, requests, total) != BIGINT_OK) return false;
        /* The factorial layout has equal sum and cross allocations. Their
         * combined geometric requests are unchanged; each request is half. */
        if (*largest < bytes / 2U) *largest = bytes / 2U;
        n = next;
    }
    return true;
}

static BigIntStatus prepare_workspace(BigIntMultiplicationWorkspace *workspace, size_t n)
{
    for (size_t depth = 0U; n >= 32U; ++depth)
    {
        size_t h, capacity, bytes;
        if (depth >= 64U || workspace_frame(n, &h, &capacity, &bytes) != BIGINT_OK)
            return BIGINT_VALUE_TOO_LARGE;
        if (workspace->capacities[depth] < capacity)
        {
            size_t allocation = workspace->split ? bytes / 2U : bytes;
            uint64_t *frame = numforge_realloc(workspace->frames[depth], allocation);
            if (!frame) return BIGINT_OUT_OF_MEMORY;
            workspace->frames[depth] = frame;
            if (workspace->split)
            {
                frame = numforge_realloc(workspace->cross_frames[depth], allocation);
                if (!frame) return BIGINT_OUT_OF_MEMORY;
                workspace->cross_frames[depth] = frame;
            }
            workspace->capacities[depth] = capacity;
        }
        n = h;
    }
    return BIGINT_OK;
}

void bigint_multiplication_workspace_free(BigIntMultiplicationWorkspace *workspace)
{
    if (!workspace) return;
    for (size_t i = 0U; i < 64U; ++i)
    {
        free(workspace->frames[i]);
        free(workspace->cross_frames[i]);
        workspace->frames[i] = NULL;
        workspace->cross_frames[i] = NULL;
        workspace->capacities[i] = 0U;
    }
}

BigIntStatus bigint_mul_workspace(BigInt *out, const BigInt *a, const BigInt *b,
                                  BigIntMultiplicationWorkspace *workspace)
{
    if (!out || !a || !b || !workspace) return BIGINT_NULL_ARGUMENT;
    size_t larger = a->size > b->size ? a->size : b->size;
    size_t smaller = a->size < b->size ? a->size : b->size;
    if (smaller < 128U || smaller <= larger / 2U) return bigint_mul(out, a, b);
    if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
    size_t length;
    if (bigint_size_add(a->size, b->size, &length) != BIGINT_OK || length > BIGINT_MAX_LIMBS)
        return BIGINT_VALUE_TOO_LARGE;
    BigInt temporary = {0};
    BigIntStatus status = prepare_workspace(workspace, larger);
    if (status != BIGINT_OK) return status;
    temporary.limbs = numforge_calloc(length, sizeof(uint64_t));
    if (!temporary.limbs) return BIGINT_OUT_OF_MEMORY;
    status = multiply(temporary.limbs, a->limbs, a->size, b->limbs, b->size,
                      32U, NULL, 0U, workspace, 0U);
    if (status == BIGINT_OK && !numforge_budget_check()) status = BIGINT_OUT_OF_MEMORY;
    if (status == BIGINT_OK)
    {
        temporary.size = temporary.capacity = length;
        temporary.is_negative = a->is_negative != b->is_negative;
        bigint_normalize(&temporary);
        bigint_commit(out, &temporary);
    }
    free(temporary.limbs);
    return status;
}
