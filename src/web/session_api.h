#ifndef NUMFORGE_SESSION_API_H
#define NUMFORGE_SESSION_API_H
#include "client_store.h"
/* Returns an owned bounded JSON response. No database or public C ABI.
 * Recognized routes use strict query schemas and share application ownership. */
CalculatorStatus numforge_web_session_request(ApplicationClientStore *store,
    const char *method, const char *target, const char *body, char **response);
bool numforge_web_is_session_target(const char *target);
#endif
