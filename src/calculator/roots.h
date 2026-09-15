#ifndef NUMFORGE_CALCULATOR_ROOTS_H
#define NUMFORGE_CALCULATOR_ROOTS_H

#include "calculator_internal.h"

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal real-root operation. Degree is 1..10000; even roots require a
    non-negative value. Exact finite roots stay exact; other roots are rounded
    to working significant digits. Failure preserves result, including aliasing.
------------------------------------------------------------------------------------------------------------------------------
*/
#define CALCULATOR_MAX_ROOT_DEGREE 10000

BigDecimalStatus calculator_decimal_root(
    BigDecimal *result,
    const BigDecimal *value,
    uint32_t degree,
    int64_t digits,
    BigDecimalRoundingMode rounding
);

#endif
