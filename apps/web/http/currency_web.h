#ifndef NUMFORGE_CURRENCY_WEB_H
#define NUMFORGE_CURRENCY_WEB_H
#include "src/engine/api/calculator_internal.h"
/* Stateless arithmetic over caller-supplied reference rates. POST body is three
 * LF-separated finite dot decimals: amount, source-per-base, target-per-base.
 * Amount <=256 bytes (200 integer/100 fractional digits), each positive rate
 * <=25 bytes (12 integer/12 fractional digits). No expressions/exponents.
 * Exact multiplication followed by one half-even fixed-scale division.
 * Query is exactly /api/currency/convert?places=N, N=0..8. Rates are not fetched
 * or authenticated here. Owned JSON on success/error, free with free(). */
CalculatorStatus numforge_web_currency_request(const char *target,
    const char *body, size_t length, char **response);
#endif
