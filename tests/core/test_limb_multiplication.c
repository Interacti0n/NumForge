#include "src/core/bigint/limb_multiply.h"
#include <stddef.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

/* Independent base-256 convolution: no full-width multiply and no copy of
 * the production 32-bit decomposition. Each column fits in uint32_t. */
static void reference_product(uint64_t a, uint64_t b, uint64_t addend, uint64_t carry,
    uint64_t *expected_high, uint64_t *expected_low)
{
    uint32_t columns[16] = {0};
    for (unsigned i = 0; i < 8U; ++i)
        for (unsigned j = 0; j < 8U; ++j)
            columns[i+j] += (uint32_t)((a >> (8U*i)) & 255U) *
                (uint32_t)((b >> (8U*j)) & 255U);
    for (unsigned i = 0; i < 8U; ++i)
        columns[i] += (uint32_t)((addend >> (8U*i)) & 255U) +
            (uint32_t)((carry >> (8U*i)) & 255U);
    for (unsigned i = 0; i < 15U; ++i) {
        columns[i+1U] += columns[i] >> 8U;
        columns[i] &= 255U;
    }
    TEST_ASSERT_LESS_THAN_UINT32(256U, columns[15]);
    *expected_low = *expected_high = 0;
    for (unsigned i = 0; i < 8U; ++i) {
        *expected_low |= (uint64_t)columns[i] << (8U*i);
        *expected_high |= (uint64_t)columns[i+8U] << (8U*i);
    }
}

static void check_product(uint64_t a, uint64_t b)
{
    uint64_t expected_low, expected_high, low = 0, high = 0;
    reference_product(a, b, 0U, 0U, &expected_high, &expected_low);
    bigint_multiply_u64_u64(a, b, &high, &low);
    TEST_ASSERT_EQUAL_UINT64(expected_low, low);
    TEST_ASSERT_EQUAL_UINT64(expected_high, high);
}

static void check_accumulated_product(uint64_t a, uint64_t b, uint64_t addend, uint64_t carry)
{
    uint64_t expected_low, expected_high, low = 0;
    reference_product(a, b, addend, carry, &expected_high, &expected_low);
    uint64_t high = bigint_multiply_accumulate_u64(a, b, addend, carry, &low);
    TEST_ASSERT_EQUAL_UINT64(expected_low, low);
    TEST_ASSERT_EQUAL_UINT64(expected_high, high);
}

static void test_boundary_products(void)
{
    const uint64_t values[] = {0U, 1U, 2U, 3U, 255U, 256U, 65535U, 65536U,
        UINT64_C(0xffffffff), UINT64_C(0x100000000), UINT64_C(0x100000001),
        UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000),
        UINT64_C(0x8000000000000001), UINT64_C(0xfffffffffffffffe), UINT64_MAX,
        UINT64_C(0xaaaaaaaaaaaaaaaa), UINT64_C(0x5555555555555555)};
    for (size_t i = 0; i < sizeof(values)/sizeof(values[0]); ++i)
        for (size_t j = 0; j < sizeof(values)/sizeof(values[0]); ++j)
            check_product(values[i], values[j]);
    for (unsigned i = 0; i < 64U; ++i)
        for (unsigned j = 0; j < 64U; ++j) {
            uint64_t a = UINT64_C(1) << i, b = UINT64_C(1) << j;
            check_product(a, b);
            check_product(a-1U, b-1U);
            check_product(~a, ~b);
        }
}

static uint64_t next_word(uint64_t *state)
{
    *state ^= *state << 13U;
    *state ^= *state >> 7U;
    *state ^= *state << 17U;
    return *state;
}

static void test_random_products(void)
{
    uint64_t state = UINT64_C(0x756c696d62323026);
    for (size_t i = 0; i < 32768U; ++i) {
        uint64_t a = next_word(&state), b = next_word(&state);
        check_product(a, b);
    }
}

static void test_accumulated_products(void)
{
    const uint64_t values[] = {0U, 1U, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000), UINT64_MAX-1U, UINT64_MAX};
    for (size_t i = 0; i < sizeof(values)/sizeof(values[0]); ++i)
        for (size_t j = 0; j < sizeof(values)/sizeof(values[0]); ++j)
            for (size_t k = 0; k < sizeof(values)/sizeof(values[0]); ++k)
                for (size_t l = 0; l < sizeof(values)/sizeof(values[0]); ++l)
                    check_accumulated_product(values[i], values[j], values[k], values[l]);
    uint64_t state = UINT64_C(0x616363756d323032);
    for (size_t i = 0; i < 32768U; ++i) {
        uint64_t a = next_word(&state), b = next_word(&state);
        uint64_t addend = next_word(&state), carry = next_word(&state);
        check_accumulated_product(a, b, addend, carry);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boundary_products);
    RUN_TEST(test_random_products);
    RUN_TEST(test_accumulated_products);
    return UNITY_END();
}
