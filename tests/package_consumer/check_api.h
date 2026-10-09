#ifndef NUMFORGE_CONSUMER_CHECK_API_H
#define NUMFORGE_CONSUMER_CHECK_API_H
#include <numforge/bigint.h>
#include <numforge/bigdecimal.h>
#include <numforge/bigrational.h>
#include <numforge/bigcomplex.h>
#include <numforge/bigrationalcomplex.h>
#include <numforge/runtime.h>
#include <numforge/units.h>
#include <stdlib.h>
#include <string.h>

/* Compiled as both C and C++; no source-tree or private-header access. */
static int public_api_checks(void)
{
    BigDecimal *a = bigdecimal_create(), *b = bigdecimal_create();
    BigInt *n = bigint_create(), *r = bigint_create();
    BigRational *fraction = bigrational_create();
    BigComplex *complex_value = bigcomplex_create();
    BigRationalComplex *exact_complex = bigrationalcomplex_create();
    char *text = NULL;
    const BigDecimal *values[2];
    bool integer = false;
    int sign = 0, result = 1;
    if (a == NULL || b == NULL || n == NULL || r == NULL || fraction == NULL || complex_value == NULL || exact_complex == NULL) goto cleanup;
    if (bigcomplex_set_strings(complex_value, "0", "1") != BIGCOMPLEX_OK ||
        bigcomplex_pow_int(complex_value, complex_value, 2, 10, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_to_string(complex_value, &text) != BIGCOMPLEX_OK || strcmp(text, "-1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_abs(a, complex_value, 10, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_format(complex_value, -1, BIGDECIMAL_ROUND_HALF_EVEN,
        BIGDECIMAL_FORMAT_PLAIN, 2U, &text) != BIGCOMPLEX_OK || strcmp(text, "-1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigrationalcomplex_from_bigcomplex(exact_complex, complex_value) != BIGCOMPLEX_OK ||
        bigrationalcomplex_pow_int(exact_complex, exact_complex, -1) != BIGCOMPLEX_OK ||
        bigrationalcomplex_to_string(exact_complex, 2U, &text) != BIGCOMPLEX_OK || strcmp(text, "-1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_format_form(complex_value, BIGCOMPLEX_FORM_EXPONENTIAL, 10, -1,
        BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, 80U, &text) != BIGCOMPLEX_OK ||
        strcmp(text, "e^(i*(π))") != 0) goto cleanup;
    free(text); text = NULL;
    values[0] = a; values[1] = b;
    if (bigint_set_string_base(n, "-fF", 16) != BIGINT_OK ||
        bigint_to_string_base(n, 2, false, &text) != BIGINT_OK ||
        strcmp(text, "-11111111") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigint_set_string(n, "3") != BIGINT_OK ||
        bigdecimal_set_string(a, "1.5") != BIGDECIMAL_OK ||
        bigdecimal_pow(a, a, n) != BIGDECIMAL_OK ||
        bigdecimal_root(a, a, 3, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_floor(b, a) != BIGDECIMAL_OK ||
        bigdecimal_ceil(b, a) != BIGDECIMAL_OK ||
        bigdecimal_trunc(b, a) != BIGDECIMAL_OK ||
        bigdecimal_round(b, a, 1) != BIGDECIMAL_OK ||
        bigdecimal_format(a, -1, BIGDECIMAL_ROUND_HALF_EVEN, &text) != BIGDECIMAL_OK ||
        strcmp(text, "1.5") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigdecimal_set_string(a, "1") != BIGDECIMAL_OK ||
        bigdecimal_set_string(b, "3") != BIGDECIMAL_OK ||
        bigdecimal_sum(a, values, 2) != BIGDECIMAL_OK ||
        bigdecimal_set_string(a, "1") != BIGDECIMAL_OK ||
        bigdecimal_product(b, values, 2) != BIGDECIMAL_OK ||
        bigdecimal_set_string(a, "1") != BIGDECIMAL_OK ||
        bigdecimal_set_string(b, "3") != BIGDECIMAL_OK ||
        bigdecimal_mean(a, values, 2, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_median(b, values, 2) != BIGDECIMAL_OK ||
        bigdecimal_geometric_mean(
            b, values, 2, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_harmonic_mean(
            b, values, 2, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_variance_population(
            b, values, 2, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_standard_deviation_population(
            b, values, 2, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_standard_deviation_sample(
            b, values, 2, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK || strcmp(text, "2") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigdecimal_set_constant(a, BIGDECIMAL_CONSTANT_PI) != BIGDECIMAL_OK ||
        bigdecimal_set_constant_significant(
            b, BIGDECIMAL_CONSTANT_PI, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_sin(b, b, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_atan(b, b, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_sign(&sign, a) != BIGDECIMAL_OK || sign != 1 ||
        bigdecimal_is_integer(&integer, a) != BIGDECIMAL_OK || integer ||
        bigdecimal_set_string(a, "0") != BIGDECIMAL_OK ||
        bigdecimal_exp(a, a, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigint_set_string(n, "-3") != BIGINT_OK ||
        bigdecimal_set_string(a, "2") != BIGDECIMAL_OK ||
        bigdecimal_pow_signed(a, a, n, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_sinh(b, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_cosh(b, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_tanh(b, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_asinh(b, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_set_string(a, "2") != BIGDECIMAL_OK ||
        bigdecimal_acosh(b, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_set_string(a, "0.5") != BIGDECIMAL_OK ||
        bigdecimal_atanh(b, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK) goto cleanup;
    if (
        bigdecimal_set_string(a, "81") != BIGDECIMAL_OK ||
        bigdecimal_sqrt(a, a, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_to_bigint(n, a) != BIGDECIMAL_OK ||
        bigint_isqrt(n, n) != BIGINT_OK ||
        bigdecimal_from_bigint(a, n) != BIGDECIMAL_OK ||
        bigdecimal_set_string(b, "6") != BIGDECIMAL_OK ||
        bigdecimal_min(a, a, b) != BIGDECIMAL_OK ||
        bigdecimal_max(b, a, b) != BIGDECIMAL_OK ||
        bigdecimal_div_significant(a, a, b, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_div_exact_or_significant(b, a, a, 20, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_cbrt(b, b, 34, BIGDECIMAL_ROUND_HALF_EVEN) != BIGDECIMAL_OK ||
        bigdecimal_to_string(b, &text) != BIGDECIMAL_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigdecimal_set_string(a, "12.5") != BIGDECIMAL_OK ||
        bigdecimal_format_mode(a, -1, BIGDECIMAL_ROUND_HALF_EVEN,
            BIGDECIMAL_FORMAT_MATHEMATICAL, 80U, &text) != BIGDECIMAL_OK ||
        strcmp(text, "1.25 × 10^1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigint_set_string(n, "5") != BIGINT_OK ||
        bigint_set_string(r, "2") != BIGINT_OK ||
        bigint_permutation(n, n, r) != BIGINT_OK ||
        (text = bigint_to_string(n)) == NULL || strcmp(text, "20") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigint_set_string(n, "5") != BIGINT_OK ||
        bigint_combination(n, n, r) != BIGINT_OK ||
        (text = bigint_to_string(n)) == NULL || strcmp(text, "10") != 0) goto cleanup;
    free(text); text = NULL;
    if (!numforge_budget_begin(5000, 1024, 512)) goto cleanup;
    if (!numforge_budget_check() || numforge_budget_failure() != NUMFORGE_BUDGET_OK)
    {
        numforge_budget_end();
        goto cleanup;
    }
    numforge_budget_end();
    if (bigint_set_string(n, "-6") != BIGINT_OK ||
        bigint_set_string(r, "8") != BIGINT_OK ||
        bigrational_set_fraction(fraction, n, r) != BIGRATIONAL_OK ||
        bigrational_to_string(fraction, &text) != BIGRATIONAL_OK ||
        strcmp(text, "-3/4") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigrational_to_bigdecimal(a, fraction, 10, BIGDECIMAL_ROUND_HALF_EVEN) != BIGRATIONAL_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK || strcmp(text, "-0.75") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigdecimal_set_string(a, "90") != BIGDECIMAL_OK ||
        !numforge_units_compatible("km/h", "m/s") ||
        numforge_unit_convert_decimal(a, a, "km/h", "m/s", 20,
            BIGDECIMAL_ROUND_HALF_EVEN) != NUMFORGE_UNIT_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK || strcmp(text, "25") != 0) goto cleanup;
    free(text); text = NULL;
    if (!numforge_unit_conversion_is_exact("MiB", "B") ||
        numforge_unit_conversion_is_exact("deg", "rad") ||
        numforge_unit_find("um") == NULL ||
        strcmp(numforge_unit_find("um")->name_sk, "mikrometer") != 0 ||
        bigdecimal_set_string(a, "180") != BIGDECIMAL_OK ||
        numforge_unit_convert_decimal(a, a, "deg", "rad", 10,
            BIGDECIMAL_ROUND_HALF_EVEN) != NUMFORGE_UNIT_OK ||
        bigdecimal_to_string(a, &text) != BIGDECIMAL_OK || strcmp(text, "3.141592654") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "0", "0") != BIGCOMPLEX_OK ||
        bigcomplex_exp(complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_to_string(complex_value, &text) != BIGCOMPLEX_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigrationalcomplex_conjugate(exact_complex, exact_complex) != BIGCOMPLEX_OK ||
        bigrationalcomplex_abs_squared(fraction, exact_complex) != BIGCOMPLEX_OK ||
        bigrational_to_string(fraction, &text) != BIGRATIONAL_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "3", "4") != BIGCOMPLEX_OK ||
        bigcomplex_sqrt(complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_to_string(complex_value, &text) != BIGCOMPLEX_OK || strcmp(text, "2 + i") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "1", "0") != BIGCOMPLEX_OK ||
        bigcomplex_ln(complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_to_string(complex_value, &text) != BIGCOMPLEX_OK || strcmp(text, "0") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "0", "1") != BIGCOMPLEX_OK ||
        bigcomplex_pow(complex_value, complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_format(complex_value, 10, BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, 80, &text) != BIGCOMPLEX_OK ||
        strcmp(text, "0.2078795764") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "0", "1") != BIGCOMPLEX_OK ||
        bigcomplex_log(complex_value, complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_to_string(complex_value, &text) != BIGCOMPLEX_OK || strcmp(text, "1") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "0", "1") != BIGCOMPLEX_OK ||
        bigcomplex_sin(complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_format(complex_value, 10, BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, 80, &text) != BIGCOMPLEX_OK ||
        strcmp(text, "1.1752011936*i") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "0", "1") != BIGCOMPLEX_OK ||
        bigcomplex_cos(complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_format(complex_value, 10, BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, 80, &text) != BIGCOMPLEX_OK ||
        strcmp(text, "1.5430806348") != 0) goto cleanup;
    free(text); text = NULL;
    if (bigcomplex_set_strings(complex_value, "0", "1") != BIGCOMPLEX_OK ||
        bigcomplex_tan(complex_value, complex_value, 12, BIGDECIMAL_ROUND_HALF_EVEN) != BIGCOMPLEX_OK ||
        bigcomplex_format(complex_value, 10, BIGDECIMAL_ROUND_HALF_EVEN, BIGDECIMAL_FORMAT_AUTO, 80, &text) != BIGCOMPLEX_OK ||
        strcmp(text, "0.761594156*i") != 0) goto cleanup;
    free(text); text = NULL;
    result = 0;
cleanup:
    bigrationalcomplex_destroy(exact_complex);
    bigcomplex_destroy(complex_value);
    free(text); bigint_destroy(n); bigint_destroy(r); bigdecimal_destroy(a); bigdecimal_destroy(b);
    bigrational_destroy(fraction);
    return result;
}
#endif
