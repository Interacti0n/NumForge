#ifndef NUMFORGE_WEB_API_H
#define NUMFORGE_WEB_API_H

#include "calculator_internal.h"
#include "session.h"

CalculatorStatus numforge_web_evaluate_session(
    CalculatorSession *session,
    uint64_t revision,
    bool commit,
    const char *input,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    char **result,
    CalculatorError *error,
    bool *reused
);
CalculatorStatus numforge_web_evaluate_session_mode(
    CalculatorSession *session, uint64_t revision, bool commit, const char *input,
    int64_t output_scale, CalculatorAngleUnit angle_unit, CalculatorNotation notation,
    char **result, CalculatorError *error, bool *reused
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Internal adapter between the local HTTP server and the calculator pipeline.
    It accepts plain expression text and returns the owned decimal text produced
    by the same parser and BigDecimal evaluator used by the console program.

    Implementation: src/web/web_api.c
------------------------------------------------------------------------------------------------------------------------------
*/

#define NUMFORGE_WEB_MAX_EXPRESSION_LENGTH CALCULATOR_MAX_INPUT_BYTES

/* Optional decimal hint for a short, already-formatted exact fraction.
 * Returns owned text or NULL when the display is not a fraction or the hint
 * cannot be produced. The primary result remains valid in either case. */
char *numforge_web_fraction_approximation(const char *display);

/* One owned successful value per client. Zero-initialize; clear on eviction.
 * Revision prevents an older queued request from replacing newer work.
 * No thread safety: the loopback server currently handles requests serially. */
typedef struct NumForgeWebCache
{
    CalculatorValue value;
    char expression[NUMFORGE_WEB_MAX_EXPRESSION_LENGTH + 1U];
    uint64_t revision;
} NumForgeWebCache;

void numforge_web_cache_clear(NumForgeWebCache *cache);
CalculatorStatus numforge_web_evaluate_cached(
    NumForgeWebCache *cache,
    uint64_t revision,
    const char *input,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    char **result,
    CalculatorError *error,
    bool *reused
);
CalculatorStatus numforge_web_evaluate_cached_mode(
    NumForgeWebCache *cache, uint64_t revision, const char *input,
    int64_t output_scale, CalculatorAngleUnit angle_unit, CalculatorNotation notation,
    char **result, CalculatorError *error, bool *reused
);

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluate one expression with NumForge's default calculator policy and the
    default 10-decimal-place output format.

    On success, result receives an owned string released with free(). On
    failure, result receives NULL and error identifies the calculator failure.
------------------------------------------------------------------------------------------------------------------------------
*/

CalculatorStatus numforge_web_evaluate(
    const char *input,
    char **result,
    CalculatorError *error
);
CalculatorStatus numforge_web_evaluate_with_output_scale(
    const char *input,
    int64_t output_scale,
    char **result,
    CalculatorError *error
);
CalculatorStatus numforge_web_evaluate_with_options(
    const char *input,
    int64_t output_scale,
    CalculatorAngleUnit angle_unit,
    char **result,
    CalculatorError *error
);

#endif
