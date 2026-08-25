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
    BigDecimalRoundingMode rounding;
} CalculatorContext;

#define CALCULATOR_DEFAULT_OUTPUT_SCALE 10
#define CALCULATOR_UNLIMITED_OUTPUT_SCALE (-1)

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

#endif
