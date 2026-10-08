#ifndef NUMFORGE_APPLICATION_CONVERSION_H
#define NUMFORGE_APPLICATION_CONVERSION_H
#include "calculator_internal.h"

#define APPLICATION_CONVERSION_CAPACITY 16U
typedef struct CalculatorSession CalculatorSession;
typedef struct ApplicationConversion {
    CalculatorValue value;
    char expression[CALCULATOR_MAX_INPUT_BYTES + 1U];
    char from[32], to[32];
    char *display;
    bool input_approximate, factor_approximate;
    uint64_t revision;
} ApplicationConversion;

void application_conversion_destroy(ApplicationConversion *entry);
CalculatorStatus application_conversion_compute(const CalculatorSession *session,
    const char *input, const char *from, const char *to, const CalculatorContext *context,
    ApplicationConversion *entry, CalculatorError *error, const char **code);
CalculatorStatus application_conversion_confirm(CalculatorSession *session,
    uint64_t revision, const char *input, const char *from, const char *to,
    const CalculatorContext *context, const ApplicationConversion **entry,
    CalculatorError *error, const char **code);
CalculatorStatus application_conversion_clear(CalculatorSession *session, uint64_t revision);
#endif
