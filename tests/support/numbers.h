#ifndef NUMFORGE_TEST_NUMBERS_H
#define NUMFORGE_TEST_NUMBERS_H

#include <numforge/bigdecimal.h>
#include <numforge/bigint.h>
#include <numforge/bigrational.h>
#include <unity.h>
#include <stdlib.h>

/* Fixture construction and canonical text checks only; references stay in tests. */
static inline BigInt *test_integer(const char *text)
{
    BigInt *value = bigint_create();
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(value, text));
    return value;
}

static inline BigDecimal *test_decimal(const char *text)
{
    BigDecimal *value = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, text));
    return value;
}

static inline BigRational *test_fraction(const char *numerator, const char *denominator)
{
    BigInt *top = test_integer(numerator);
    BigInt *bottom = test_integer(denominator);
    BigRational *value = bigrational_create();
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, top, bottom));
    bigint_destroy(top);
    bigint_destroy(bottom);
    return value;
}

static inline void test_assert_fraction(const char *expected, const BigRational *value)
{
    char *actual = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value, &actual));
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static inline void test_assert_integer(const BigInt *value, const char *expected)
{
    char *actual = bigint_to_string(value);
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

static inline void test_assert_decimal(const char *expected, const BigDecimal *value)
{
    char *actual = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &actual));
    TEST_ASSERT_NOT_NULL(actual);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
    free(actual);
}

#endif
