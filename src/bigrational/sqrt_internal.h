#ifndef NUMFORGE_RATIONAL_SQRT_INTERNAL_H
#define NUMFORGE_RATIONAL_SQRT_INTERNAL_H
#include <numforge/bigrational.h>
/* Prove rational sqrt without approximation. On OK, *exact says whether result
 * was written. An irrational root preserves result; errors preserve both outputs.
 * Used by the expression evaluator and polynomial solver. */
BigRationalStatus numforge_rational_sqrt_exact(BigRational *result,
    const BigRational *operand, bool *exact);
#endif
