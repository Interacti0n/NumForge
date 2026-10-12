#ifndef NUMFORGE_APPLICATION_EXPRESSION_H
#define NUMFORGE_APPLICATION_EXPRESSION_H

#include "session.h"

/* Internal C application API, shared by future graph/solver adapters. This is
 * not part of the installed numeric SDK. A handle owns one AST and immutable
 * snapshots of referenced session values; it never borrows the source session.
 * Handles are thread-confined and must not be evaluated concurrently. */
typedef struct ApplicationExpression ApplicationExpression;

#define APPLICATION_EXPRESSION_MAX_SAMPLES 1024U

typedef struct ApplicationExpressionSample
{
    CalculatorValue value;
    CalculatorError error; /* OK or a recoverable numeric-domain error. */
} ApplicationExpressionSample;

/* Parameter follows session variable naming rules and shadows the same session
 * name. Assignments and random calls are rejected. Optional session is read-only;
 * only referenced variables/ans are copied. On failure *result is unchanged. */
CalculatorStatus application_expression_prepare(ApplicationExpression **result,
    const char *input, const char *parameter, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
/* Infer the unique name absent from the source session, otherwise x if referenced
 * or a constant graph over x. Multiple free names return UNDEFINED_VARIABLE. */
CalculatorStatus application_expression_prepare_auto(ApplicationExpression **result,
    const char *input, const CalculatorSession *session,
    const CalculatorContext *context, CalculatorError *error);
const char *application_expression_parameter(const ApplicationExpression *expression);
void application_expression_destroy(ApplicationExpression *expression);

/* Inputs are borrowed typed values with a populated number marker. Results are
 * owned typed samples, never display text. Domain failures are individual empty
 * samples; resource/structural failures abort atomically and leave *result alone.
 * All points share one budget, inherited if the caller already owns a scope.
 * A graph/solver operation spanning preparation/multiple batches must install
 * its outer budget first. This API never modifies session state or the inputs. */
CalculatorStatus application_expression_sample(ApplicationExpression *expression,
    const CalculatorValue *arguments, size_t count,
    ApplicationExpressionSample **result, CalculatorError *error);
void application_expression_samples_destroy(ApplicationExpressionSample *samples,
    size_t count);

#endif
