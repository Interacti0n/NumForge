#ifndef NUMFORGE_MATRIX_WEB_H
#define NUMFORGE_MATRIX_WEB_H
#include "src/engine/api/calculator_internal.h"
/* Stateless POST adapter. Body: whitespace-separated rows of A, blank LF line,
 * then optional B; CRLF accepted. At most 6000 bytes. Query has one exact
 * operation field. Owned JSON on success or a mathematical/validation error;
 * free with free(). Returns the operation status; transport supplies HTTP code.
 * Inherits/starts default calculator resource budgets through serialization. */
CalculatorStatus numforge_web_matrix_request(const char *target, const char *body,
    size_t length, char **response);
#endif
