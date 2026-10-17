#ifndef NUMFORGE_APPLICATION_EQUATION_H
#define NUMFORGE_APPLICATION_EQUATION_H

#include "calculator_internal.h"

typedef enum ApplicationEquationKind {
    APPLICATION_EQUATION_NONE,
    APPLICATION_EQUATION_FINITE,
    APPLICATION_EQUATION_ALL
} ApplicationEquationKind;

/* Owned exact solution of a*x^2+b*x+c=0 for real rational coefficients.
 * Fields are read-only to callers. NONE/ALL have count=0 and no root fields.
 * FINITE roots are center +/- sqrt(radicand), or center +/- i*sqrt(radicand)
 * when imaginary=true. For two roots index 0 uses minus and index 1 plus.
 * A single root is center, with the indicated multiplicity. offset is the exact
 * nonnegative rational sqrt(radicand), or NULL if the radical is irrational.
 * discriminant is b*b-4*a*c for degree 2 only. No decimal rounding is performed.
 * This private application API is not installed with the numeric SDK. */
typedef struct ApplicationEquationSolution {
    ApplicationEquationKind kind;
    unsigned degree;
    size_t count;
    unsigned multiplicity[2];
    bool imaginary;
    BigRational *center;
    BigRational *radicand;
    BigRational *offset;
    BigRational *discriminant;
} ApplicationEquationSolution;

/* Borrowed coefficients, including aliases; no expression parsing or sessions.
 * Zero leading coefficients reduce the degree. All work shares an inherited
 * application budget; context is validated even for constant equations.
 * On failure *result and inputs are unchanged. On success caller owns *result;
 * an existing destination is not destroyed automatically. */
CalculatorStatus application_equation_solve(ApplicationEquationSolution **result,
    const BigRational *a, const BigRational *b, const BigRational *c,
    const CalculatorContext *context, CalculatorError *error);
void application_equation_destroy(ApplicationEquationSolution *solution);

/* Materialize an owned rational-component root; caller destroys *result.
 * Irrational radicals return NOT_IMPLEMENTED rather than silently approximating;
 * their exact center/radicand remain available in the solution above.
 * All failures preserve result. index must be less than count. */
CalculatorStatus application_equation_root_exact(BigRationalComplex **result,
    const ApplicationEquationSolution *solution, size_t index,
    const CalculatorContext *context, CalculatorError *error);

#endif
