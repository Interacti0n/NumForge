#ifndef NUMFORGE_CALCULATOR_INTERNAL_H
#define NUMFORGE_CALCULATOR_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <numforge/bigdecimal.h>
#include <numforge/bigrational.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared status model for the calculator application pipeline. These types
    belong to the private client layer over the public numeric library.

    Implementation: src/calculator/calculator.c and value.c
------------------------------------------------------------------------------------------------------------------------------
*/

typedef enum CalculatorStatus
{
    CALCULATOR_OK = 0,
    CALCULATOR_NULL_ARGUMENT,
    CALCULATOR_OUT_OF_MEMORY,
    CALCULATOR_INVALID_ARGUMENT,
    CALCULATOR_INVALID_TOKEN,
    CALCULATOR_SYNTAX_ERROR,
    CALCULATOR_DIVISION_BY_ZERO,
    CALCULATOR_VALUE_TOO_LARGE,
    CALCULATOR_SCALE_OVERFLOW,
    CALCULATOR_TIME_LIMIT,
    CALCULATOR_NOT_IMPLEMENTED,
    CALCULATOR_ARGUMENT_COUNT,
    CALCULATOR_UNDEFINED_ANSWER,
    CALCULATOR_STALE_REQUEST,
    CALCULATOR_SESSION_EXPIRED,
    CALCULATOR_UNDEFINED_VARIABLE
} CalculatorStatus;

typedef enum CalculatorAngleUnit
{
    CALCULATOR_ANGLE_RADIANS = 0,
    CALCULATOR_ANGLE_DEGREES
} CalculatorAngleUnit;

typedef enum CalculatorNotation
{
    CALCULATOR_NOTATION_AUTO = 0,
    CALCULATOR_NOTATION_PLAIN,
    CALCULATOR_NOTATION_SCIENTIFIC,
    CALCULATOR_NOTATION_MATHEMATICAL,
    CALCULATOR_NOTATION_FRACTION
} CalculatorNotation;

/*
------------------------------------------------------------------------------------------------------------------------------
    Evaluation policy. Division always receives explicit settings rather than
    relying on mutable global state.
------------------------------------------------------------------------------------------------------------------------------
*/

typedef struct CalculatorContext
{
    int64_t division_scale;
    int64_t output_scale;
    int64_t time_limit_ms;
    BigDecimalRoundingMode rounding;
    CalculatorNotation notation;
    CalculatorAngleUnit angle_unit;
    bool significant_division;
} CalculatorContext;

typedef enum CalculatorValueKind
{
    CALCULATOR_VALUE_DECIMAL = 0,
    CALCULATOR_VALUE_INTEGER,
    CALCULATOR_VALUE_RATIONAL
} CalculatorValueKind;

/* Owned, unformatted value. number is a decimal projection for the established
 * client path; kind identifies the authoritative value. Decimal is treated
 * conservatively as approximate. independent is a separate cache property.
 * Initialize to zero and destroy before reuse. */
typedef struct CalculatorValue
{
    BigDecimal *number;
    BigInt *integer;
    BigRational *rational;
    CalculatorValueKind kind;
    CalculatorContext context;
    bool independent;
    bool uses_answer;
    bool uses_random;
    bool uses_variables;
} CalculatorValue;

#define CALCULATOR_VARIABLE_CAPACITY 32U
#define CALCULATOR_VARIABLE_NAME_BYTES 31U
typedef struct CalculatorVariable
{
    char name[CALCULATOR_VARIABLE_NAME_BYTES + 1U];
    CalculatorValue value;
} CalculatorVariable;




#define CALCULATOR_DEFAULT_OUTPUT_SCALE 10
#define CALCULATOR_UNLIMITED_OUTPUT_SCALE (-1)
#define CALCULATOR_DEFAULT_DIVISION_SCALE 34
#define CALCULATOR_DIVISION_GUARD_DIGITS 4
#define CALCULATOR_ANGLE_GUARD_DIGITS 24
#define CALCULATOR_ANGLE_REDUCTION_GUARD_DIGITS 8
#define CALCULATOR_DEFAULT_TIME_LIMIT_MS 5000
#define CALCULATOR_FACTORIAL_MAX_N 10000
#define CALCULATOR_MAX_ROOT_DEGREE 10000
#define CALCULATOR_MAX_EXPRESSION_DEPTH 256U
#define CALCULATOR_MAX_CALL_ARGUMENTS 256U
#define CALCULATOR_MAX_OUTPUT_SCALE 10000
#define CALCULATOR_MAX_INPUT_BYTES 4096U
#define CALCULATOR_MAX_OUTPUT_BYTES 65536U
#define CALCULATOR_ALLOCATION_BUDGET (64U * 1024U * 1024U)
#define CALCULATOR_SINGLE_ALLOCATION (128U * 1024U)

/*
------------------------------------------------------------------------------------------------------------------------------
    offset is a zero-based byte position in the original UTF-8 input.
------------------------------------------------------------------------------------------------------------------------------
*/

typedef struct CalculatorError
{
    CalculatorStatus status;
    size_t offset;
} CalculatorError;

CalculatorStatus calculator_compute_value_with_variables(const char *input,
    const CalculatorContext *context, const CalculatorValue *answer,
    uint64_t *random_state, const CalculatorVariable *variables, size_t count,
    CalculatorValue *result, CalculatorError *error);

CalculatorStatus calculator_compute_value(
    const char *input,
    const CalculatorContext *context,
    CalculatorValue *result,
    CalculatorError *error
);
CalculatorStatus calculator_compute_value_with_answer(
    const char *input,
    const CalculatorContext *context,
    const CalculatorValue *answer,
    uint64_t *random_state,
    CalculatorValue *result,
    CalculatorError *error
);
void calculator_value_destroy(CalculatorValue *value);
CalculatorStatus calculator_value_copy(CalculatorValue *result, const CalculatorValue *value);
bool calculator_value_matches(const CalculatorValue *value, const CalculatorContext *context);
CalculatorStatus calculator_format_value(
    const CalculatorValue *value, const CalculatorContext *context, char **result);

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared calculator utility functions.
------------------------------------------------------------------------------------------------------------------------------
*/

const char *calculator_status_to_string( /*Human-readable description of a CalculatorStatus, for diagnostics*/
    CalculatorStatus status
);
/* Complete bounded pipeline shared by CLI and HTTP. */

CalculatorStatus calculator_compute(
    const char *input,
    const CalculatorContext *context,
    char **result,
    CalculatorError *error
);
CalculatorStatus calculator_budget_status(
    CalculatorStatus status
);
void calculator_context_init(
    CalculatorContext *context
);
CalculatorStatus calculator_context_set_output_scale(
    CalculatorContext *context,
    int64_t output_scale
);
CalculatorStatus calculator_context_set_angle_unit(
    CalculatorContext *context,
    CalculatorAngleUnit angle_unit
);
void calculator_error_clear(
    CalculatorError *error
);
void calculator_error_set(
    CalculatorError *error,
    CalculatorStatus status,
    size_t offset
);
size_t calculator_error_column( /*Convert a zero-based UTF-8 byte offset to a one-based character column*/
    const char *input,
    size_t byte_offset
);

#endif
