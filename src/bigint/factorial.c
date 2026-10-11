#include "bigint_internal.h"
#include "../internal/numforge_alloc.h"
#include <stdlib.h>

static unsigned counter_bits(uint64_t n)
{
    unsigned bits = 0U;
    do { ++bits; n >>= 1U; } while (n);
    return bits;
}

bool bigint_factorial_tree_fits(uint64_t n)
{
    size_t bit_bound, limbs, bytes, total, extra, largest = 8U;
    unsigned bits = counter_bits(n);
    if ((uint64_t)(size_t)n != n ||
        bigint_size_mul((size_t)n, bits, &bit_bound) != BIGINT_OK ||
        bigint_size_add(bit_bound, 63U, &limbs) != BIGINT_OK)
        return false;
    limbs = limbs / 64U + 2U;
    if (limbs > BIGINT_MAX_LIMBS || bigint_size_mul(limbs, 8U, &bytes) != BIGINT_OK ||
        bigint_size_add(bytes, 64U, &extra) != BIGINT_OK)
        return false;
    while (largest < extra)
        if (bigint_size_mul(largest, 2U, &largest) != BIGINT_OK) return false;
    /* Every factor has <= bits bits. Product buffers at each level total
     * <= n*bits/8 plus rounding. There are < bits levels and < n/8 nodes.
     * Frame capacities are <= twice each 4*h bound. Geometric growth requests
     * < twice final capacities; sum(4*h) <= 4*max_input_limbs + 12*depth.
     * Eight result bounds cover frames; n*8 + 32 KiB covers product rounding
     * and the additive scratch terms over at most 64 recursive levels. */
    if (bigint_size_mul(bytes, (size_t)bits + 8U, &total) != BIGINT_OK ||
        bigint_size_mul((size_t)n, 8U, &extra) != BIGINT_OK ||
        bigint_size_add(total, extra, &total) != BIGINT_OK ||
        bigint_size_add(total, 32768U, &total) != BIGINT_OK)
        return false;
    return numforge_budget_can_allocate_plan(total, largest);
}

static BigIntStatus range_product(BigInt *out, uint64_t low, uint64_t high,
    unsigned bits, BigIntMultiplicationWorkspace *workspace)
{
    if (!numforge_budget_check()) return BIGINT_OUT_OF_MEMORY;
    if (low > high) return bigint_set_uint64(out, 1U);
    BigInt left = {0}, right = {0};
    BigIntStatus status;
    if (high - low < 32U)
    {
        size_t count = (size_t)(high - low + 1U);
        left.capacity = (count * bits + 63U) / 64U + 1U;
        left.limbs = numforge_calloc(left.capacity, sizeof(uint64_t));
        if (!left.limbs) return BIGINT_OUT_OF_MEMORY;
        left.size = 1U; left.limbs[0] = 1U;
        uint64_t group = 1U;
        status = BIGINT_OK;
        for (uint64_t factor = low;; ++factor)
        {
            if (!numforge_budget_check()) { status = BIGINT_OUT_OF_MEMORY; break; }
            if (group > UINT64_MAX / factor)
            {
                status = bigint_multiply_by_uint64(&left, group);
                if (status != BIGINT_OK) break;
                group = factor;
            }
            else group *= factor;
            if (factor == high) break;
        }
        if (status == BIGINT_OK) status = bigint_multiply_by_uint64(&left, group);
    }
    else
    {
        uint64_t middle = low + (high - low) / 2U;
        status = range_product(&left, low, middle, bits, workspace);
        if (status == BIGINT_OK) status = range_product(&right, middle + 1U, high, bits, workspace);
        if (status == BIGINT_OK) status = bigint_mul_workspace(out, &left, &right, workspace);
    }
    if (status == BIGINT_OK && high - low < 32U) bigint_commit(out, &left);
    free(left.limbs); free(right.limbs);
    return status;
}

BigIntStatus bigint_factorial_tree(BigInt *out, uint64_t n)
{
    if (!out) return BIGINT_NULL_ARGUMENT;
    BigInt temporary = {0};
    BigIntMultiplicationWorkspace workspace = {0};
    BigIntStatus status = range_product(&temporary, 2U, n, counter_bits(n), &workspace);
    if (status == BIGINT_OK && !numforge_budget_check()) status = BIGINT_OUT_OF_MEMORY;
    if (status == BIGINT_OK) bigint_commit(out, &temporary);
    bigint_multiplication_workspace_free(&workspace);
    free(temporary.limbs);
    return status;
}
