#include "src/core/bigint/bigint_internal.h"
#include "src/runtime/numforge_alloc.h"
#include <stdlib.h>

static unsigned counter_bits(uint64_t n)
{
    unsigned bits = 0U;
    do { ++bits; n >>= 1U; } while (n);
    return bits;
}

/* Sum integer bit lengths by power-of-two ranges. Each factor k is <
 * 2^bit_length(k); this is a checked upper bound, without floating point. */
static bool range_bits(uint64_t low, uint64_t high, size_t *total)
{
    *total = 0U;
    for (unsigned bits = 1U; bits <= 64U; ++bits)
    {
        uint64_t first = UINT64_C(1) << (bits - 1U);
        if (first > high) break;
        uint64_t last = bits == 64U ? UINT64_MAX : (UINT64_C(1) << bits) - 1U;
        if (first < low) first = low;
        if (last > high) last = high;
        if (first <= last)
        {
            size_t count, contribution;
            if ((uint64_t)(size_t)(last - first) != last - first ||
                bigint_size_add((size_t)(last - first), 1U, &count) != BIGINT_OK ||
                bigint_size_mul(count, bits, &contribution) != BIGINT_OK ||
                bigint_size_add(*total, contribution, total) != BIGINT_OK)
                return false;
        }
    }
    return true;
}

bool bigint_factorial_tree_plan(uint64_t n, size_t *total, size_t *largest)
{
    size_t full_bits, child_bits, limbs, child_limbs, bytes, extra, workspace;
    if (!total || !largest || (uint64_t)(size_t)n != n ||
        !range_bits(2U, n, &full_bits) || !range_bits(n / 2U + 1U, n, &child_bits) ||
        bigint_size_add(full_bits, 63U, &limbs) != BIGINT_OK ||
        bigint_size_add(child_bits, 63U, &child_limbs) != BIGINT_OK)
        return false;
    limbs = limbs / 64U + 2U;
    child_limbs /= 64U;
    if (limbs > BIGINT_MAX_LIMBS || bigint_size_mul(limbs, 8U, &bytes) != BIGINT_OK ||
        !bigint_multiplication_workspace_plan(child_limbs, &workspace, largest)) return false;
    if (*largest < bytes) *largest = bytes;
    /* Upper-half factors dominate either root child and every descendant.
     * Sum product buffers by tree level; 8*n + 32 KiB covers node/leaf
     * rounding, scalar output reserves and global-bit leaf overestimation.
     * Workspace includes every geometric reallocation request at each depth. */
    if (bigint_size_mul(bytes, counter_bits(n), total) != BIGINT_OK ||
        bigint_size_add(*total, workspace, total) != BIGINT_OK ||
        bigint_size_mul((size_t)n, 8U, &extra) != BIGINT_OK ||
        bigint_size_add(*total, extra, total) != BIGINT_OK ||
        bigint_size_add(*total, 32768U, total) != BIGINT_OK)
        return false;
    return true;
}

bool bigint_factorial_tree_fits(uint64_t n)
{
    size_t total, largest;
    return bigint_factorial_tree_plan(n, &total, &largest) &&
        numforge_budget_can_allocate_plan(total, largest);
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

static BigIntStatus factorial_tree(BigInt *out, uint64_t n, bool split)
{
    if (!out) return BIGINT_NULL_ARGUMENT;
    BigInt temporary = {0};
    BigIntMultiplicationWorkspace workspace = {0};
    workspace.split = split;
    BigIntStatus status = range_product(&temporary, 2U, n, counter_bits(n), &workspace);
    if (status == BIGINT_OK && !numforge_budget_check()) status = BIGINT_OUT_OF_MEMORY;
    if (status == BIGINT_OK) bigint_commit(out, &temporary);
    bigint_multiplication_workspace_free(&workspace);
    free(temporary.limbs);
    return status;
}

BigIntStatus bigint_factorial_tree(BigInt *out, uint64_t n)
{
    return factorial_tree(out, n, true);
}

BigIntStatus bigint_factorial_tree_unsplit(BigInt *out, uint64_t n)
{
    return factorial_tree(out, n, false);
}

BigIntStatus bigint_factorial_tree_split(BigInt *out, uint64_t n)
{
    return factorial_tree(out, n, true);
}
