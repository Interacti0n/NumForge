#ifndef NUMFORGE_NUMERIC_ROOT_BRENT_INTERNAL_H
#define NUMFORGE_NUMERIC_ROOT_BRENT_INTERNAL_H
#include "numeric_root_experiment.h"

/* a/b bracket opposite signs; b has the smaller residual. c/d retain the last
 * two best-point positions. All coordinates and stored finite values are exact
 * rationals. The hybrid forms bounded decimal weights and quantizes a local
 * proposal coordinate; acceptance always uses the original expression. */
typedef struct NumericRootBrent {
    BigRational *a, *b, *c, *d, *fa, *fb, *fc;
    BigRational *scratch[12];
    BigDecimal *decimal, *one;
    BigDecimal *weights[6];
    BigRational *checkpoint_width;
    int64_t digits;
    size_t steps, rejected_streak;
    bool bisected, hybrid, disabled;
} NumericRootBrent;
void numeric_root_brent_destroy(NumericRootBrent *work);
CalculatorStatus numeric_root_brent_create(NumericRootBrent *work, int64_t digits, bool hybrid);
CalculatorStatus numeric_root_brent_reset(NumericRootBrent *work,
    const BigRational *lower, const BigRational *upper,
    const BigDecimal *left, const BigDecimal *right);
CalculatorStatus numeric_root_brent_propose(NumericRootBrent *work,
    BigRational *x, ApplicationNumericRootStats *stats);
CalculatorStatus numeric_root_brent_accept(NumericRootBrent *work,
    const BigRational *x, const BigDecimal *value, ApplicationNumericRootStats *stats);
#endif
