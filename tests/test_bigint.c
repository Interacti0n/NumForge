#include <stdlib.h>
#include "unity.h"
#include "mysciencecalc/bigint.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_bigint_add(void)
{
    BigInt *a = bigint_create();
    BigInt *b = bigint_create();
    BigInt *result = bigint_create();

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_NULL(result);

    TEST_ASSERT_TRUE(bigint_set_string(a, "15"));
    TEST_ASSERT_TRUE(bigint_set_string(b, "11"));

    TEST_ASSERT_TRUE(bigint_add(result, a, b));

    char *string = bigint_to_string(result);

    TEST_ASSERT_NOT_NULL(string);
    TEST_ASSERT_EQUAL_STRING("26", string);

    free(string);

    bigint_destroy(a);
    bigint_destroy(b);
    bigint_destroy(result);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_bigint_add);

    return UNITY_END();
}