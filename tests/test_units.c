#include <numforge/units.h>
#include "numforge_alloc.h"
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include "unit_angle_references.h"

void setUp(void) {}
void tearDown(void) { numforge_test_allocator_end(); }

static void assert_rational(const char *expected, const BigRational *value)
{
    char *text = NULL;
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_to_string(value, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
}
static void assert_decimal(const char *expected, const BigDecimal *value)
{
    char *text = NULL;
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_to_string(value, &text));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    free(text);
}
static void test_known_conversions(void)
{
    const char *cases[][4] = {
        { "1", "km", "m", "1000" },
        { "1", "cm2", "m2", "0.0001" },
        { "1", "L", "m3", "0.001" },
        { "1", "kg", "g", "1000" },
        { "2", "h", "s", "7200" },
        { "90", "km/h", "m/s", "25" },
        { "0", "degC", "K", "273.15" },
        { "32", "degF", "degC", "0" },
        { "100", "degC", "degF", "212" },
        { "-40", "degC", "degF", "-40" },
        { "-273.15", "degC", "K", "0" },
        { "18", "deltaF", "deltaC", "10" },
        { "-1", "m", "cm", "-100" },
        { "123456789012345678901234567890", "km", "m",
          "123456789012345678901234567890000" }
    };
    BigDecimal *value = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(value);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, cases[i][0]));
        TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, numforge_unit_convert_decimal(
            value, value, cases[i][1], cases[i][2], 30, BIGDECIMAL_ROUND_HALF_EVEN));
        assert_decimal(cases[i][3], value);
    }
    bigdecimal_destroy(value);
}
static void test_exact_and_rounding(void)
{
    BigInt *n = bigint_create(), *d = bigint_create();
    BigRational *value = bigrational_create();
    BigDecimal *decimal = bigdecimal_create(), *result = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(n); TEST_ASSERT_NOT_NULL(d); TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_NOT_NULL(decimal); TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(n, "1"));
    TEST_ASSERT_EQUAL(BIGINT_OK, bigint_set_string(d, "3"));
    TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_set_fraction(value, n, d));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, numforge_unit_convert_rational(value, value, "km", "m"));
    assert_rational("1000/3", value);
    const char *positive[] = { "0.27", "0.28", "0.27", "0.28", "0.28", "0.28" };
    const char *negative[] = { "-0.27", "-0.28", "-0.28", "-0.27", "-0.28", "-0.28" };
    for (int mode = 0; mode < 6; ++mode)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "1"));
        TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, numforge_unit_convert_decimal(result,
            decimal, "km/h", "m/s", 2, (BigDecimalRoundingMode)mode));
        assert_decimal(positive[mode], result);
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "-1"));
        TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, numforge_unit_convert_decimal(result,
            decimal, "km/h", "m/s", 2, (BigDecimalRoundingMode)mode));
        assert_decimal(negative[mode], result);
    }
    bigint_destroy(n); bigint_destroy(d); bigrational_destroy(value);
    bigdecimal_destroy(decimal); bigdecimal_destroy(result);
}
static void test_registry_and_errors(void)
{
    TEST_ASSERT_EQUAL_UINT(233, numforge_unit_count());
    TEST_ASSERT_NULL(numforge_unit_at(numforge_unit_count()));
    TEST_ASSERT_NULL(numforge_unit_find(NULL));
    TEST_ASSERT_NULL(numforge_unit_find("KM"));
    TEST_ASSERT_FALSE(numforge_units_compatible("m", "kg"));
    TEST_ASSERT_FALSE(numforge_units_compatible("degC", "deltaC"));
    TEST_ASSERT_FALSE(numforge_units_compatible(NULL, "m"));
    TEST_ASSERT_TRUE(numforge_units_compatible("m/s", "km/h"));
    BigDecimal *value = bigdecimal_create();
    BigRational *rational = bigrational_create();
    TEST_ASSERT_NOT_NULL(value); TEST_ASSERT_NOT_NULL(rational);
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "99"));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_INCOMPATIBLE_UNITS,
        numforge_unit_convert_decimal(value, value, "kg", "m", 10, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_UNKNOWN_UNIT,
        numforge_unit_convert_decimal(value, value, "bad", "m", 10, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_INVALID_ARGUMENT,
        numforge_unit_convert_decimal(value, value, "m", "cm", 0, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_INVALID_ARGUMENT,
        numforge_unit_convert_decimal(value, value, "m", "cm", 10, (BigDecimalRoundingMode)99));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_NULL_ARGUMENT,
        numforge_unit_convert_decimal(NULL, value, "m", "cm", 10, BIGDECIMAL_ROUND_HALF_EVEN));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_NULL_ARGUMENT,
        numforge_unit_convert_rational(rational, rational, NULL, "m"));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_UNKNOWN_UNIT,
        numforge_unit_convert_rational(rational, rational, "unknown", "m"));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_INCOMPATIBLE_UNITS,
        numforge_unit_convert_rational(rational, rational, "degC", "deltaC"));
    assert_decimal("99", value);
    for (size_t i = 0; i < numforge_unit_count(); ++i)
    {
        const NumForgeUnitInfo *a = numforge_unit_at(i);
        TEST_ASSERT_TRUE(a == numforge_unit_find(a->id));
        TEST_ASSERT_NOT_NULL(a->name_en);
        TEST_ASSERT_NOT_NULL(a->name_sk);
        TEST_ASSERT_NOT_NULL(a->source_url);
        TEST_ASSERT_TRUE(strlen(a->name_en) > 0 && strlen(a->name_sk) > 0);
        TEST_ASSERT_TRUE(strncmp(a->source_url, "https://", 8) == 0);
        for (size_t other = i + 1; other < numforge_unit_count(); ++other)
            TEST_ASSERT_TRUE(strcmp(a->id, numforge_unit_at(other)->id) != 0);
        for (size_t j = 0; j < numforge_unit_count(); ++j)
        {
            const NumForgeUnitInfo *b = numforge_unit_at(j);
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(rational, value));
            NumForgeUnitStatus status = numforge_unit_convert_rational(rational, rational, a->id, b->id);
            TEST_ASSERT_EQUAL(a->quantity != b->quantity ? NUMFORGE_UNIT_INCOMPATIBLE_UNITS :
                a->pi_power != b->pi_power ? NUMFORGE_UNIT_NON_RATIONAL_CONVERSION : NUMFORGE_UNIT_OK, status);
            if (status == NUMFORGE_UNIT_OK)
                TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK,
                    numforge_unit_convert_rational(rational, rational, b->id, a->id));
            assert_rational("99", rational);
        }
    }
    bigdecimal_destroy(value); bigrational_destroy(rational);
}
static void test_allocation_failures_preserve_outputs(void)
{
    BigDecimal *decimal = bigdecimal_create();
    BigRational *rational = bigrational_create();
    TEST_ASSERT_NOT_NULL(decimal); TEST_ASSERT_NOT_NULL(rational);
    for (int kind = 0; kind < 4; ++kind)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "32"));
        TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(rational, decimal));
        numforge_test_allocator_begin(0);
        NumForgeUnitStatus status = kind == 0 ?
            numforge_unit_convert_rational(rational, rational, "degF", "degC") :
            numforge_unit_convert_decimal(decimal, decimal, kind == 1 ? "degF" : kind == 2 ? "deg" : "rad",
            kind == 1 ? "degC" : kind == 2 ? "rad" : "deg", 20, BIGDECIMAL_ROUND_HALF_EVEN);
        size_t count = numforge_test_allocator_call_count();
        numforge_test_allocator_end();
        TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, status);
        TEST_ASSERT_TRUE(count > 0);
        for (size_t failure = 1; failure <= count; ++failure)
        {
            TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(decimal, "32"));
            TEST_ASSERT_EQUAL(BIGRATIONAL_OK, bigrational_from_bigdecimal(rational, decimal));
            numforge_test_allocator_begin(failure);
            status = kind == 0 ?
                numforge_unit_convert_rational(rational, rational, "degF", "degC") :
                numforge_unit_convert_decimal(decimal, decimal, kind == 1 ? "degF" : kind == 2 ? "deg" : "rad",
                    kind == 1 ? "degC" : kind == 2 ? "rad" : "deg", 20, BIGDECIMAL_ROUND_HALF_EVEN);
            bool injected = numforge_test_allocator_did_fail();
            numforge_test_allocator_end();
            TEST_ASSERT_TRUE(injected);
            TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OUT_OF_MEMORY, status);
            assert_rational("32", rational);
            assert_decimal("32", decimal);
        }
    }
    bigdecimal_destroy(decimal); bigrational_destroy(rational);
}
static void test_extended_reference_conversions(void)
{
    /* Values independently taken/derived from definitions cited in UNIT_CATALOG.md. */
    const char *cases[][4] = {
        {"1", "um", "m", "0.000001"},
        {"1", "mg", "kg", "0.000001"},
        {"1", "dm3", "L", "1"},
        {"1", "km2", "ha", "100"},
        {"1", "in", "cm", "2.54"},
        {"1", "ft", "m", "0.3048"},
        {"1", "yd", "m", "0.9144"},
        {"1", "mi", "km", "1.609344"},
        {"1", "acre", "m2", "4046.8564224"},
        {"1", "in3", "mL", "16.387064"},
        {"1", "lb", "kg", "0.45359237"},
        {"1", "oz", "g", "28.349523125"},
        {"1", "gal_US", "L", "3.785411784"},
        {"1", "gal_UK", "L", "4.54609"},
        {"1", "pt_US", "mL", "473.176473"},
        {"1", "pt_UK", "mL", "568.26125"},
        {"1", "floz_US", "mL", "29.5735295625"},
        {"1", "floz_UK", "mL", "28.4130625"},
        {"1", "mph", "km/h", "1.609344"},
        {"1", "kn", "km/h", "1.852"},
        {"1", "day", "s", "86400"},
        {"1", "B", "bit", "8"},
        {"1", "MB", "B", "1000000"},
        {"1", "MiB", "B", "1048576"},
        {"1", "YiB", "B", "1208925819614629174706176"},
        {"1", "Qbit", "bit", "1000000000000000000000000000000"},
        {"1", "turn", "deg", "360"},
        {"1", "deg", "arcmin", "60"},
        {"1", "deg", "arcsec", "3600"},
        {"100", "gon", "deg", "90"},
        {"0", "deg", "rad", "0"}
    };
    BigDecimal *value = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(value);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, cases[i][0]));
        TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, numforge_unit_convert_decimal(value,
            value, cases[i][1], cases[i][2], 30, BIGDECIMAL_ROUND_HALF_EVEN));
        assert_decimal(cases[i][3], value);
    }
    TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, "1"));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, numforge_unit_convert_decimal(value,
        value, "Qm3", "qm3", 30, BIGDECIMAL_ROUND_HALF_EVEN));
    char huge[182]; huge[0] = '1'; memset(huge + 1, '0', 180); huge[181] = '\0';
    assert_decimal(huge, value);
    TEST_ASSERT_EQUAL_STRING("mikrometer", numforge_unit_find("um")->name_sk);
    TEST_ASSERT_EQUAL_STRING("µm", numforge_unit_find("um")->symbol);
    TEST_ASSERT_NULL(numforge_unit_find("gal"));
    TEST_ASSERT_NULL(numforge_unit_find("KB"));
    TEST_ASSERT_NULL(numforge_unit_find("µm"));
    TEST_ASSERT_FALSE(numforge_units_compatible("bit", "m"));
    TEST_ASSERT_TRUE(numforge_units_compatible("deg", "rad"));
    TEST_ASSERT_FALSE(numforge_unit_conversion_is_exact("deg", "rad"));
    TEST_ASSERT_TRUE(numforge_unit_conversion_is_exact("deg", "turn"));
    TEST_ASSERT_FALSE(numforge_unit_conversion_is_exact("kg", "m"));
    TEST_ASSERT_EQUAL(NUMFORGE_UNIT_VALUE_TOO_LARGE, numforge_unit_convert_decimal(
        value, value, "deg", "rad", INT64_MAX, BIGDECIMAL_ROUND_HALF_EVEN));
    assert_decimal(huge, value);
    bigdecimal_destroy(value);
}
static void test_angle_references(void)
{
    BigDecimal *value = bigdecimal_create();
    TEST_ASSERT_NOT_NULL(value);
    for (size_t i = 0; i < sizeof(unit_angle_references) / sizeof(unit_angle_references[0]); ++i)
    {
        TEST_ASSERT_EQUAL(BIGDECIMAL_OK, bigdecimal_set_string(value, unit_angle_references[i].input));
        TEST_ASSERT_EQUAL(NUMFORGE_UNIT_OK, numforge_unit_convert_decimal(value, value,
            unit_angle_references[i].from, unit_angle_references[i].to,
            unit_angle_references[i].digits, (BigDecimalRoundingMode)unit_angle_references[i].rounding));
        assert_decimal(unit_angle_references[i].expected, value);
    }
    bigdecimal_destroy(value);
}
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_known_conversions);
    RUN_TEST(test_extended_reference_conversions);
    RUN_TEST(test_angle_references);
    RUN_TEST(test_exact_and_rounding);
    RUN_TEST(test_registry_and_errors);
    RUN_TEST(test_allocation_failures_preserve_outputs);
    return UNITY_END();
}
