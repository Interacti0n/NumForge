#ifndef NUMFORGE_POLYNOMIAL_INTERNAL_H
#define NUMFORGE_POLYNOMIAL_INTERNAL_H
#include <numforge/polynomial.h>

/* Shared read-only layout for legacy application formatting. Not installed. */
struct NumForgePolynomialSolution {
    NumForgePolynomialKind kind;
    unsigned degree;
    size_t count;
    unsigned multiplicity[2];
    bool imaginary;
    BigRational *center, *radicand, *offset, *discriminant;
};
#endif
