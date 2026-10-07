#ifndef NUMFORGE_UNIT_WEB_H
#define NUMFORGE_UNIT_WEB_H

#include "session.h"
#include <numforge/units.h>

typedef struct NumForgeConversionOptions
{
    char from[32], to[32], client[33];
    int64_t precision;
    bool snapshot;
    CalculatorContext context;
} NumForgeConversionOptions;

/* Strict, order-independent query; rejects duplicate/unknown/malformed fields.
 * Body is expression text, like /api/evaluate. No action or revision mutation. */
bool numforge_web_parse_conversion_options(const char *target, NumForgeConversionOptions *options);

/* Owned UTF-8 JSON, free with free(). Read-only session: no preview/cache,
 * revision, random, variables, history or ans mutation, even on failure.
 * Metadata describes input/factor approximation, not display-rounding proof. */
CalculatorStatus numforge_web_convert(const CalculatorSession *session,
    const char *input, const NumForgeConversionOptions *options,
    char **response, CalculatorError *error, const char **code);
CalculatorStatus numforge_web_unit_catalog(char **response);

#endif
