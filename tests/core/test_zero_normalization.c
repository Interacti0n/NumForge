#include "src/core/bigint/bigint_internal.h"
#include <stdlib.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

/* Build the reference from separately counted factors of two and five, not
 * from either normalization implementation. Include both pair parities and
 * the 256-word deadline checkpoint boundary. */
static void test_independent_factor_valuations(void)
{
    static const unsigned powers[] = {0U, 1U, 18U, 19U, 20U, 26U, 27U,
        28U, 53U, 54U, 55U, 63U, 64U, 65U, 127U, 128U, 129U};
    static const size_t sizes[] = {1U, 2U, 3U, 4U, 127U, 128U, 129U, 255U, 256U, 257U};
    for (size_t shape = 0U; shape < sizeof(sizes) / sizeof(sizes[0]); ++shape)
    {
        BigInt base = {0};
        base.size = base.capacity = sizes[shape];
        base.limbs = calloc(base.size, sizeof(uint64_t));
        TEST_ASSERT_NOT_NULL(base.limbs);
        for (size_t i = 0U; i < base.size; ++i)
            base.limbs[i] = shape & 1U ? UINT64_MAX : UINT64_C(0xaaaaaaaaaaaaaaaa);
        base.limbs[0] |= 1U;
        /* Ensure the base has neither factor. */
        unsigned remainder = 0U;
        for (size_t i = 0U; i < base.size; ++i)
            remainder = (remainder + (unsigned)(base.limbs[i] % 5U)) % 5U;
        if (!remainder)
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_add_uint64(&base, 2U));
        for (size_t a = 0U; a < sizeof(powers) / sizeof(powers[0]); ++a)
        for (size_t b = 0U; b < sizeof(powers) / sizeof(powers[0]); ++b)
        {
            unsigned count = powers[a] < powers[b] ? powers[a] : powers[b];
            BigInt input = {0}, expected = {0};
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_left(&input, &base, powers[a]));
            TEST_ASSERT_EQUAL(BIGINT_OK, bigint_shift_left(&expected, &base, powers[a] - count));
            for (unsigned j = 0U; j < powers[b]; ++j)
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_multiply_by_uint64(&input, 5U));
            for (unsigned j = count; j < powers[b]; ++j)
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_multiply_by_uint64(&expected, 5U));
            input.is_negative = expected.is_negative = (a + b) & 1U;
            for (unsigned block = 19U; block <= 27U; block += 8U)
            {
                BigInt actual = {0}; uint64_t removed = UINT64_MAX;
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_copy(&actual, &input));
                TEST_ASSERT_EQUAL(BIGINT_OK, bigint_strip_decimal_zeros_inverse(&actual, &removed, block));
                TEST_ASSERT_EQUAL_UINT64(count, removed);
                TEST_ASSERT_EQUAL_INT(0, bigint_compare(&actual, &expected));
                TEST_ASSERT_TRUE(!actual.size || actual.limbs[actual.size - 1U] != 0U);
                free(actual.limbs);
            }
            free(input.limbs); free(expected.limbs);
        }
        free(base.limbs);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_independent_factor_valuations);
    return UNITY_END();
}
