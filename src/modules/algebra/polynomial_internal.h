#ifndef NUMFORGE_POLYNOMIAL_INTERNAL_H
#define NUMFORGE_POLYNOMIAL_INTERNAL_H
#include <numforge/polynomial.h>

/* Result storage belongs exclusively to the algebra module. Not installed. */
struct NumForgePolynomialSolution {
    NumForgePolynomialKind kind;
    unsigned degree;
    size_t count;
    unsigned multiplicity[2];
    bool imaginary;
    BigRational *center, *radicand, *offset, *discriminant;
};
#endif
