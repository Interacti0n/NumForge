#ifndef NUMFORGE_CALCULATOR_INTERNAL_H
#define NUMFORGE_CALCULATOR_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include <numforge/bigdecimal.h>

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared status model for the calculator pipeline. These types stay internal
    until the calculator API itself is ready to become part of NumForge's public
    library interface.
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
    CALCULATOR_NOT_IMPLEMENTED
} CalculatorStatus;

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
} CalculatorContext;

#define CALCULATOR_DEFAULT_OUTPUT_SCALE 10
#define CALCULATOR_UNLIMITED_OUTPUT_SCALE (-1)
#define CALCULATOR_DEFAULT_DIVISION_SCALE 34
#define CALCULATOR_DIVISION_GUARD_DIGITS 4
#define CALCULATOR_DEFAULT_TIME_LIMIT_MS 5000
#define CALCULATOR_FACTORIAL_MAX_N 5000
#define CALCULATOR_MAX_EXPRESSION_DEPTH 256U

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

/*
------------------------------------------------------------------------------------------------------------------------------
    Shared calculator utility functions.
------------------------------------------------------------------------------------------------------------------------------
*/
const char *calculator_status_to_string( /*Human-readable description of a CalculatorStatus, for diagnostics*/
    CalculatorStatus status
);
void calculator_context_init(
    CalculatorContext *context
);
CalculatorStatus calculator_context_set_output_scale(
    CalculatorContext *context,
    int64_t output_scale
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
