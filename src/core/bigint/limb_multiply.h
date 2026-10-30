#ifndef NUMFORGE_LIMB_MULTIPLY_H
#define NUMFORGE_LIMB_MULTIPLY_H

#include <stdint.h>
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

#if defined(NUMFORGE_TEST_PORTABLE_MULTIPLICATION) && !defined(NUMFORGE_ENABLE_ALLOC_FAILURE_TESTING)
#error "NUMFORGE_TEST_PORTABLE_MULTIPLICATION is restricted to test builds"
#endif

/* Exact full-width unsigned product. Keep the primitive visible to each
 * kernel so its inner loop need not call through an external function.
 * Other C17 targets retain the four-product, 32-bit decomposition. */
static inline void bigint_multiply_u64_u64(uint64_t a, uint64_t b,
    uint64_t *high, uint64_t *low)
{
#if defined(__SIZEOF_INT128__) && !defined(NUMFORGE_TEST_PORTABLE_MULTIPLICATION)
    __uint128_t product = (__uint128_t)a * (__uint128_t)b;
    *low = (uint64_t)product;
    *high = (uint64_t)(product >> 64U);
#elif defined(_MSC_VER) && defined(_M_X64) && !defined(NUMFORGE_TEST_PORTABLE_MULTIPLICATION)
    *low = _umul128(a, b, high);
#else
    uint64_t a0 = (uint32_t)a, a1 = a >> 32U;
    uint64_t b0 = (uint32_t)b, b1 = b >> 32U;
    uint64_t p00 = a0 * b0, p01 = a0 * b1;
    uint64_t p10 = a1 * b0, p11 = a1 * b1;
    uint64_t middle = (p00 >> 32U) + (uint32_t)p01 + (uint32_t)p10;
    *low = (p00 & UINT64_C(0xffffffff)) | (middle << 32U);
    *high = p11 + (p01 >> 32U) + (p10 >> 32U) + (middle >> 32U);
#endif
}

#endif
