#ifndef NUMFORGE_APPLICATION_EQUATION_H
#define NUMFORGE_APPLICATION_EQUATION_H

#include "src/engine/api/calculator_internal.h"

/* Legacy application adapter over the standalone public numeric solver.
 * Internal consumers may inspect the read-only descriptor layout for formatting;
 * public consumers use polynomial.h accessors instead. */
#include "src/modules/algebra/polynomial_internal.h"
typedef NumForgePolynomialSolution ApplicationEquationSolution;
typedef NumForgePolynomialKind ApplicationEquationKind;
#define APPLICATION_EQUATION_NONE NUMFORGE_POLYNOMIAL_NONE
#define APPLICATION_EQUATION_FINITE NUMFORGE_POLYNOMIAL_FINITE
#define APPLICATION_EQUATION_ALL NUMFORGE_POLYNOMIAL_ALL

/* Borrowed coefficients, including aliases; no expression parsing or sessions.
 * Zero leading coefficients reduce the degree. All work shares an inherited
 * application budget; context is validated even for constant equations.
 * On failure *result and inputs are unchanged. On success caller owns *result;
 * an existing destination is not destroyed automatically. */
CalculatorStatus application_equation_solve(ApplicationEquationSolution **result,
    const BigRational *a, const BigRational *b, const BigRational *c,
    const CalculatorContext *context, CalculatorError *error);
void application_equation_destroy(ApplicationEquationSolution *solution);

/* Stateless coefficient expressions in a,b,c order. Only exact real integer /
 * rational results are accepted; assignments, random calls, quantities and
 * approximate values are rejected. Exact evaluation is enabled independently
 * of the caller's significant_division display policy. No session is read.
 * failed_coefficient, if supplied, receives 0..2 for a coefficient error, or
 * SIZE_MAX for success / a non-coefficient error. error offsets are local to
 * that coefficient. Output ownership and atomic failure match solve above. */
CalculatorStatus application_equation_solve_expressions(ApplicationEquationSolution **result,
    const char *const inputs[3], const CalculatorContext *context,
    CalculatorError *error, size_t *failed_coefficient);

/* One equality of polynomials in x. Exact real constant subexpressions; +,-,*,
 * division by x-free constants, powers 0..2 and square postfix. Intermediate
 * polynomial degree must remain <=2. No variable denominators/cancellation of
 * domain holes, other variables, session mutation or random calls. Atomic output;
 * UTF-8 byte error offsets refer to the complete equation. */
CalculatorStatus application_equation_solve_text(ApplicationEquationSolution **result,
    const char *input, const CalculatorContext *context, CalculatorError *error);

/* Materialize an owned rational-component root; caller destroys *result.
 * Irrational radicals return NOT_IMPLEMENTED rather than silently approximating;
 * their exact center/radicand remain available in the solution above.
 * All failures preserve result. index must be less than count. */
CalculatorStatus application_equation_root_exact(BigRationalComplex **result,
    const ApplicationEquationSolution *solution, size_t index,
    const CalculatorContext *context, CalculatorError *error);

typedef struct ApplicationEquationRootText {
    char *exact;
    char *approximation;
} ApplicationEquationRootText;

/* Owned exact expression and approximate decimal preview (significant digits
 * from division_scale, half-even). Preview is not a certified rounding interval.
 * Stable real-root projection avoids subtracting nearly equal decimal values.
 * Atomic output; inherited budget; caller destroys with root_text_destroy. */
CalculatorStatus application_equation_format_root(ApplicationEquationRootText **result,
    const ApplicationEquationSolution *solution, size_t index,
    const CalculatorContext *context, CalculatorError *error);
void application_equation_root_text_destroy(ApplicationEquationRootText *text);

#endif
