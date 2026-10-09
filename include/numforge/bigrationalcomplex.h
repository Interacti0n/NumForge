#ifndef NUMFORGE_BIGRATIONALCOMPLEX_H
#define NUMFORGE_BIGRATIONALCOMPLEX_H
#include <numforge/bigcomplex.h>
#include <numforge/bigrational.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Exact complex value with two reduced rational components. All mutations
 * are atomic, support input/output aliasing and inherit runtime budgets.
 * Status values use BigComplexStatus. No implicit decimal promotion. */
typedef struct BigRationalComplex BigRationalComplex;
BigRationalComplex *bigrationalcomplex_create(void);
void bigrationalcomplex_destroy(BigRationalComplex *value);
BigComplexStatus bigrationalcomplex_set_parts(BigRationalComplex *result,
    const BigRational *real, const BigRational *imaginary);
BigComplexStatus bigrationalcomplex_copy(BigRationalComplex *result, const BigRationalComplex *value);
BigComplexStatus bigrationalcomplex_conjugate(BigRationalComplex *result, const BigRationalComplex *value);
/* Exact squared modulus re*re + im*im; preserves result on failure. */
BigComplexStatus bigrationalcomplex_abs_squared(BigRational *result, const BigRationalComplex *value);
BigComplexStatus bigrationalcomplex_get_real(BigRational *result, const BigRationalComplex *value);
BigComplexStatus bigrationalcomplex_get_imaginary(BigRational *result, const BigRationalComplex *value);
BigComplexStatus bigrationalcomplex_add(BigRationalComplex *result, const BigRationalComplex *a, const BigRationalComplex *b);
BigComplexStatus bigrationalcomplex_sub(BigRationalComplex *result, const BigRationalComplex *a, const BigRationalComplex *b);
BigComplexStatus bigrationalcomplex_mul(BigRationalComplex *result, const BigRationalComplex *a, const BigRationalComplex *b);
BigComplexStatus bigrationalcomplex_div(BigRationalComplex *result, const BigRationalComplex *a, const BigRationalComplex *b);
/* 0^0 = 1. Negative powers remain exact, except zero is an error. */
BigComplexStatus bigrationalcomplex_pow_int(BigRationalComplex *result, const BigRationalComplex *value, int64_t exponent);
/* Explicit approximate projection. Finite quotients remain exact; recurring
 * components round independently. Source and output are preserved on failure. */
BigComplexStatus bigrationalcomplex_to_bigcomplex(BigComplex *result,
    const BigRationalComplex *value, int64_t digits, BigDecimalRoundingMode rounding);
/* Stored finite decimal components convert exactly, without inferring their
 * mathematical origin. */
BigComplexStatus bigrationalcomplex_from_bigcomplex(BigRationalComplex *result, const BigComplex *value);
/* Exact display, e.g. 1/3 + (2/3)*i. Owned string, free after success.
 * Complete byte limit excludes NUL; SIZE_MAX removes the extra bound. */
BigComplexStatus bigrationalcomplex_to_string(const BigRationalComplex *value, size_t max_output_bytes, char **result);
/* Cartesian keeps exact fractions (places/notation do not round them).
 * Polar forms use an explicit decimal projection at digits+12, then decimal
 * polar formatting. Approximate, not correctly rounded exact rational atan2.
 * All parameters are validated even for Cartesian output. */
BigComplexStatus bigrationalcomplex_format_form(const BigRationalComplex *value,
    BigComplexForm form, int64_t digits, int64_t places, BigDecimalRoundingMode rounding,
    BigDecimalFormatMode mode, size_t max_output_bytes, char **result);
#ifdef __cplusplus
}
#endif
#endif
